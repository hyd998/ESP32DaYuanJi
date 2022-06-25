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
#include "dynreg_api.h"
#include "infra_types.h"
#include "infra_defs.h"
#include "dev_sign_api.h"

char EXAMPLE_PRODUCT_KEY[IOTX_PRODUCT_KEY_LEN + 1] = "a1NF8b6Oz71";
char EXAMPLE_PRODUCT_SECRET[IOTX_PRODUCT_SECRET_LEN+1] = "oNBNCv86zrNYhXQj";
char EXAMPLE_DEVICE_NAME[IOTX_DEVICE_NAME_LEN +1] = "DaYuanJi002";
// char EXAMPLE_DEVICE_SECRET[IOTX_DEVICE_SECRET_LEN + 1] = "69a0432816d33620b97ec85cea28d4c3" ;

extern volatile int32_t current_rotations; //总转数
extern volatile double rpm; //转速
extern volatile bool rotation_state;//启停状态标识
extern volatile int stop_duration; //停机时长


// char EXAMPLE_PRODUCT_KEY[IOTX_PRODUCT_KEY_LEN + 1] = {0};
// char EXAMPLE_DEVICE_NAME[IOTX_DEVICE_NAME_LEN + 1] = {0};
// char EXAMPLE_DEVICE_SECRET[IOTX_DEVICE_SECRET_LEN + 1] = {0};

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

    topic_len = strlen(fmt) + strlen(EXAMPLE_PRODUCT_KEY) + strlen(EXAMPLE_DEVICE_NAME) + 1;
    topic = HAL_Malloc(topic_len);
    if (topic == NULL) {
        EXAMPLE_TRACE("memory not enough");
        return -1;
    }
    memset(topic, 0, topic_len);
    HAL_Snprintf(topic, topic_len, fmt, EXAMPLE_PRODUCT_KEY, EXAMPLE_DEVICE_NAME);

    res = IOT_MQTT_Subscribe(handle, topic, IOTX_MQTT_QOS0, example_message_arrive, NULL);
    if (res < 0) {
        EXAMPLE_TRACE("subscribe failed");
        HAL_Free(topic);
        return -1;
    }

    HAL_Free(topic);
    return 0;
}

int post_property()
{
    int             res = 0;
    const char     *fmt = "/sys/%s/%s/thing/event/property/post";
    char           *topic = NULL;
    int             topic_len = 0;
    char           *payload = NULL;

    payload = HAL_Malloc(200);
	memset(payload, 0, 200);
    sprintf(payload,"{\"params\":{\current_rotations\":%d,\"rpm\":%lf},\"method\":\"thing.event.property.post\"}",current_rotations,rpm);
                    
    topic_len = strlen(fmt) + strlen(EXAMPLE_PRODUCT_KEY) + strlen(EXAMPLE_DEVICE_NAME) + 1;
    topic = HAL_Malloc(topic_len);
    if (topic == NULL) {
        EXAMPLE_TRACE("memory not enough");
        return -1;
    }
    memset(topic, 0, topic_len);
    HAL_Snprintf(topic, topic_len, fmt, EXAMPLE_PRODUCT_KEY, EXAMPLE_DEVICE_NAME);

    res = IOT_MQTT_Publish_Simple(0, topic, IOTX_MQTT_QOS0, payload, strlen(payload));
    if (res < 0) {
        EXAMPLE_TRACE("publish failed, res = %d", res);
        HAL_Free(topic);
        return -1;
    }

    HAL_Free(topic);
    return 0;
}

int post_stop_alert()
{
    //received topic=/sys/a1QxiJtSoHS/DaYuanji001/thing/event/post_stop_alert/post_reply,
    // payload={"code":200,"data":{},"id":"1655514261049","message":"success","method":"thing.event.post_stop_alert.post","version":"1.0"}
    //"id":1655517112000,"version":"1.0","params":{},"method":"thing.event.post_stop_alert.post"
    int             res = 0;
    const char     *fmt = "/sys/%s/%s/thing/event/post_stop_alert/post";
    char           *topic = NULL;
    int             topic_len = 0;
    char           *payload = "{\"id\":1655517112001,\"version\":\"1.0\",\"params\":{},\"method\":\"thing.event.post_stop_alert.post\"}";
    
    // char           *payload = "{}";
    topic_len = strlen(fmt) + strlen(EXAMPLE_PRODUCT_KEY) + strlen(EXAMPLE_DEVICE_NAME) + 1;
    topic = HAL_Malloc(topic_len);
    if (topic == NULL) {
        EXAMPLE_TRACE("memory not enough");
        return -1;
    }
    memset(topic, 0, topic_len);
    HAL_Snprintf(topic, topic_len, fmt, EXAMPLE_PRODUCT_KEY, EXAMPLE_PRODUCT_KEY);

    res = IOT_MQTT_Publish_Simple(0, topic, IOTX_MQTT_QOS0, payload, strlen(payload));
    if (res < 0) {
        EXAMPLE_TRACE("publish failed, res = %d", res);
        HAL_Free(topic);
        return -1;
    }
    
    HAL_Free(topic);
    return 0;
}

int post_rotation()
{
    int             res = 0;
    const char     *fmt = "/sys/%s/%s/thing/event/post_rotation/post";
    char           *topic = NULL;
    int             topic_len = 0;
    char           *payload =  NULL;
    // char           *payload = "{\"id\":1655517112002,\"version\":\"1.0\",\"params\":{\"rotations\":%d,\"rpm\":%lf,\"post_rotation\":%d,\"rotation_state\":%d},\"method\":\"thing.event.post_rotation.post\"}",current_rotations,rpm,stop_duration,rotation_state;

    payload = HAL_Malloc(200);
	memset(payload, 0, 200);
    sprintf(payload,"{\"id\":1655517112001,\"version\":\"1.0\",\"params\":{\"rotations\":%d,\"stop_duration\":%d,\"rotation_state\":%d},\"method\":\"thing.event.post_rotation.post\"}",current_rotations,stop_duration,rotation_state);//这里的current_rotations之后会替换成由nvc存储中读取的历史总转速。

    topic_len = strlen(fmt) + strlen(EXAMPLE_PRODUCT_KEY) + strlen(EXAMPLE_DEVICE_NAME) + 1;
    topic = HAL_Malloc(topic_len);
    if (topic == NULL) {
        EXAMPLE_TRACE("memory not enough");
        return -1;
    }
    memset(topic, 0, topic_len);
    HAL_Snprintf(topic, topic_len, fmt, EXAMPLE_PRODUCT_KEY, EXAMPLE_DEVICE_NAME);

    res = IOT_MQTT_Publish_Simple(0, topic, IOTX_MQTT_QOS0, payload, strlen(payload));
    if (res < 0) {
        EXAMPLE_TRACE("publish failed, res = %d", res);
        HAL_Free(topic);
        return -1;
    }
    
    HAL_Free(topic);
    return 0;
}

void example_event_handle(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    EXAMPLE_TRACE("msg->event_type : %d", msg->event_type);
}

/*
 *   NOTE: About demo topic of /${productKey}/${deviceName}/user/get
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
/* Implenment this HAL or using "printf" of your own system if you want to print something in example*/

void mqtt_main(void *paras)
{
    void                   *pclient = NULL;
    int                     res = 0;
    // int                     loop_cnt = 0;
    
    iotx_http_region_types_t region = IOTX_HTTP_REGION_SHANGHAI;
    iotx_dev_meta_info_t    meta;
    iotx_sign_mqtt_t sign_mqtt;
    iotx_mqtt_param_t       mqtt_params;

    HAL_Printf("dynreg example\n");

    memset(&mqtt_params, 0x0, sizeof(mqtt_params));
    memset(&meta,0x0,sizeof(iotx_dev_meta_info_t));
    memcpy(meta.product_key,EXAMPLE_PRODUCT_KEY,strlen(EXAMPLE_PRODUCT_KEY));
    memcpy(meta.product_secret,EXAMPLE_PRODUCT_SECRET,strlen(EXAMPLE_PRODUCT_SECRET));
    memcpy(meta.device_name,EXAMPLE_DEVICE_NAME,strlen(EXAMPLE_DEVICE_NAME));
    
    res = IOT_Dynamic_Register(region, &meta);//动态注册

    if (res < 0) 
    {
    HAL_Printf("IOT_Dynamic_Register failed\n");
    return -1;
    }
    HAL_Printf("\nDevice NAME: %s\n\n", meta.device_name);
    HAL_Printf("\nDevice Secret: %s\n\n", meta.device_secret);
    HAL_Printf("\nProduct Key: %s\n\n", meta.product_key);
    HAL_Printf("\nProduct Secret: %s\n\n", meta.product_secret);

    mqtt_params.handle_event.h_fp = example_event_handle;
    pclient = IOT_MQTT_Construct(&mqtt_params); //构建mqtt连接

    if (NULL == pclient) 
        EXAMPLE_TRACE("MQTT construct failed");

    if (IOT_Sign_MQTT(region,&meta,&sign_mqtt) < 0) {
        return -1; 
    }
// memcpy(meta.device_secret,EXAMPLE_DEVICE_SECRET,strlen(EXAMPLE_DEVICE_SECRET));
//     if (IOT_Sign_MQTT(region,&meta,&sign_mqtt) < 0) {
//         return -1; 
//     }
//     #if 0   /* Uncomment this if you want to show more information */
    HAL_Printf("sign_mqtt.hostname: %s\n",sign_mqtt.hostname);
    HAL_Printf("sign_mqtt.port    : %d\n",sign_mqtt.port);
    HAL_Printf("sign_mqtt.username: %s\n",sign_mqtt.username);
    HAL_Printf("sign_mqtt.password: %s\n",sign_mqtt.password);
    HAL_Printf("sign_mqtt.clientid: %s\n",sign_mqtt.clientid);
// #endif  
//     // HAL_GetProductKey(meta.product_key);
//     // HAL_GetProductSecret(meta.product_secret);
//     // HAL_GetDeviceName(meta.device_name);

//     res = IOT_Dynamic_Register(region, &meta);
//     if (res < 0) 
//     {
//     HAL_Printf("IOT_Dynamic_Register failed\n");
//     return -1;
//     }
//     HAL_Printf("\nDevice Secret: %s\n\n", meta.device_secret);
    return 0;
}

