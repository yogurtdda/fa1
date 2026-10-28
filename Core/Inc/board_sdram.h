#ifndef BOARD_SDRAM_H
#define BOARD_SDRAM_H

#include "stm32f7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_SDRAM_BASE_ADDRESS       0xC0000000UL
#define BOARD_SDRAM_SIZE_BYTES         (8UL * 1024UL * 1024UL)
#define BOARD_LCD_FRAMEBUFFER0_ADDRESS 0xC0000000UL
#define BOARD_LCD_FRAMEBUFFER1_ADDRESS 0xC0040000UL
#define BOARD_LCD_FRAMEBUFFER_BYTES    (480UL * 272UL * 2UL)

typedef enum
{
  BOARD_SDRAM_RESET = 0,
  BOARD_SDRAM_CHECK_CONFIG,
  BOARD_SDRAM_CLOCK_ENABLE,
  BOARD_SDRAM_PRECHARGE,
  BOARD_SDRAM_AUTO_REFRESH,
  BOARD_SDRAM_LOAD_MODE,
  BOARD_SDRAM_SET_REFRESH,
  BOARD_SDRAM_READY
} BoardSDRAM_Stage;

/* Inspect this object through SWD if startup stops in Error_Handler(). */
typedef struct
{
  BoardSDRAM_Stage stage;
  HAL_StatusTypeDef hal_status;
  uint32_t clock_hz;
  uint32_t refresh_count;
  uint32_t framebuffer_bytes_cleared;
} BoardSDRAM_Status;

extern volatile BoardSDRAM_Status board_sdram_status;

/* Call after HAL_SDRAM_Init(), before any CPU/LTDC/DMA access to SDRAM.
 * Board-specific timing contract: HCLK=200 MHz, SDCLK=100 MHz, CAS=3.
 * The caller owns the handle; this function does not initialise FMC again. */
HAL_StatusTypeDef Board_SDRAM_Init(SDRAM_HandleTypeDef *sdram);

/* Startup only: clear the two RGB565 buffers used by TouchGFX.
 * SDRAM must be ready and non-cacheable; do not call while rendering. */
HAL_StatusTypeDef Board_SDRAM_ClearFramebuffers(void);

#ifdef __cplusplus
}
#endif

#endif
