
/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mqtt_api.h"
#include "dm_wrapper.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpioconfig.h"

extern volatile int debounceCounter; //总转速
#define GPIO_OUTPUT_IO_0    22
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)  // 配置GPIO_OUT位寄存器
#define GPIO_INPUT_IO_1    23
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_1)  // 配置GPIO_IN位寄存器


char DEMO_PRODUCT_KEY[IOTX_PRODUCT_KEY_LEN + 1] = {0};
char DEMO_DEVICE_NAME[IOTX_DEVICE_NAME_LEN + 1] = {0};
char DEMO_DEVICE_SECRET[IOTX_DEVICE_SECRET_LEN + 1] = {0};


// /*GPIO初始化函数*/
// void gpio1_init(void)
// {
//     gpio_config_t io_conf;  // 定义一个gpio_config类型的结构体，下面的都算对其进行的配置

//     io_conf.intr_type = GPIO_PIN_INTR_DISABLE;  // 禁止中断  
//     io_conf.mode = GPIO_MODE_INPUT_OUTPUT;            // 选择输出模式
//     io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL; // 配置GPIO_OUT寄存器
//     io_conf.pull_down_en = 0;                   // 禁止下拉
//     io_conf.pull_up_en = 0;                     // 禁止上拉
    
//     gpio_config(&io_conf);                      // 最后配置使能
// }

// uint8_t get_rotation(void)
// {
//     uint8_t cnt=0;
//     gpio_pad_select_gpio(GPIO_NUM_23);
//     gpio_set_direction(GPIO_NUM_23, GPIO_MODE_INPUT);

//         if(gpio_get_level(GPIO_NUM_23)==0)
//         {
//             cnt++;
//         }
//          printf("total_rotation is : %d\n",cnt);

//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//         return cnt;
    

// }

// int get_total_rotation()
// {
//     int total_ratation;
//     total_ratation=get_ratation();
//     printf("total_ratation is : %d\n",total_ratation);
//     vTaskDelay(1000 / portTICK_PERIOD_MS);
//     return total_ratation;
// }

/*GPIO初始化函数*/

#define EXAMPLE_TRACE(fmt, ...)  \
    do { \
        HAL_Printf("%s|%03d :: ", __func__, __LINE__); \
        HAL_Printf(fmt, ##__VA_ARGS__); \
        HAL_Printf("%s", "\r\n"); \
    } while(0)

void example_message_arrive(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    iotx_mqtt_topic_info_t     *topic_info = (iotx_mqtt_topic_info_pt) msg->msg;

    switch (msg->event_type) {
        case IOTX_MQTT_EVENT_PUBLISH_RECEIVED:
            /* print topic name and topic message */
            EXAMPLE_TRACE("Message Arrived:");
            EXAMPLE_TRACE("Topic  : %.*s", topic_info->topic_len, topic_info->ptopic);
            EXAMPLE_TRACE("Payload: %.*s", topic_info->payload_len, topic_info->payload);
            EXAMPLE_TRACE("\n");
            break;
        default:
            break;
    }
}

int example_subscribe(void *handle)
{
    int res = 0;
    const char *fmt = "/%s/%s/user/get";
    char *topic = NULL;
    int topic_len = 0;

    topic_len = strlen(fmt) + strlen(DEMO_PRODUCT_KEY) + strlen(DEMO_DEVICE_NAME) + 1;
    topic = HAL_Malloc(topic_len);
    if (topic == NULL) {
        EXAMPLE_TRACE("memory not enough");
        return -1;
    }
    memset(topic, 0, topic_len);
    HAL_Snprintf(topic, topic_len, fmt, DEMO_PRODUCT_KEY, DEMO_DEVICE_NAME);

    res = IOT_MQTT_Subscribe(handle, topic, IOTX_MQTT_QOS0, example_message_arrive, NULL);
    if (res < 0) {
        EXAMPLE_TRACE("subscribe failed");
        HAL_Free(topic);
        return -1;
    }

    HAL_Free(topic);
    return 0;
}

int example_publish(void *handle)
{
    int             res = 0;
    const char     *fmt = "/sys/%s/%s/thing/event/property/post";
    char           *topic = NULL;
    int             topic_len = 0;
    // char           *payload = "{\"message\":\"hello!\"}";
    char           *payload = NULL;

    payload = HAL_Malloc(200);
	memset(payload, 0, 200);
    sprintf(payload,"{\"params\":{\"total_rotation\":%d},\"method\":\"thing.event.property.post\"}",debounceCounter);//这里的CurrentVoltage就是之前添加标准功能的key

    topic_len = strlen(fmt) + strlen(DEMO_PRODUCT_KEY) + strlen(DEMO_DEVICE_NAME) + 1;
    topic = HAL_Malloc(topic_len);
    if (topic == NULL) {
        EXAMPLE_TRACE("memory not enough");
        return -1;
    }
    memset(topic, 0, topic_len);
    HAL_Snprintf(topic, topic_len, fmt, DEMO_PRODUCT_KEY, DEMO_DEVICE_NAME);

    res = IOT_MQTT_Publish_Simple(0, topic, IOTX_MQTT_QOS0, payload, strlen(payload));
    if (res < 0) {
        EXAMPLE_TRACE("publish failed, res = %d", res);
        HAL_Free(topic);
        return -1;
    }

    HAL_Free(topic);
    return 0;
}

// int total_ratation_publish(void *handle)
// {
//     int             res = 0;
//     const char     *fmt = "/%s/%s/thing/event/property/post";
//     char           *topic = NULL;
//     int             topic_len = 0;
//     char           *payload = "{ "properties": [
//                             "identifier":    total_rotation 
//                                 ]
//                                 }";
//     topic_len = strlen(fmt) + strlen(DEMO_PRODUCT_KEY) + strlen(DEMO_DEVICE_NAME) + 1;
//     topic = HAL_Malloc(topic_len);
//     if (topic == NULL) {
//         EXAMPLE_TRACE("memory not enough");
//         return -1;
//     }
//     memset(topic, 0, topic_len);
//     HAL_Snprintf(topic, topic_len, fmt, DEMO_PRODUCT_KEY, DEMO_DEVICE_NAME);

//     res = IOT_MQTT_Publish_Simple(0, topic, IOTX_MQTT_QOS0, payload, strlen(payload));
//     if (res < 0) {
//         EXAMPLE_TRACE("publish failed, res = %d", res);
//         HAL_Free(topic);
//         return -1;
//     }

//     HAL_Free(topic);
//     return 0;
// }

void example_event_handle(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    EXAMPLE_TRACE("msg->event_type : %d", msg->event_type);
}

/*
 *  NOTE: About demo topic of /${productKey}/${deviceName}/user/get
 *
 *  The demo device has been configured in IoT console (https://iot.console.aliyun.com)
 *  so that its /${productKey}/${deviceName}/user/get can both be subscribed and published
 *
 *  We design this to completely demonstrate publish & subscribe process, in this way
 *  MQTT client can receive original packet sent by itself
 *
 *  For new devices created by yourself, pub/sub privilege also requires being granted
 *  to its /${productKey}/${deviceName}/user/get for successfully running whole example
 */

int mqtt_main(void *paras)
{
    void                   *pclient = NULL;
    int                     res = 0;
    int                     loop_cnt = 0;
    iotx_mqtt_param_t       mqtt_params;

    HAL_GetProductKey(DEMO_PRODUCT_KEY);
    HAL_GetDeviceName(DEMO_DEVICE_NAME);
    HAL_GetDeviceSecret(DEMO_DEVICE_SECRET);

    EXAMPLE_TRACE("mqtt example");

    /* Initialize MQTT parameter */
    /*
     * Note:
     *
     * If you did NOT set value for members of mqtt_params, SDK will use their default values
     * If you wish to customize some parameter, just un-comment value assigning expressions below
     *
     **/
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    /**
     *
     *  MQTT connect hostname string
     *
     *  MQTT server's hostname can be customized here
     *
     *  default value is ${productKey}.iot-as-mqtt.cn-shanghai.aliyuncs.com
     */
    /* mqtt_params.host = "something.iot-as-mqtt.cn-shanghai.aliyuncs.com"; */

    /**
     *
     *  MQTT connect port number
     *
     *  TCP/TLS port which can be 443 or 1883 or 80 or etc, you can customize it here
     *
     *  default value is 1883 in TCP case, and 443 in TLS case
     */
    /* mqtt_params.port = 1883; */

    /**
     *
     * MQTT request timeout interval
     *
     * MQTT message request timeout for waiting ACK in MQTT Protocol
     *
     * default value is 2000ms.
     */
    /* mqtt_params.request_timeout_ms = 2000; */

    /**
     *
     * MQTT clean session flag
     *
     * If CleanSession is set to 0, the Server MUST resume communications with the Client based on state from
     * the current Session (as identified by the Client identifier).
     *
     * If CleanSession is set to 1, the Client and Server MUST discard any previous Session and Start a new one.
     *
     * default value is 0.
     */
    /* mqtt_params.clean_session = 0; */

    /**
     *
     * MQTT keepAlive interval
     *
     * KeepAlive is the maximum time interval that is permitted to elapse between the point at which
     * the Client finishes transmitting one Control Packet and the point it starts sending the next.
     *
     * default value is 60000.
     */
    /* mqtt_params.keepalive_interval_ms = 60000; */

    /**
     *
     * MQTT write buffer size
     *
     * Write buffer is allocated to place upstream MQTT messages, MQTT client will be limitted
     * to send packet no longer than this to Cloud
     *
     * default value is 1024.
     *
     */
    /* mqtt_params.write_buf_size = 1024; */

    /**
     *
     * MQTT read buffer size
     *
     * Write buffer is allocated to place downstream MQTT messages, MQTT client will be limitted
     * to recv packet no longer than this from Cloud
     *
     * default value is 1024.
     *
     */
    /* mqtt_params.read_buf_size = 1024; */

    /**
     *
     * MQTT event callback function
     *
     * Event callback function will be called by SDK when it want to notify user what is happening inside itself
     *
     * default value is NULL, which means PUB/SUB event won't be exposed.
     *
     */
    mqtt_params.handle_event.h_fp = example_event_handle;

    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        EXAMPLE_TRACE("MQTT construct failed");
        return -1;
    }

    res = example_subscribe(pclient);
    if (res < 0) {
        IOT_MQTT_Destroy(&pclient);
        return -1;
    }

    while (1) {
        if (0 == loop_cnt % 20) {
            example_publish(pclient);
            // total_ratation_publish(pclient);
        }

        IOT_MQTT_Yield(pclient, 200);

        loop_cnt += 1;
    }

    return 0;
}
