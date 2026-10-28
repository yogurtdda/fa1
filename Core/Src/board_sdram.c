#include "board_sdram.h"

#define SDRAM_COMMAND_TIMEOUT_MS 100U
#define SDRAM_MODE_BURST_LENGTH_1     0x0000U
#define SDRAM_MODE_SEQUENTIAL         0x0000U
#define SDRAM_MODE_CAS_LATENCY_3       0x0030U
#define SDRAM_MODE_STANDARD_OPERATION 0x0000U
#define SDRAM_MODE_SINGLE_WRITE_BURST  0x0200U

volatile BoardSDRAM_Status board_sdram_status;

_Static_assert(BOARD_LCD_FRAMEBUFFER0_ADDRESS + BOARD_LCD_FRAMEBUFFER_BYTES <=
               BOARD_LCD_FRAMEBUFFER1_ADDRESS, "LCD framebuffers overlap");
_Static_assert(BOARD_LCD_FRAMEBUFFER1_ADDRESS + BOARD_LCD_FRAMEBUFFER_BYTES <=
               BOARD_SDRAM_BASE_ADDRESS + BOARD_SDRAM_SIZE_BYTES,
               "LCD framebuffer exceeds SDRAM");

static HAL_StatusTypeDef SendCommand(SDRAM_HandleTypeDef *sdram,
                                    BoardSDRAM_Stage stage, uint32_t mode,
                                    uint32_t refreshes, uint32_t mode_register)
{
  FMC_SDRAM_CommandTypeDef command = {0};
  board_sdram_status.stage = stage;
  command.CommandMode = mode;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  command.AutoRefreshNumber = refreshes;
  command.ModeRegisterDefinition = mode_register;
  board_sdram_status.hal_status =
      HAL_SDRAM_SendCommand(sdram, &command, SDRAM_COMMAND_TIMEOUT_MS);
  return board_sdram_status.hal_status;
}

HAL_StatusTypeDef Board_SDRAM_Init(SDRAM_HandleTypeDef *sdram)
{
  board_sdram_status.stage = BOARD_SDRAM_CHECK_CONFIG;
  board_sdram_status.hal_status = HAL_ERROR;
  board_sdram_status.clock_hz = HAL_RCC_GetHCLKFreq() / 2U;
  board_sdram_status.refresh_count = 0U;
  board_sdram_status.framebuffer_bytes_cleared = 0U;

  /* The FMC timing table and SDRAM mode register must describe the same
   * hardware. Fail before accessing memory if CubeMX settings have drifted. */
  if (sdram == NULL || sdram->Instance != FMC_SDRAM_DEVICE ||
      HAL_RCC_GetHCLKFreq() != 200000000U ||
      sdram->State != HAL_SDRAM_STATE_READY ||
      sdram->Init.SDBank != FMC_SDRAM_BANK1 ||
      sdram->Init.SDClockPeriod != FMC_SDRAM_CLOCK_PERIOD_2 ||
      sdram->Init.CASLatency != FMC_SDRAM_CAS_LATENCY_3 ||
      sdram->Init.ColumnBitsNumber != FMC_SDRAM_COLUMN_BITS_NUM_8 ||
      sdram->Init.RowBitsNumber != FMC_SDRAM_ROW_BITS_NUM_12 ||
      sdram->Init.MemoryDataWidth != FMC_SDRAM_MEM_BUS_WIDTH_16 ||
      sdram->Init.InternalBankNumber != FMC_SDRAM_INTERN_BANKS_NUM_4 ||
      sdram->Init.WriteProtection != FMC_SDRAM_WRITE_PROTECTION_DISABLE)
  {
    return HAL_ERROR;
  }

  HAL_StatusTypeDef status = SendCommand(sdram, BOARD_SDRAM_CLOCK_ENABLE,
                                       FMC_SDRAM_CMD_CLK_ENABLE, 1U, 0U);
  if (status != HAL_OK) return status;
  HAL_Delay(1U); /* At least 100 us after a stable SDRAM clock is available. */

  status = SendCommand(sdram, BOARD_SDRAM_PRECHARGE,
                       FMC_SDRAM_CMD_PALL, 1U, 0U);
  if (status != HAL_OK) return status;
  status = SendCommand(sdram, BOARD_SDRAM_AUTO_REFRESH,
                       FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8U, 0U);
  if (status != HAL_OK) return status;

  const uint32_t mode_register = SDRAM_MODE_BURST_LENGTH_1 |
      SDRAM_MODE_SEQUENTIAL | SDRAM_MODE_CAS_LATENCY_3 |
      SDRAM_MODE_STANDARD_OPERATION | SDRAM_MODE_SINGLE_WRITE_BURST;
  status = SendCommand(sdram, BOARD_SDRAM_LOAD_MODE,
                       FMC_SDRAM_CMD_LOAD_MODE, 1U, mode_register);
  if (status != HAL_OK) return status;

  /* 4096 rows / 64 ms. FMC accounts for 20 clock cycles:
   * floor(100000000 * 0.064 / 4096 - 20) = 1542. */
  board_sdram_status.refresh_count =
      (uint32_t)(((uint64_t)board_sdram_status.clock_hz * 64U) /
                 (1000U * 4096U)) - 20U;
  board_sdram_status.stage = BOARD_SDRAM_SET_REFRESH;
  status = HAL_SDRAM_ProgramRefreshRate(sdram, board_sdram_status.refresh_count);
  board_sdram_status.hal_status = status;
  if (status != HAL_OK) return status;
  __DSB();
  board_sdram_status.stage = BOARD_SDRAM_READY;
  return HAL_OK;
}

HAL_StatusTypeDef Board_SDRAM_ClearFramebuffers(void)
{
  if (board_sdram_status.stage != BOARD_SDRAM_READY) return HAL_ERROR;
  volatile uint16_t *front = (volatile uint16_t *)BOARD_LCD_FRAMEBUFFER0_ADDRESS;
  volatile uint16_t *back = (volatile uint16_t *)BOARD_LCD_FRAMEBUFFER1_ADDRESS;
  for (uint32_t index = 0U; index < BOARD_LCD_FRAMEBUFFER_BYTES / 2U; ++index)
  {
    front[index] = 0U;
    back[index] = 0U;
  }
  __DSB();
  board_sdram_status.framebuffer_bytes_cleared = BOARD_LCD_FRAMEBUFFER_BYTES * 2U;
  return HAL_OK;
}
