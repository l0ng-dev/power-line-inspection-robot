/**
 ******************************************************************************
 * @file    k230.h
 * @brief   K230 视觉告警串口接收驱动接口
 *
 * @details
 * 通过 UART4 接收以换行结束的 ALERT,<type>,<id> 文本报文，
 * 并在主循环中解析为视觉告警事件。
 ******************************************************************************
 */

#ifndef BSP_K230_H
#define BSP_K230_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

#define BSP_K230_RX_LINE_SIZE    96U
#define BSP_K230_TYPE_SIZE       32U
#define BSP_K230_DEVICE_ID_SIZE  32U

typedef struct
{
  char type[BSP_K230_TYPE_SIZE];
  char device_id[BSP_K230_DEVICE_ID_SIZE];
} BSP_K230_AlertTypeDef;

HAL_StatusTypeDef BSP_K230_Init(UART_HandleTypeDef *uart);
uint8_t BSP_K230_TakeAlert(BSP_K230_AlertTypeDef *alert);
void BSP_K230_RxCpltCallback(UART_HandleTypeDef *uart);
void BSP_K230_ErrorCallback(UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif

#endif /* BSP_K230_H */
