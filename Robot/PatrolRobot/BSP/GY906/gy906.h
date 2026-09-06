/**
 ******************************************************************************
 * @file    gy906.h
 * @brief   GY-906 红外温度传感器驱动接口
 *
 * @details
 * 主要功能：
 * 1. 定义 GY-906 驱动状态和温度数据。
 * 2. 提供设备探测、温度读取、PEC 计算和总线恢复接口。
 *
 * 模块关系：
 * PatrolRobot 应用层通过本接口读取红外温度。
 *
 * 主要接口：
 * BSP_GY906_Init()、BSP_GY906_ReadTemperatures()、BSP_GY906_RecoverBus()。
 ******************************************************************************
 */

#ifndef BSP_GY906_H
#define BSP_GY906_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

#define BSP_GY906_DEFAULT_ADDRESS_7BIT  0x5AU
#define BSP_GY906_REGISTER_AMBIENT      0x06U
#define BSP_GY906_REGISTER_OBJECT_1     0x07U

/** @brief GY-906 驱动状态。 */
typedef enum
{
  BSP_GY906_STATUS_OK = 0,
  BSP_GY906_STATUS_INVALID_ARGUMENT = -1,
  BSP_GY906_STATUS_NOT_READY = -2,
  BSP_GY906_STATUS_I2C_ERROR = -3,
  BSP_GY906_STATUS_PEC_ERROR = -4,
  BSP_GY906_STATUS_SENSOR_ERROR = -5,
  BSP_GY906_STATUS_BUS_BUSY = -6
} BSP_GY906_StatusTypeDef;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint16_t device_address;
  HAL_StatusTypeDef last_hal_status;
} BSP_GY906_HandleTypeDef;

/** @brief GY-906 温度采样结果。 */
typedef struct
{
  uint16_t raw_ambient;
  uint16_t raw_object;
  float ambient_c;
  float object_c;
} BSP_GY906_DataTypeDef;

/**
 * @brief  初始化并探测 GY-906
 * @param  sensor GY-906 驱动句柄
 * @param  i2c    传感器使用的 I2C 句柄
 * @retval GY-906 驱动状态
 */
BSP_GY906_StatusTypeDef BSP_GY906_Init(BSP_GY906_HandleTypeDef *sensor,
                                        I2C_HandleTypeDef *i2c);
/**
 * @brief  读取环境温度和目标温度
 * @param  sensor GY-906 驱动句柄
 * @param  data   温度结果输出地址
 * @retval GY-906 驱动状态
 */
BSP_GY906_StatusTypeDef BSP_GY906_ReadTemperatures(
    BSP_GY906_HandleTypeDef *sensor,
    BSP_GY906_DataTypeDef *data);
/**
 * @brief  计算 SMBus PEC-8 校验值
 * @param  data   待校验数据
 * @param  length 数据长度
 * @retval PEC-8 校验值
 */
uint8_t BSP_GY906_CalculatePEC(const uint8_t *data, uint8_t length);
/**
 * @brief  尝试恢复 I2C1 总线
 * @param  sensor GY-906 驱动句柄
 * @retval HAL 状态
 */
HAL_StatusTypeDef BSP_GY906_RecoverBus(BSP_GY906_HandleTypeDef *sensor);
#ifdef __cplusplus
}
#endif

#endif /* BSP_GY906_H */
