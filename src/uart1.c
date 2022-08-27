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
 * @brief     UART init
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
/*************** END OF FUNCTIONS *********************************************/

#if 0
  //--- IRQ
void UART0_IRQHandler(void){    
	UART1->THR = UART0->RBR;
}
#endif