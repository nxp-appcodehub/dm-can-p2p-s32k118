/*
 *   Copyright 2024 NXP
 *
 *   NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
 *   used strictly in accordance with the applicable license terms.  By expressly
 *   accepting such terms or by downloading, installing, activating and/or otherwise
 *   using the software, you are agreeing that you have read, and that you agree to
 *   comply with and are bound by, such license terms.  If you do not agree to be
 *   bound by the applicable license terms, then you may not retain, install,
 *   activate or otherwise use the software.
 *
 *   This file contains sample code only. It is not part of the production code deliverables.
 */

#ifndef CAN_APP_H
#define CAN_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
 *                                        INCLUDE FILES
 ==================================================================================================*/
#include "Std_Types.h"
#include "Port_Ci_Port_Ip.h"
/*==================================================================================================
 *                                       PUBLIC MACROS
 ==================================================================================================*/

/* Default CAN hardware object used for transmission (Tx mailbox) */
#define CAN_APP_TX_HOH   Can_43_FLEXCANConf_CanHardwareObject_CanHardwareObject_1

/* Default CAN controller index */
#define CAN_APP_CTRL_ID  Can_43_FLEXCANConf_CanController_CanController_0

/*==================================================================================================
 *                              SHARED RX STATE (filled from ISR callback)
 ==================================================================================================*/

/* Set to TRUE by CanIf_RxIndication when a new frame arrives; cleared by the application */
extern volatile boolean App_bNewRxFrame;

/* Last received frame content - valid when App_bNewRxFrame == TRUE */
extern uint8  App_RxData[8U];
extern uint8  App_RxLength;
extern uint32 App_RxCanId;

/*==================================================================================================
 *                                   FUNCTION PROTOTYPES
 ==================================================================================================*/

/**
 * @brief  Initialize the CAN driver and start the CAN controller.
 *         Call this once during system startup, after MCU/clock/GPIO init.
 */
void CAN_App_Init(void);

/**
 * @brief  Send a CAN frame.
 *
 * @param[in]  canId   11-bit or 29-bit CAN identifier.
 * @param[in]  data    Pointer to the payload bytes (max 8 bytes for classic CAN).
 * @param[in]  length  Number of bytes to send (1..8).
 *
 * @return TRUE  - frame was transmitted and confirmed by the driver.
 * @return FALSE - transmission timed out (bus error or no ACK).
 */
boolean CAN_App_Send(uint32 canId, const uint8 *data, uint8 length);

#ifdef __cplusplus
}
#endif

#endif /* CAN_APP_H */
