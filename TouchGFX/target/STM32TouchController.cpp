/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : STM32TouchController.cpp
  ******************************************************************************
  * This file was created by TouchGFX Generator 4.26.1. This file is only
  * generated once! Delete this file from your project and re-generate code
  * using STM32CubeMX or change this file manually to update it.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN STM32TouchController */

#include <STM32TouchController.hpp>
#include "main.h"
#include "cmsis_os.h"

extern "C" I2C_HandleTypeDef hi2c3;

/* SWD watch: this is input diagnostics, not a count of GUI button clicks. */
struct TouchDiagnostics
{
    uint32_t init_calls;
    uint32_t polls;
    uint32_t valid_samples;
    uint32_t read_errors;
    HAL_StatusTypeDef last_status;
    uint8_t chip_id;
    bool ready;
    bool pressed;
    uint16_t raw_x;
    uint16_t raw_y;
    uint16_t screen_x;
    uint16_t screen_y;
};
volatile TouchDiagnostics touch_diag = {};
static bool touch_initialization_pending = true;

namespace
{
constexpr uint16_t TouchAddress = 0x70U; // HAL expects 7-bit 0x38 shifted left
constexpr uint16_t ChipIdRegister = 0xA8U;
constexpr uint16_t InterruptModeRegister = 0xA4U;
constexpr uint16_t TouchStatusRegister = 0x02U;
constexpr uint32_t TimeoutMs = 5U;

HAL_StatusTypeDef ReadRegisters(uint16_t reg, uint8_t* data, uint16_t size)
{
    const HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
        &hi2c3, TouchAddress, reg, I2C_MEMADD_SIZE_8BIT, data, size, TimeoutMs);
    touch_diag.last_status = status;
    if (status != HAL_OK) ++touch_diag.read_errors;
    return status;
}
}

void STM32TouchController::init()
{
    ++touch_diag.init_calls;
    touch_initialization_pending = true;
    touch_diag.ready = false;
    touch_diag.pressed = false;
    // TouchGFX calls init before the scheduler starts. FreeRTOS queue creation
    // has already raised BASEPRI, so HAL_Delay/I2C timeout ticks cannot run here.
    // Perform bus I/O on the first GUI-task poll instead.
}

static void InitializeController()
{
    touch_diag.ready = false;
    touch_diag.pressed = false;
    touch_diag.chip_id = 0U;
    touch_diag.last_status = HAL_ERROR;
    // CubeMX owns I2C3 configuration. It must be ready before TouchGFX init.
    if (hi2c3.Instance != I2C3 || HAL_I2C_GetState(&hi2c3) != HAL_I2C_STATE_READY)
        return;

    osDelay(200U); // Runs in the GUI task with interrupts and scheduler active.
    uint8_t id = 0U;
    if (ReadRegisters(ChipIdRegister, &id, 1U) != HAL_OK) return;
    touch_diag.chip_id = id;
    if (id != 0x51U) return;

    uint8_t pollingMode = 0U;
    const HAL_StatusTypeDef status = HAL_I2C_Mem_Write(
        &hi2c3, TouchAddress, InterruptModeRegister, I2C_MEMADD_SIZE_8BIT,
        &pollingMode, 1U, TimeoutMs);
    touch_diag.last_status = status;
    if (status != HAL_OK) ++touch_diag.read_errors;
    touch_diag.ready = (status == HAL_OK);
}

bool STM32TouchController::sampleTouch(int32_t& x, int32_t& y)
{
    ++touch_diag.polls;
    if (touch_initialization_pending) {
        touch_initialization_pending = false;
        InitializeController();
    }
    touch_diag.pressed = false;
    if (!touch_diag.ready) return false;

    // Consecutive registers: TD_STATUS, P1_XH, P1_XL, P1_YH, P1_YL.
    uint8_t data[5] = {0};
    if (ReadRegisters(TouchStatusRegister, data, sizeof(data)) != HAL_OK)
        return false;
    const uint8_t count = data[0] & 0x0FU;
    if (count == 0U || count > 5U) return false;
    if (((data[1] >> 6U) & 0x03U) == 1U) return false; // Lift-up event

    const uint16_t rawX = ((data[1] & 0x0FU) << 8U) | data[2];
    const uint16_t rawY = ((data[3] & 0x0FU) << 8U) | data[4];
    touch_diag.raw_x = rawX;
    touch_diag.raw_y = rawY;

    // Match the discovery BSP's TS_SWAP_XY for the 480x272 landscape panel.
    const uint16_t screenX = rawY;
    const uint16_t screenY = rawX;
    if (screenX >= 480U || screenY >= 272U) return false;
    x = screenX;
    y = screenY;
    touch_diag.screen_x = screenX;
    touch_diag.screen_y = screenY;
    touch_diag.pressed = true;
    ++touch_diag.valid_samples;
    return true;
}

/* USER CODE END STM32TouchController */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
