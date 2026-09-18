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
#include "can_app.h"
#include "Can_43_FLEXCAN.h"
#include "SchM_Can_43_FLEXCAN.h"
#include "Gpio_Dio_Ip.h"
#include "OsIf.h"
#include "CDD_Uart.h"
#include <stdio.h>   /* snprintf */
#include <string.h>  /* strlen   */

/*==================================================================================================
 *                                       LOCAL MACROS
 ==================================================================================================*/

#define CAN_APP_UART_CH      (0U)        /* UART channel used for debug output       */
#define CAN_APP_UART_TIMEOUT (10000U)    /* Uart_SyncSend timeout in us              */
#define CAN_APP_TX_TIMEOUT   (1000U)     /* CAN Tx polling loop iteration count      */

/* LED active-high convention (STD_HIGH = ON) */
#define LED_ON   (STD_HIGH)
#define LED_OFF  (STD_LOW)

/*==================================================================================================
 *                                      MODULE VARIABLES
 ==================================================================================================*/

/* Tx done flag - set by CanIf_TxConfirmation, cleared by CAN_App_Send */
static volatile boolean s_bTxDone = FALSE;

/* Running counters exposed for diagnostics */
static uint8 s_u8TxCount = 0U;
static uint8 s_u8RxCount = 0U;

/*==================================================================================================
 *                              SHARED RX STATE (filled from ISR callback)
 ==================================================================================================*/

volatile boolean App_bNewRxFrame = FALSE;
uint8            App_RxData[8U]  = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
uint8            App_RxLength    = 0U;
uint32           App_RxCanId     = 0U;

/*==================================================================================================
 *                                   LOCAL HELPERS
 ==================================================================================================*/

/* Send a null-terminated ASCII string over UART (blocking, fire-and-forget). */
static void Uart_Print(const char *msg)
{
    uint32 len = (uint32)strlen(msg);
    if (len > 0U)
    {
        (void)Uart_SyncSend(CAN_APP_UART_CH, (const uint8 *)msg, len, CAN_APP_UART_TIMEOUT);
    }
}

/* Convert a byte value to its two-character hex representation in buf[0..1]. */
static void ByteToHex(uint8 val, char *buf)
{
    static const char hex[] = "0123456789ABCDEF";
    buf[0] = hex[(val >> 4U) & 0x0FU];
    buf[1] = hex[val & 0x0FU];
}

/* Print the CAN payload bytes as "XX XX XX ..." followed by \r\n. */
static void Uart_PrintPayload(const uint8 *data, uint8 length)
{
    char    lineBuf[64U];   /* enough for 8 bytes: "XX XX XX XX XX XX XX XX \r\n" */
    uint8   pos = 0U;
    uint8   i;

    for (i = 0U; (i < length) && (i < 8U); i++)
    {
        ByteToHex(data[i], &lineBuf[pos]);
        pos += 2U;
        lineBuf[pos] = ' ';
        pos++;
    }
    lineBuf[pos]     = '\r';
    lineBuf[pos + 1U] = '\n';
    lineBuf[pos + 2U] = '\0';

    Uart_Print(lineBuf);
}

/*==================================================================================================
 *                                  CAN DRIVER CALLBACKS
 * All AUTOSAR CAN callbacks are implemented here so that main.c stays clean.
 ==================================================================================================*/

/* Called when a frame has been successfully placed on the bus. */
void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    (void)CanTxPduId;

    s_bTxDone = TRUE;
    s_u8TxCount++;

    /* Turn off blue LED: transmission complete */
    Gpio_Dio_Ip_WritePin(BLUE_PORT, BLUE_PIN, LED_OFF);

    Uart_Print("[CAN TX] OK - frame confirmed\r\n");
}

/* Called from ISR when a CAN frame has been received. */
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr)
{
    uint8 i;
    char  idBuf[32U];

    /* Capture received frame */
    App_RxCanId  = (uint32)(Mailbox->CanId & 0x1FFFFFFFU);
    App_RxLength = (uint8)PduInfoPtr->SduLength;
    for (i = 0U; i < App_RxLength; i++)
    {
        App_RxData[i] = PduInfoPtr->SduDataPtr[i];
    }

    /* Signal main loop */
    App_bNewRxFrame = TRUE;
    s_u8RxCount++;

    /* Green LED ON: frame received (brief visual cue) */
    Gpio_Dio_Ip_WritePin(GREEN_PORT, GREEN_PIN, LED_ON);

    /* UART: print RX event header */
    snprintf(idBuf, sizeof(idBuf), "[CAN RX] ID=0x%03X len=%u data: ",
             (unsigned int)App_RxCanId, (unsigned int)App_RxLength);
    Uart_Print(idBuf);

    /* UART: print payload bytes */
    Uart_PrintPayload(App_RxData, App_RxLength);
}

/* Called on bus-off condition. */
void CanIf_ControllerBusOff(uint8 ControllerId)
{
    (void)ControllerId;

    /* Red LED ON to signal the error */
    Gpio_Dio_Ip_WritePin(RED_PORT, RED_PIN, LED_ON);

    Uart_Print("[CAN ERR] Bus-Off detected on controller\r\n");
}

/* Called after a controller mode change - informational only. */
void CanIf_ControllerModeIndication(uint8 ControllerId,
                                    Can_ControllerStateType ControllerMode)
{
    (void)ControllerId;
    (void)ControllerMode;
}

/*==================================================================================================
 *                                   PUBLIC FUNCTIONS
 ==================================================================================================*/

void CAN_App_Init(void)
{
    /* Initialize CAN driver */
#if (CAN_43_FLEXCAN_PRECOMPILE_SUPPORT == STD_ON)
    Can_43_FLEXCAN_Init(NULL_PTR);
#else
    Can_43_FLEXCAN_Init(&Can_43_FLEXCAN_Config);
#endif

    /* Start the CAN controller */
    Can_43_FLEXCAN_SetControllerMode(CAN_APP_CTRL_ID, CAN_CS_STARTED);

    Uart_Print("[INIT] CAN initialized and started\r\n");
}

boolean CAN_App_Send(uint32 canId, const uint8 *data, uint8 length)
{
    Can_PduType  pdu;
    /* Cast away const: the driver API takes a non-const pointer but does not modify the data */
    uint8       *sduPtr = (uint8 *)(uintptr_t)data;
    uint32       timeout;
    boolean      result;
    char         msgBuf[64U];

    /* Build the PDU descriptor */
    pdu.id          = (Can_IdType)canId;
    pdu.swPduHandle = 0U;
    pdu.length      = length;
    pdu.sdu         = sduPtr;

    /* UART: announce what is about to be sent */
    snprintf(msgBuf, sizeof(msgBuf), "[CAN TX] Sending ID=0x%03X len=%u ...\r\n",
             (unsigned int)canId, (unsigned int)length);
    Uart_Print(msgBuf);

    /* Blue LED ON: frame submission in progress */
    Gpio_Dio_Ip_WritePin(BLUE_PORT, BLUE_PIN, LED_ON);
    s_bTxDone = FALSE;

    /* Submit the frame to the driver */
    (void)Can_43_FLEXCAN_Write(CAN_APP_TX_HOH, &pdu);

    /* Poll until TxConfirmation callback sets s_bTxDone or timeout expires */
    timeout = CAN_APP_TX_TIMEOUT;
    while ((s_bTxDone == FALSE) && (timeout != 0U))
    {
        Can_43_FLEXCAN_MainFunction_Write();
        timeout--;
    }

    if (s_bTxDone == TRUE)
    {
        result = TRUE;
        /* Blue LED already turned off inside TxConfirmation */
    }
    else
    {
        /* Timeout: turn off blue LED, flash red to signal error */
        Gpio_Dio_Ip_WritePin(BLUE_PORT, BLUE_PIN, LED_OFF);
        Gpio_Dio_Ip_WritePin(RED_PORT,  RED_PIN,  LED_ON);
        Uart_Print("[CAN TX] TIMEOUT - no confirmation received\r\n");
        result = FALSE;
    }

    return result;
}

#ifdef __cplusplus
}
#endif

/** @} */
