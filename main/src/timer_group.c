/* General Purpose Timer example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "timer_group.h"
#include "gpioconfig.h"
#include "stdio.h"
#include "math.h"

#include "mqtt_solo.h"
#include "dm_wrapper.h"
#include "mqtt_api.h"

#include "driver/pcnt.h"
#include "esp_attr.h"
#include "esp_log.h"

static const char *TAG = "pcnt";

#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

volatile int32_t current_rotations;//当前转数
volatile bool MachineState = 0; //机器状态 1启动 0停止
volatile int Timercnt; //Timercnt用于记录定时器中断的次数
volatile double rpm = 0;//每分钟转速
volatile unsigned long PulseInterval = 0; //两次脉冲直接的间隔时间
volatile int stop_duration; //停转时间 stop_duration=1 大约为1S
volatile int rotation_state = 0;//启停状态标识

/*定时器配置*/
#define TIMER0_INTERVAL_MS        1
#define DEBOUNCING_INTERVAL_MS    2000
#define LOCAL_DEBUG               1

/*移植pcnt*/
#define PCNT_H_LIM_VAL      5
#define PCNT_L_LIM_VAL     -10
#define PCNT_THRESH1_VAL    10
#define PCNT_THRESH0_VAL   -5
#define PCNT_INPUT_SIG_IO   22  // Pulse Input GPIO

typedef struct {
    int timer_group;
    int timer_idx;
    int alarm_interval;
    bool auto_reload;
} example_timer_info_t;

/**
 * @brief A sample structure to pass events from the timer ISR to task
 *
 */
typedef struct {
    example_timer_info_t info;
    uint64_t timer_counter_value;
} example_timer_event_t;
static xQueueHandle s_timer_queue;

/* A sample structure to pass events from the PCNT
 * interrupt handler to the main program.
 */
typedef struct {
    int unit;  // the PCNT unit that originated an interrupt
    uint32_t status; // information on the event type that caused the interrupt
    unsigned long timeStamp; // The time the event occured
} pcnt_evt_t;
static xQueueHandle pcnt_evt_queue;   // A queue to handle pulse counter events

/**
 * @brief 定时器中断回调函数
 *
 *
 */
static bool IRAM_ATTR timer_group_isr_callback(void *args)
{
    BaseType_t high_task_awoken = pdFALSE;
    example_timer_info_t *info = (example_timer_info_t *) args;

    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(info->timer_group, info->timer_idx);
        example_timer_event_t evt = {
        .info.timer_group = info->timer_group,
        .info.timer_idx = info->timer_idx,
        .info.auto_reload = info->auto_reload,
        .info.alarm_interval = info->alarm_interval,
        .timer_counter_value = timer_counter_value
    };
    /*每分钟转速待优化*/
        Timercnt++;
    // if(MachineState==1&&stop_duration>=10&&Timercnt>=10)
    if(MachineState==1&&stop_duration>=10&&Timercnt>=10)
    {
        stop_duration=0;
        PulseInterval=0;
        Timercnt=0;
        rotation_state=0;
        rpm=0;
    }
    if(MachineState==0)
    {
        stop_duration=0;
        rotation_state=0 ;
        Timercnt=0;
        rpm=0;
    }
    xQueueSendFromISR(s_timer_queue, &evt, &high_task_awoken);
    return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
}

/* Initialize PCNT functions:
 *  - configure and initialize PCNT
 *  - set up the input filter
 *  - set up the counter events to watch
 */
static void IRAM_ATTR pcnt_example_intr_handler(void *arg)
{
    int pcnt_unit = (int)arg;
    pcnt_evt_t pcntevt;
    pcntevt.unit = pcnt_unit;
    unsigned long currentMillis = xTaskGetTickCountFromISR();
    portBASE_TYPE HPTaskAwoken = pdFALSE;
    pcntevt.timeStamp = currentMillis;
    stop_duration=0;
    xQueueSendFromISR(pcnt_evt_queue, &pcntevt, &HPTaskAwoken);     // 队列发送
}



static void pcnt_init(int unit)
{
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = PCNT_INPUT_SIG_IO,
        .channel = PCNT_CHANNEL_0,
        .unit = unit,
        // What to do on the positive / negative edge of pulse input?
        .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
        .neg_mode = PCNT_COUNT_DIS,   // Keep the counter value on the negative edge
        // What to do when control input is low or high?
        .lctrl_mode = PCNT_MODE_REVERSE, // Reverse counting direction if low
        .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
    };
    /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);

    /* Configure and enable the input filter */
    pcnt_set_filter_value(unit, 300);//调大该值可以增加稳定性，但会错过一些信息
    pcnt_filter_enable(unit);

    /* Set threshold 0 and 1 values and enable events to watch */
    pcnt_set_event_value(unit, PCNT_EVT_THRES_1, PCNT_THRESH1_VAL);
    pcnt_event_enable(unit, PCNT_EVT_THRES_1);
    pcnt_set_event_value(unit, PCNT_EVT_THRES_0, PCNT_THRESH0_VAL);
    pcnt_event_enable(unit, PCNT_EVT_ZERO);
    pcnt_event_enable(unit, PCNT_EVT_H_LIM);

    /* Initialize PCNT's counter */
    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);

    /* Install interrupt service and add isr callback handler */
    pcnt_isr_service_install(0);
    pcnt_isr_handler_add(unit, pcnt_example_intr_handler, (void *)unit);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(unit);
}

/**
 * @brief Initialize selected timer of timer group
 *
 * @param group Timer Group number, index from 0
 * @param timer timer ID, index from 0
 * @param auto_reload whether auto-reload on alarm event
 * @param timer_interval_sec interval of alarm
 */
static void tg_timer_init(int group, int timer, bool auto_reload, int timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = auto_reload,
    }; // default clock source is APB
    timer_init(group, timer, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(group, timer, 0);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(group, timer, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(group, timer);

    example_timer_info_t *timer_info = calloc(1, sizeof(example_timer_info_t));
    timer_info->timer_group = group;
    timer_info->timer_idx = timer;
    timer_info->auto_reload = auto_reload;
    timer_info->alarm_interval = timer_interval_sec;
    timer_isr_callback_add(group, timer, timer_group_isr_callback, timer_info, 0);
    timer_start(group, timer);
}


void timer_main(void)
{

    volatile unsigned long firstTime,lastTime;
    s_timer_queue = xQueueCreate(10, sizeof(example_timer_event_t));
    pcnt_evt_queue = xQueueCreate(10, sizeof(pcnt_evt_t));
    int pcnt_unit = PCNT_UNIT_0;
    tg_timer_init(TIMER_GROUP_0, TIMER_0, true, 1);
    pcnt_init(pcnt_unit);
    pcnt_evt_t pcntevt;
    portBASE_TYPE res;
    while (1)
    {
        example_timer_event_t evt;
        xQueueReceive(s_timer_queue, &evt, portMAX_DELAY);
        res = xQueueReceive(pcnt_evt_queue, &pcntevt, 1000 / portTICK_PERIOD_MS);
        if(res == pdTRUE)
        {
            pcnt_get_counter_value(pcnt_unit, &current_rotations);
            ESP_LOGI(TAG, "Event PCNT plusetime is :%ld; uint is %d; cnt: %d", pcntevt.timeStamp,pcntevt.unit, current_rotations);
            // firstTime=pcntevt.unit
            lastTime = pcntevt.timeStamp;
        }
        else
        {
            pcnt_get_counter_value(pcnt_unit, &current_rotations);
            ESP_LOGI(TAG, "Current counter value :%d", current_rotations);
            // firstTime=pcntevt.timeStamp;
            firstTime = lastTime; // 上一次上升沿的时间保存到第一次
            lastTime = pcntevt.timeStamp; // 当前的保存到这一次，两个差值就是中间的时间
        }

        if (lastTime != firstTime)
            PulseInterval = lastTime - firstTime;
        else
            stop_duration++;  //记录没进pcnt中断，累积的次数。
            
        if(PulseInterval!=0)//过滤间隔低于1秒的脉冲。
            rpm = round(60/(PulseInterval/100));

        if (PulseInterval>0 && PulseInterval<900) //600为6秒
            {
            MachineState = 1;// 开启
            rotation_state++;
            }
        else
            MachineState = 0;// 停机

        if(rotation_state == 1 && rpm> 5) //旋转时刻
        {
            post_rotation();
            printf("旋转时刻 \n");
        }
        if(MachineState==1&&stop_duration>=10&&Timercnt>=10)
        {
            post_stop_alert();
            printf("停转时刻 \n");
        }
        printf("getPulseinterval is %ld;stop_duration is %d;Timercnt is %d;\n",PulseInterval,stop_duration,Timercnt);
        printf("MachineState is %d \n",MachineState);
        printf("rpm is %lf \n",rpm);
        printf("rotation_state is %d \n",rotation_state);
    }
}