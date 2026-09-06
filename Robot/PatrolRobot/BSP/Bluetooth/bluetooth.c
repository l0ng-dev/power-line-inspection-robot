/**
 ******************************************************************************
 * @file    bluetooth.c
 * @brief   基于 USART3 中断接收的蓝牙文本通信
 *
 * @details
 * 主要功能：
 * 1. 提供带超时的文本发送接口。
 * 2. 将接收字节按换行符组装为命令，并处理超长行和串口错误。
 *
 * 模块关系：
 * PatrolRobot 应用层调用本模块，本模块使用 CubeMX 生成的 USART3 句柄。
 *
 * 主要接口：
 * BSP_Bluetooth_Init()、BSP_Bluetooth_Transmit()、BSP_Bluetooth_TakeLine()。
 *
 * @note
 * 接收回调只完成组帧和状态更新，命令解析在应用层主循环中执行。
 ******************************************************************************
 */

#include "bluetooth.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

#define BSP_BLUETOOTH_TX_TIMEOUT_MS  200U

static UART_HandleTypeDef *bluetooth_uart = NULL;
static uint8_t bluetooth_rx_byte = 0U;
static volatile uint8_t bluetooth_rx_line[BSP_BLUETOOTH_RX_LINE_SIZE];
static volatile uint16_t bluetooth_rx_index = 0U;
static volatile uint16_t bluetooth_ready_length = 0U;
static volatile uint8_t bluetooth_line_ready = 0U;
static volatile uint8_t bluetooth_discarding_line = 0U;
static volatile uint8_t bluetooth_overflow_event = 0U;

/** @brief 初始化蓝牙串口并启动单字节中断接收 */
HAL_StatusTypeDef BSP_Bluetooth_Init(UART_HandleTypeDef *uart)
{
  if (uart == NULL)
  {
    return HAL_ERROR;
  }

  bluetooth_uart = uart;
  bluetooth_rx_index = 0U;
  bluetooth_ready_length = 0U;
  bluetooth_line_ready = 0U;
  bluetooth_discarding_line = 0U;
  bluetooth_overflow_event = 0U;
  return HAL_UART_Receive_IT(bluetooth_uart, &bluetooth_rx_byte, 1U);
}

/** @brief 通过蓝牙串口发送一条文本消息 */
HAL_StatusTypeDef BSP_Bluetooth_Transmit(const char *message)
{
  size_t length;

  if ((bluetooth_uart == NULL) || (message == NULL))
  {
    return HAL_ERROR;
  }

  length = strlen(message);
  if ((length == 0U) || (length > UINT16_MAX))
  {
    return HAL_ERROR;
  }

  return HAL_UART_Transmit(bluetooth_uart,
                           (const uint8_t *)message,
                           (uint16_t)length,
                           BSP_BLUETOOTH_TX_TIMEOUT_MS);
}

/** @brief 取出一条中断侧已组装完成的命令 */
uint8_t BSP_Bluetooth_TakeLine(char *line, uint16_t capacity)
{
  uint32_t primask;
  uint16_t index;
  uint16_t length;

  if ((line == NULL) || (capacity == 0U))
  {
    return 0U;
  }

  /*
   * 行内容和就绪标志由接收中断更新，复制期间关闭中断，
   * 防止应用层读到一半旧数据和一半新数据。
   */
  primask = __get_PRIMASK();
  __disable_irq();

  if (bluetooth_line_ready == 0U)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return 0U;
  }

  length = bluetooth_ready_length;
  if (length >= capacity)
  {
    length = capacity - 1U;
  }
  for (index = 0U; index < length; index++)
  {
    line[index] = (char)bluetooth_rx_line[index];
  }
  line[length] = '\0';
  bluetooth_ready_length = 0U;
  bluetooth_line_ready = 0U;

  if (primask == 0U)
  {
    __enable_irq();
  }

  return 1U;
}

/** @brief 读取并清除一次接收溢出事件 */
uint8_t BSP_Bluetooth_TakeOverflowEvent(void)
{
  uint32_t primask;
  uint8_t overflow_event;

  primask = __get_PRIMASK();
  __disable_irq();
  overflow_event = bluetooth_overflow_event;
  bluetooth_overflow_event = 0U;
  if (primask == 0U)
  {
    __enable_irq();
  }

  return overflow_event;
}

/** @brief 处理一个接收字节并继续挂载下一次中断接收 */
void BSP_Bluetooth_RxCpltCallback(UART_HandleTypeDef *uart)
{
  uint8_t received;

  if ((bluetooth_uart == NULL) || (uart != bluetooth_uart))
  {
    return;
  }

  received = bluetooth_rx_byte;
  /*
   * 换行符发布完整命令，回车符忽略。缓冲区溢出后整行丢弃，
   * 直到下一次换行才恢复，避免残缺命令进入应用层。
   */
  if (received == (uint8_t)'\n')
  {
    if (bluetooth_discarding_line != 0U)
    {
      bluetooth_discarding_line = 0U;
      bluetooth_rx_index = 0U;
      bluetooth_overflow_event = 1U;
    }
    else if (bluetooth_line_ready != 0U)
    {
      /* 上一条命令尚未处理时丢弃新行。 */
    }
    else if (bluetooth_rx_index > 0U)
    {
      bluetooth_ready_length = bluetooth_rx_index;
      bluetooth_rx_index = 0U;
      bluetooth_line_ready = 1U;
    }
  }
  else if (received != (uint8_t)'\r')
  {
    if ((bluetooth_line_ready == 0U) &&
        (bluetooth_discarding_line == 0U))
    {
      if (bluetooth_rx_index < (BSP_BLUETOOTH_RX_LINE_SIZE - 1U))
      {
        bluetooth_rx_line[bluetooth_rx_index] = received;
        bluetooth_rx_index++;
      }
      else
      {
        bluetooth_rx_index = 0U;
        bluetooth_discarding_line = 1U;
      }
    }
  }

  (void)HAL_UART_Receive_IT(bluetooth_uart, &bluetooth_rx_byte, 1U);
}

/** @brief 记录串口错误并尝试恢复中断接收 */
void BSP_Bluetooth_ErrorCallback(UART_HandleTypeDef *uart)
{
  if ((bluetooth_uart == NULL) || (uart != bluetooth_uart))
  {
    return;
  }

  if (uart->RxState == HAL_UART_STATE_READY)
  {
    (void)HAL_UART_Receive_IT(bluetooth_uart, &bluetooth_rx_byte, 1U);
  }
}
