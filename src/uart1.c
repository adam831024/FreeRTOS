/******************************************************************************
 * Copyright (C)
 *
 * NAME:
 *		uart1.c
 * DESCRIPTION:
 *      uart.c is define in SDK, so name as uart1 for easy different.
*******************************************************************************/
/******************************************************************************
 * Includes
 *******************************************************************************/
/*Standard include*/
#include <stdint.h>
#include "Nano100Series.h"   
#include "uart.h" 

/*Free-RTOS include*/


/*Application include*/

/******************************************************************************
 * Module Preprocessor Constants
 *******************************************************************************/
#define CONSTANT 5

/******************************************************************************
 * Module Preprocessor Macros
 *******************************************************************************/

/******************************************************************************
 * Module Typedefs
 *******************************************************************************/

/******************************************************************************
 * Module Variable Definitions
 *******************************************************************************/

/******************************************************************************
 * Function Prototypes
 *******************************************************************************/

/******************************************************************************
 * Function Definitions
 *******************************************************************************/
/******************************************************************************
 * @brief     UART0 init
 * @param[in] baudrate                  uart baudrate
 * @return                              void
 *******************************************************************************/
void init_UART0(uint32_t baudRate)
{
  SYS_UnlockReg();
  //--- CLK
  CLK_EnableModuleClock(UART0_MODULE);
  CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART_S_HXT, CLK_UART_CLK_DIVIDER(1));
  //--- PIN
  SYS->PB_L_MFP = (SYS->PB_L_MFP & ~SYS_PB_L_MFP_PB0_MFP_Msk) | SYS_PB_L_MFP_PB0_MFP_UART0_RX;
  SYS->PB_L_MFP = (SYS->PB_L_MFP & ~SYS_PB_L_MFP_PB1_MFP_Msk) | SYS_PB_L_MFP_PB1_MFP_UART0_TX;
  SYS_LockReg();
  //--- OPEN
  UART_Open(UART0, baudRate);
  //--- NVIC
  UART_ENABLE_INT(UART0, UART_IER_RDA_IE_Msk);
  NVIC_EnableIRQ(UART0_IRQn);
}

/******************************************************************************
 * @brief     IRQ, UART0 Rx callback
 * @return                              void
 *******************************************************************************/
void UART0_IRQHandler(void)
{
  /*the same as UART_WRITE(UART0, UART0->RBR)*/
  UART0->THR = UART0->RBR;
}
/******************************************************************************
 * @brief     UART0 Tx send
 * @param[in] pData                     data pointer
 * @param[in] dataLength                length of pData
 * @return                              void
 *******************************************************************************/
void uart0Send(uint8_t *pData, uint16_t dataLength)
{
  uint16_t i = 0;
  for (i = 0; i < dataLength; i++)
  {
    UART0->THR = pData[i];
  }
}
/******************************************************************************
 * @brief     UART1 init
 * @param[in] baudrate                  uart baudrate
 * @return                              void
 *******************************************************************************/
void init_UART1(uint32_t baudrate)
{
  SYS_UnlockReg();
  //--- CLK
  CLK_EnableModuleClock(UART1_MODULE);
  CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART_S_HXT, CLK_UART_CLK_DIVIDER(1));
  //--- PIN
  SYS->PB_L_MFP &= ~SYS_PB_L_MFP_PB4_MFP_Msk;
  SYS->PB_L_MFP |= SYS_PB_L_MFP_PB4_MFP_UART1_RX;
  SYS->PB_L_MFP &= ~SYS_PB_L_MFP_PB5_MFP_Msk;
  SYS->PB_L_MFP |= SYS_PB_L_MFP_PB5_MFP_UART1_TX;
  SYS_LockReg();
  //--- OPEN
  UART_Open(UART1, baudrate);
  //--- NVIC
  UART_ENABLE_INT(UART1, UART_IER_RDA_IE_Msk);
  NVIC_EnableIRQ(UART1_IRQn);
}
/******************************************************************************
 * @brief     IRQ, UART1 Rx callback
 * @return                              void
 *******************************************************************************/
void UART1_IRQHandler(void)
{
  /*the same as UART_WRITE(UART1, UART1->RBR)*/
  UART0->THR = UART1->RBR;
}
/******************************************************************************
 * @brief     UART1 Tx send
 * @param[in] pData                     data pointer
 * @param[in] dataLength                length of pData
 * @return                              void
 *******************************************************************************/
void uart1Send(uint8_t *pData, uint16_t dataLength)
{
  uint16_t i = 0;
  for (i = 0; i < dataLength; i++)
  {
    UART1->THR = pData[i];
  }
}
/*************** END OF FUNCTIONS *********************************************/
