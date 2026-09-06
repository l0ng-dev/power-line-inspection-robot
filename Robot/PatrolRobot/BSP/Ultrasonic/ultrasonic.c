/**
 ******************************************************************************
 * @file    ultrasonic.c
 * @brief   基于 TIM2 输入捕获的超声波测距驱动
 *
 * @details
 * 主要功能：
 * 1. 产生 10 微秒触发脉冲，并通过双边沿捕获计算回波宽度。
 * 2. 处理无回波超时以及 50/60 毫米近距滞回。
 *
 * 模块关系：
 * PatrolRobot 应用层调用本模块，本模块使用 CubeMX 生成的 TIM2 和 GPIO。
 *
 * 主要接口：
 * BSP_Ultrasonic_Init()、BSP_Ultrasonic_StartMeasurement()、
 * BSP_Ultrasonic_GetData()。
 ******************************************************************************
 */

#include "ultrasonic.h"
#include "main.h"

#define BSP_ULTRASONIC_MAX_ECHO_US  \
  (BSP_ULTRASONIC_ECHO_TIMEOUT_MS * 1000U)

static uint32_t BSP_Ultrasonic_EnterCritical(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  return primask;
}

static void BSP_Ultrasonic_ExitCritical(uint32_t primask)
{
  if (primask == 0U)
  {
    __enable_irq();
  }
}

/** @brief 初始化超声波驱动并启动 TIM2 输入捕获 */
HAL_StatusTypeDef BSP_Ultrasonic_Init(BSP_Ultrasonic_HandleTypeDef *sensor,
                                      TIM_HandleTypeDef *timer)
{
  HAL_StatusTypeDef hal_status;

  if ((sensor == NULL) || (timer == NULL) || (timer->Instance != TIM2))
  {
    return HAL_ERROR;
  }

  sensor->timer = timer;
  sensor->status = BSP_ULTRASONIC_STATUS_NOT_INITIALIZED;
  sensor->rise_capture_us = 0U;
  sensor->distance_mm = 0U;
  sensor->sample_valid = 0U;
  sensor->too_close_active = 0U;
  sensor->measurement_started_ms = 0U;

  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port,
                    ULTRASONIC_TRIG_Pin,
                    GPIO_PIN_RESET);
  __HAL_TIM_SET_CAPTUREPOLARITY(timer,
                                TIM_CHANNEL_1,
                                TIM_INPUTCHANNELPOLARITY_RISING);
  __HAL_TIM_CLEAR_FLAG(timer, TIM_FLAG_CC1);

  hal_status = HAL_TIM_IC_Start_IT(timer, TIM_CHANNEL_1);
  if (hal_status != HAL_OK)
  {
    sensor->status = BSP_ULTRASONIC_STATUS_ERROR;
    return hal_status;
  }

  sensor->status = BSP_ULTRASONIC_STATUS_IDLE;
  return HAL_OK;
}

/**
 * @brief  发起一次超声波测距
 * @retval HAL_BUSY 表示上一轮测量尚未结束
 */
HAL_StatusTypeDef BSP_Ultrasonic_StartMeasurement(
    BSP_Ultrasonic_HandleTypeDef *sensor)
{
  uint32_t start_counter;
  uint32_t primask;

  if ((sensor == NULL) || (sensor->timer == NULL))
  {
    return HAL_ERROR;
  }

  /* 测量尚未结束时禁止再次触发，避免覆盖边沿状态。 */
  primask = BSP_Ultrasonic_EnterCritical();
  if ((sensor->status == BSP_ULTRASONIC_STATUS_WAIT_RISING) ||
      (sensor->status == BSP_ULTRASONIC_STATUS_WAIT_FALLING))
  {
    BSP_Ultrasonic_ExitCritical(primask);
    return HAL_BUSY;
  }

  __HAL_TIM_SET_CAPTUREPOLARITY(sensor->timer,
                                TIM_CHANNEL_1,
                                TIM_INPUTCHANNELPOLARITY_RISING);
  __HAL_TIM_CLEAR_FLAG(sensor->timer, TIM_FLAG_CC1);
  sensor->rise_capture_us = 0U;
  sensor->measurement_started_ms = HAL_GetTick();
  sensor->status = BSP_ULTRASONIC_STATUS_WAIT_RISING;
  BSP_Ultrasonic_ExitCritical(primask);

  /* TIM2 以 1 MHz 计数，用计数差生成 10 微秒触发脉冲。 */
  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port,
                    ULTRASONIC_TRIG_Pin,
                    GPIO_PIN_SET);
  start_counter = __HAL_TIM_GET_COUNTER(sensor->timer);
  while ((uint32_t)(__HAL_TIM_GET_COUNTER(sensor->timer) - start_counter) <
         BSP_ULTRASONIC_TRIGGER_PULSE_US)
  {
  }
  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port,
                    ULTRASONIC_TRIG_Pin,
                    GPIO_PIN_RESET);

  return HAL_OK;
}

/** @brief 检查并处理无回波超时 */
void BSP_Ultrasonic_Process(BSP_Ultrasonic_HandleTypeDef *sensor,
                            uint32_t now_ms)
{
  uint32_t primask;

  if ((sensor == NULL) || (sensor->timer == NULL))
  {
    return;
  }

  primask = BSP_Ultrasonic_EnterCritical();
  if (((sensor->status == BSP_ULTRASONIC_STATUS_WAIT_RISING) ||
       (sensor->status == BSP_ULTRASONIC_STATUS_WAIT_FALLING)) &&
      ((uint32_t)(now_ms - sensor->measurement_started_ms) >=
       BSP_ULTRASONIC_ECHO_TIMEOUT_MS))
  {
    sensor->status = BSP_ULTRASONIC_STATUS_TIMEOUT;
    __HAL_TIM_SET_CAPTUREPOLARITY(sensor->timer,
                                  TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_RISING);
  }
  BSP_Ultrasonic_ExitCritical(primask);
}

/** @brief 获取一致的测量结果快照 */
HAL_StatusTypeDef BSP_Ultrasonic_GetData(
    const BSP_Ultrasonic_HandleTypeDef *sensor,
    BSP_Ultrasonic_DataTypeDef *data)
{
  uint32_t primask;

  if ((sensor == NULL) || (data == NULL) || (sensor->timer == NULL))
  {
    return HAL_ERROR;
  }

  /* 测量值由中断更新，复制期间保持临界区以获得一致快照。 */
  primask = BSP_Ultrasonic_EnterCritical();
  data->status = sensor->status;
  data->distance_mm = sensor->distance_mm;
  data->sample_valid = sensor->sample_valid;
  BSP_Ultrasonic_ExitCritical(primask);

  return HAL_OK;
}

/** @brief 处理回波上升沿和下降沿输入捕获 */
void BSP_Ultrasonic_IC_CaptureCallback(
    BSP_Ultrasonic_HandleTypeDef *sensor,
    TIM_HandleTypeDef *timer)
{
  uint32_t falling_capture;
  uint32_t pulse_width;
  uint32_t distance_mm;

  if ((sensor == NULL) || (timer == NULL) ||
      (timer != sensor->timer) ||
      (timer->Channel != HAL_TIM_ACTIVE_CHANNEL_1))
  {
    return;
  }

  if (sensor->status == BSP_ULTRASONIC_STATUS_WAIT_RISING)
  {
    sensor->rise_capture_us =
        HAL_TIM_ReadCapturedValue(timer, TIM_CHANNEL_1);
    sensor->status = BSP_ULTRASONIC_STATUS_WAIT_FALLING;
    __HAL_TIM_SET_CAPTUREPOLARITY(timer,
                                  TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_FALLING);
  }
  else if (sensor->status == BSP_ULTRASONIC_STATUS_WAIT_FALLING)
  {
    /* 无符号减法可在 TIM2 计数器回绕时继续得到正确脉宽。 */
    falling_capture = HAL_TIM_ReadCapturedValue(timer, TIM_CHANNEL_1);
    pulse_width = falling_capture - sensor->rise_capture_us;
    __HAL_TIM_SET_CAPTUREPOLARITY(timer,
                                  TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_RISING);

    if ((pulse_width == 0U) ||
        (pulse_width > BSP_ULTRASONIC_MAX_ECHO_US))
    {
      sensor->status = BSP_ULTRASONIC_STATUS_ERROR;
      return;
    }

    distance_mm = ((pulse_width * 343U) + 1000U) / 2000U;
    sensor->distance_mm = distance_mm;
    sensor->sample_valid = 1U;

    /*
     * 首次小于 50 毫米进入过近状态；进入后必须达到 60 毫米才恢复，
     * 避免测量值在有效下限附近反复切换。
     */
    if (sensor->too_close_active != 0U)
    {
      if (distance_mm < ULTRASONIC_RECOVERY_DISTANCE_MM)
      {
        sensor->status = BSP_ULTRASONIC_STATUS_TOO_CLOSE;
        return;
      }

      sensor->too_close_active = 0U;
    }
    else if (distance_mm < ULTRASONIC_MIN_VALID_DISTANCE_MM)
    {
      sensor->too_close_active = 1U;
      sensor->status = BSP_ULTRASONIC_STATUS_TOO_CLOSE;
      return;
    }

    sensor->status = BSP_ULTRASONIC_STATUS_VALID;
  }
}
