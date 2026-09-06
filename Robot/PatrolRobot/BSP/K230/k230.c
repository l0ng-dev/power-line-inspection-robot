/**
 ******************************************************************************
 * @file    k230.c
 * @brief   基于 UART4 中断接收的 K230 视觉告警驱动
 *
 * @details
 * 中断回调只完成逐字节组帧，ALERT,<type>,<id> 的解析在主循环完成。
 * 四行队列接住同一画面的多类别告警；超长行和队列满时安全丢弃整行。
 ******************************************************************************
 */

#include "k230.h"
#include <stddef.h>
#include <string.h>

#define BSP_K230_RX_QUEUE_DEPTH  4U

static UART_HandleTypeDef *k230_uart = NULL;
static uint8_t k230_rx_byte = 0U;
static volatile uint8_t k230_rx_line[BSP_K230_RX_QUEUE_DEPTH][BSP_K230_RX_LINE_SIZE];
static volatile uint16_t k230_rx_index = 0U;
static volatile uint16_t k230_ready_length[BSP_K230_RX_QUEUE_DEPTH];
static volatile uint8_t k230_read_slot = 0U;
static volatile uint8_t k230_write_slot = 0U;
static volatile uint8_t k230_queued_lines = 0U;
/* 0: 正常组行；1: 超长行；2: 队列满；3: UART 错误后的残行。 */
static volatile uint8_t k230_discarding_line = 0U;

static uint8_t BSP_K230_TakeLine(char *line, uint16_t capacity);
static uint8_t BSP_K230_ParseAlert(const char *line,
                                   BSP_K230_AlertTypeDef *alert);

HAL_StatusTypeDef BSP_K230_Init(UART_HandleTypeDef *uart)
{
  if (uart == NULL)
  {
    return HAL_ERROR;
  }

  k230_uart = uart;
  k230_rx_index = 0U;
  k230_read_slot = 0U;
  k230_write_slot = 0U;
  k230_queued_lines = 0U;
  k230_discarding_line = 0U;
  return HAL_UART_Receive_IT(k230_uart, &k230_rx_byte, 1U);
}

uint8_t BSP_K230_TakeAlert(BSP_K230_AlertTypeDef *alert)
{
  char line[BSP_K230_RX_LINE_SIZE];

  if (alert == NULL)
  {
    return 0U;
  }
  if (BSP_K230_TakeLine(line, sizeof(line)) == 0U)
  {
    return 0U;
  }
  if (BSP_K230_ParseAlert(line, alert) == 0U)
  {
    return 0U;
  }
  return 1U;
}

void BSP_K230_RxCpltCallback(UART_HandleTypeDef *uart)
{
  uint8_t received;

  if ((k230_uart == NULL) || (uart != k230_uart))
  {
    return;
  }

  received = k230_rx_byte;
  if (received == (uint8_t)'\n')
  {
    if (k230_discarding_line == 1U)
    {
    }
    else if (k230_discarding_line == 2U)
    {
    }
    else if (k230_discarding_line == 3U)
    {
      /* UART 错误后的残行到此结束。 */
    }
    else if (k230_rx_index > 0U)
    {
      k230_ready_length[k230_write_slot] = k230_rx_index;
      k230_write_slot = (uint8_t)((k230_write_slot + 1U) % BSP_K230_RX_QUEUE_DEPTH);
      k230_queued_lines++;
    }
    k230_rx_index = 0U;
    k230_discarding_line = 0U;
  }
  else if (received != (uint8_t)'\r')
  {
    if (k230_discarding_line == 0U)
    {
      if (k230_queued_lines >= BSP_K230_RX_QUEUE_DEPTH)
      {
        /* 一旦丢弃行头，即使主循环随后腾出空间，也丢弃至换行。 */
        k230_discarding_line = 2U;
      }
      else if (k230_rx_index < (BSP_K230_RX_LINE_SIZE - 1U))
      {
        k230_rx_line[k230_write_slot][k230_rx_index] = received;
        k230_rx_index++;
      }
      else
      {
        k230_rx_index = 0U;
        k230_discarding_line = 1U;
      }
    }
  }

  (void)HAL_UART_Receive_IT(k230_uart, &k230_rx_byte, 1U);
}

void BSP_K230_ErrorCallback(UART_HandleTypeDef *uart)
{
  uint32_t error;

  if ((k230_uart == NULL) || (uart != k230_uart))
  {
    return;
  }

  error = HAL_UART_GetError(uart);
  /* 帧、噪声或奇偶校验错误会破坏当前文本行，丢弃到下一个换行再恢复组帧。 */
  if ((error & (HAL_UART_ERROR_FE | HAL_UART_ERROR_NE | HAL_UART_ERROR_PE)) != 0U)
  {
    k230_rx_index = 0U;
    k230_discarding_line = 3U;
  }

  /* ORE 等阻塞错误会由 HAL 结束接收，此时重新挂载单字节接收。 */
  if (uart->RxState == HAL_UART_STATE_READY)
  {
    (void)HAL_UART_Receive_IT(k230_uart, &k230_rx_byte, 1U);
  }
}

static uint8_t BSP_K230_TakeLine(char *line, uint16_t capacity)
{
  uint32_t primask;
  uint16_t index;
  uint16_t length;

  if ((line == NULL) || (capacity == 0U))
  {
    return 0U;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (k230_queued_lines == 0U)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return 0U;
  }

  length = k230_ready_length[k230_read_slot];
  if (length >= capacity)
  {
    length = capacity - 1U;
  }
  for (index = 0U; index < length; index++)
  {
    line[index] = (char)k230_rx_line[k230_read_slot][index];
  }
  line[length] = '\0';
  k230_read_slot = (uint8_t)((k230_read_slot + 1U) % BSP_K230_RX_QUEUE_DEPTH);
  k230_queued_lines--;
  if (primask == 0U)
  {
    __enable_irq();
  }
  return 1U;
}

static uint8_t BSP_K230_ParseAlert(const char *line,
                                   BSP_K230_AlertTypeDef *alert)
{
  static const char prefix[] = "ALERT,";
  const char *cursor = line;
  uint16_t index;

  for (index = 0U; index < (sizeof(prefix) - 1U); index++)
  {
    if (cursor[index] != prefix[index])
    {
      return 0U;
    }
  }
  cursor += sizeof(prefix) - 1U;

  index = 0U;
  while ((*cursor != '\0') && (*cursor != ','))
  {
    if (index >= (BSP_K230_TYPE_SIZE - 1U))
    {
      return 0U;
    }
    alert->type[index++] = *cursor++;
  }
  alert->type[index] = '\0';
  if ((index == 0U) || (*cursor != ','))
  {
    return 0U;
  }
  cursor++;

  index = 0U;
  while (*cursor != '\0')
  {
    if ((*cursor == ',') || (index >= (BSP_K230_DEVICE_ID_SIZE - 1U)))
    {
      return 0U;
    }
    alert->device_id[index++] = *cursor++;
  }
  alert->device_id[index] = '\0';
  if (index == 0U)
  {
    return 0U;
  }

  /* 本轮协议只接受三个已定义类别和固定设备编号。 */
  if ((strcmp(alert->type, "break") != 0) &&
      (strcmp(alert->type, "heat") != 0) &&
      (strcmp(alert->type, "wear") != 0))
  {
    return 0U;
  }
  return (strcmp(alert->device_id, "K230_01") == 0) ? 1U : 0U;
}
