/**
 ******************************************************************************
 * @file    ultrasonic.h
 * @brief   超声波测距驱动接口
 *
 * @details
 * 主要功能：
 * 1. 定义测距状态、有效距离阈值和测量数据。
 * 2. 提供触发测量、超时处理和输入捕获回调接口。
 *
 * 模块关系：
 * PatrolRobot 应用层通过本接口获取超声波距离。
 *
 * 主要接口：
 * BSP_Ultrasonic_Init()、BSP_Ultrasonic_StartMeasurement()、
 * BSP_Ultrasonic_GetData()。
 ******************************************************************************
 */

#ifndef BSP_ULTRASONIC_H
#define BSP_ULTRASONIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

#define BSP_ULTRASONIC_TRIGGER_PULSE_US  10U
#define BSP_ULTRASONIC_ECHO_TIMEOUT_MS   40U
#define ULTRASONIC_MIN_VALID_DISTANCE_MM  50U
#define ULTRASONIC_RECOVERY_DISTANCE_MM   60U

#if ULTRASONIC_RECOVERY_DISTANCE_MM <= ULTRASONIC_MIN_VALID_DISTANCE_MM
#error "Ultrasonic recovery distance must exceed the minimum valid distance"
#endif

/** @brief 超声波测距状态。 */
typedef enum
{
  BSP_ULTRASONIC_STATUS_TOO_CLOSE = -4,
  BSP_ULTRASONIC_STATUS_NOT_INITIALIZED = -3,
  BSP_ULTRASONIC_STATUS_ERROR = -2,
  BSP_ULTRASONIC_STATUS_TIMEOUT = -1,
  BSP_ULTRASONIC_STATUS_IDLE = 0,
  BSP_ULTRASONIC_STATUS_WAIT_RISING = 1,
  BSP_ULTRASONIC_STATUS_WAIT_FALLING = 2,
  BSP_ULTRASONIC_STATUS_VALID = 3
} BSP_Ultrasonic_StatusTypeDef;

typedef struct
{
  TIM_HandleTypeDef *timer;
  volatile BSP_Ultrasonic_StatusTypeDef status;
  volatile uint32_t rise_capture_us;
  volatile uint32_t distance_mm;
  volatile uint8_t sample_valid;
  volatile uint8_t too_close_active;
  uint32_t measurement_started_ms;
} BSP_Ultrasonic_HandleTypeDef;

/** @brief 超声波测量数据快照。 */
typedef struct
{
  BSP_Ultrasonic_StatusTypeDef status;
  uint32_t distance_mm;
  uint8_t sample_valid;
} BSP_Ultrasonic_DataTypeDef;

/**
 * @brief  初始化超声波驱动
 * @param  sensor 超声波驱动句柄
 * @param  timer  TIM2 句柄
 * @retval HAL 状态
 */
HAL_StatusTypeDef BSP_Ultrasonic_Init(BSP_Ultrasonic_HandleTypeDef *sensor,
                                      TIM_HandleTypeDef *timer);
/**
 * @brief  发起一次测距
 * @param  sensor 超声波驱动句柄
 * @retval HAL 状态
 */
HAL_StatusTypeDef BSP_Ultrasonic_StartMeasurement(
    BSP_Ultrasonic_HandleTypeDef *sensor);
/**
 * @brief  处理无回波超时
 * @param  sensor 超声波驱动句柄
 * @param  now_ms 当前系统毫秒节拍
 */
void BSP_Ultrasonic_Process(BSP_Ultrasonic_HandleTypeDef *sensor,
                            uint32_t now_ms);
/**
 * @brief  获取测量数据快照
 * @param  sensor 超声波驱动句柄
 * @param  data   测量结果输出地址
 * @retval HAL 状态
 */
HAL_StatusTypeDef BSP_Ultrasonic_GetData(
    const BSP_Ultrasonic_HandleTypeDef *sensor,
    BSP_Ultrasonic_DataTypeDef *data);
/**
 * @brief  处理 HAL 输入捕获事件
 * @param  sensor 超声波驱动句柄
 * @param  timer  触发回调的定时器句柄
 */
void BSP_Ultrasonic_IC_CaptureCallback(
    BSP_Ultrasonic_HandleTypeDef *sensor,
    TIM_HandleTypeDef *timer);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ULTRASONIC_H */
