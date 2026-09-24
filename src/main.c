/*
 *   Copyright 2026 NXP
 *
 *   NXP Proprietary. This software is owned or controlled by NXP and may only be
 *   used strictly in accordance with the applicable license terms.  By expressly
 *   accepting such terms or by downloading, installing, activating and/or otherwise
 *   using the software, you are agreeing that you have read, and that you agree to
 *   comply with and are bound by, such license terms.  If you do not agree to be
 *   bound by the applicable license terms, then you may not retain, install,
 *   activate or otherwise use the software.
 *
 *   This file contains sample code only. It is not part of the production code deliverables.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
 *                                        INCLUDE FILES
 ==================================================================================================*/
#include "Mcu.h"
#include "Platform.h"
#include "Port_Ci_Port_Ip.h"
#include "Gpio_Dio_Ip.h"
#include "OsIf.h"
#include "CDD_Uart.h"
#include "can_app.h"   /* CAN_App_Init, CAN_App_Send, shared RX state */
#include <string.h>

/*==================================================================================================
 *                                       LOCAL MACROS
 ==================================================================================================*/

/* LED active-high convention */
#define LED_ON              (STD_HIGH)
#define LED_OFF             (STD_LOW)

/* Interval between consecutive Tx frames on the sender board (ms) */
#define TX_INTERVAL_MS      (500U)

/* Duration of the green LED flash on the receiver board (ms) */
#define RX_LED_FLASH_MS     (200U)

/* UART channel */
#define UART_CHANNEL        (0U)
#define UART_TIMEOUT_US     (10000U)

/*==================================================================================================
 *                                   LOCAL FUNCTION PROTOTYPES
 ==================================================================================================*/

static void DelayMs(uint32 ms);
static void SendMessage(const char *msg);

/*==================================================================================================
 *                                       LOCAL FUNCTIONS
 ==================================================================================================*/
void UartCallback(uint8 HwInstance, Uart_EventType Event) {
	(void) HwInstance;
	(void) Event;
}

/* Blocking delay using OsIf system counter */
static void DelayMs(uint32 ms) {
	uint32 cur = OsIf_GetCounter(OSIF_COUNTER_SYSTEM);
	uint32 elapsed = 0U;
	uint32 timeout = OsIf_MicrosToTicks(ms * 1000U, OSIF_COUNTER_SYSTEM);

	while (elapsed < timeout) {
		elapsed += OsIf_GetElapsed(&cur, OSIF_COUNTER_SYSTEM);
	}
}

/* Send a null-terminated string over UART (synchronous) */
static void SendMessage(const char *msg) {
	uint32 len = (uint32) strlen(msg);
	if (len > 0U) {
		(void) Uart_SyncSend(UART_CHANNEL, (const uint8*) msg, len,
				UART_TIMEOUT_US);
	}
}

/*==================================================================================================
 *                                            MAIN
 ==================================================================================================*/

int main(void) {
	/*
	 * boardOne = TRUE  -> this board SENDS CAN frames (blue LED blinks on each Tx)
	 * boardOne = FALSE -> this board RECEIVES CAN frames (green LED blinks on each Rx)
	 */
	boolean boardOne = FALSE;

	uint8 txPayload[8U] = { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
			0x08U };
	uint8 txSeq = 0U; /* rolling sequence number embedded in payload[0] */

	/* ------------------------------------------------------------------
	 * System initialization
	 * ------------------------------------------------------------------ */
	OsIf_Init(NULL_PTR);
	Platform_Init(NULL_PTR);

#if (MCU_PRECOMPILE_SUPPORT == STD_ON)
    Mcu_Init(NULL_PTR);
#else
	Mcu_Init(&Mcu_Config);
#endif
	Mcu_InitClock(McuClockSettingConfig_0);
#if (MCU_NO_PLL == STD_OFF)
    while (MCU_PLL_LOCKED != Mcu_GetPllStatus()) { /* wait for PLL lock */ }
    Mcu_DistributePllClock();
#endif
	Mcu_SetMode(McuModeSettingConf_0);

	Port_Ci_Port_Ip_Init(
	NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
			g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals);

	/* Initialize UART for debug output */
	Uart_Init(NULL_PTR);

	/* All LEDs off at startup */
	Gpio_Dio_Ip_WritePin(BLUE_PORT, BLUE_PIN, LED_OFF);
	Gpio_Dio_Ip_WritePin(GREEN_PORT, GREEN_PIN, LED_OFF);
	Gpio_Dio_Ip_WritePin(RED_PORT, RED_PIN, LED_OFF);

	/* Initialize and start CAN (driver init + controller start + UART log) */
	CAN_App_Init();

	/* Announce board role over UART */
	if (boardOne) {
		SendMessage("[INIT] Board role: SENDER\r\n");
	} else {
		SendMessage("[INIT] Board role: RECEIVER\r\n");
	}

	/* ------------------------------------------------------------------
	 * Main application loop
	 * ------------------------------------------------------------------ */
	while (TRUE) {
		if (boardOne) {
			/* ----------------------------------------------------------------
			 * SENDER BOARD
			 * Send one CAN frame every TX_INTERVAL_MS ms.
			 * The call handles: PDU build, Write, TxConfirmation wait, LED,
			 * and all UART messages.
			 * ---------------------------------------------------------------- */
			txPayload[0] = txSeq; /* embed rolling counter in first byte */
			txSeq++;

			if (CAN_App_Send(0x100U, txPayload, 8U) == FALSE) {
				/* Transmission timed out - flash red LED briefly */
				DelayMs(100U);
				Gpio_Dio_Ip_WritePin(RED_PORT, RED_PIN, LED_OFF);
			}

			DelayMs(TX_INTERVAL_MS);
		} else {
			/* ----------------------------------------------------------------
			 * RECEIVER BOARD
			 * Frames arrive via interrupt -> CanIf_RxIndication (in can_app.c)
			 * sets App_bNewRxFrame and prints the UART log.
			 * Here we only handle the green LED flash.
			 * ---------------------------------------------------------------- */
			if (App_bNewRxFrame == TRUE) {
				App_bNewRxFrame = FALSE;

				/* Green LED already turned ON inside CanIf_RxIndication */
				DelayMs(RX_LED_FLASH_MS);
				Gpio_Dio_Ip_WritePin(GREEN_PORT, GREEN_PIN, LED_OFF);
			}
		}
	}

	/* Never reached - kept to satisfy the compiler */
	return (0U);
}

#ifdef __cplusplus
}
#endif

/** @} */
