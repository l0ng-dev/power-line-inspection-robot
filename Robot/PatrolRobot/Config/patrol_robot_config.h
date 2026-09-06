/**
 ******************************************************************************
 * @file    patrol_robot_config.h
 * @brief   巡检机器人应用层调度与容量配置
 ******************************************************************************
 */
#ifndef PATROL_ROBOT_CONFIG_H
#define PATROL_ROBOT_CONFIG_H

#define GY906_SAMPLE_INTERVAL_MS             500U
#define GY906_POWER_ON_DELAY_MS               250U
#define GY906_MAX_BUS_RECOVERIES                3U
#define ULTRASONIC_MEASUREMENT_INTERVAL_MS    100U
#define BLUETOOTH_STATUS_INTERVAL_MS         5000U
#define BLUETOOTH_LINE_INTERVAL_MS             80U
#define BLUETOOTH_NOTIFY_PAYLOAD_SIZE          20U
#define BLUETOOTH_TX_BUFFER_SIZE               32U
#define K230_MQTT_EVENT_QUEUE_DEPTH             4U

#endif /* PATROL_ROBOT_CONFIG_H */
