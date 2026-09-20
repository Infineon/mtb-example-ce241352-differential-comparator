/*******************************************************************************
* File Name:   main.c
*
* Description: This is the main file for the main CPU non safe application of
* the code example. It initializes the peripherals, PPCA and starts PPCA.
* This file contains the interrupt service routine that generates signal for
* comparing with an input to the DCSG from the pot.
*
* Related Document: See README.md
*
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cy_pdl.h"
#include "cycfg.h"
#include "cybsp.h"
#include <stdio.h>
#include "cy_retarget_io.h"

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Debug UART variables */
static cy_stc_scb_uart_context_t    UART_context; /* UART context */
static mtb_hal_uart_t               UART_hal_obj; /* Debug UART HAL object */

/******************************************************************************
* Macros
*******************************************************************************/
/* These are the addresses where the core0 and core1 images are located. */
#define CORE0_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca0_nvm_C_S_START
#define CORE1_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca1_nvm_C_S_START
#define PPCA0_IMAGE_SIZE       CYMEM_CM33_0_S_ppca0_code_SIZE
#define PPCA1_IMAGE_SIZE       CYMEM_CM33_0_S_ppca1_code_SIZE

/* This is the address in the memory shared between main CPU and PPCA CPUs
 * at which PPCA CPU 0 is using for sharing data with main CPU. */
#define PPCA_M33_0_SHARED_ADDRESS 0x53050400

/* This is the address in the memory shared between main CPU and PPCA CPUs
 * at which PPCA CPU 1 is using for sharing data with main CPU. */
#define PPCA_M33_1_SHARED_ADDRESS 0x53050800

/* Triangular wave maximum count */
#define COUNTER_MAX 4095

/*******************************************************************************
* Global Variables
*******************************************************************************/
volatile uint32_t count = 0;
volatile uint32_t count_up = 0;

cy_stc_sysint_t pwm_intr_config =
{
    .intrSrc = PWM_IRQ,
    .intrPriority = 1U,
};

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void pwm_compare_isr();

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: main
*********************************************************************************
* Summary:
* This is the main function for the non safe project for the main core. It
* performs the initialization of the peripherals, initialization and starting
* of the PPCA.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t             result;
    cy_en_tcpwm_status_t  pwm_status;
    cy_en_sysint_status_t status;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Init(UART_HW, &UART_config, &UART_context);
    Cy_SCB_UART_Enable(UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&UART_hal_obj, &UART_hal_config,
                                &UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize redirecting of low level IO */
    result = cy_retarget_io_init(&UART_hal_obj);

    /* retarget IO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Transmit header to the terminal */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: Differential comparator\r\n");
    printf("************************************************************\r\n\n");
    printf("This CE compares input from the potentiometer R212 with a \r\n");
    printf("triangular wave generated by the code using the comparator \r\n");
    printf("in a DCSG. Output can be observed on the LED D13 and Pin P3.5 \r\n");
    printf("using an oscilloscope. Try rotating the potentiometer, you \r\n");
    printf("can observe the change in the output.\r\n");

    /* enable interrupts */
    __enable_irq();

     /* Initializing PPCA Configuration. */
    Cy_PPCA_CNFG_Init(PPCA_CNFG_HW, &PPCA_CNFG_config);

    /* Enabling PPCA Configuration. */
    Cy_PPCA_Enable(PPCA_CNFG_HW);

    /* Initializing TCPWM as PWM */
    pwm_status = Cy_TCPWM_PWM_Init(PWM_HW, PWM_NUM, &PWM_config);

    /* Initialization failed */
    if(CY_TCPWM_SUCCESS != pwm_status)
    {
        CY_ASSERT(0);
    }

    /* Enabling PWM */
    Cy_TCPWM_PWM_Enable(PWM_HW, PWM_NUM);

    /* Initializing the timer interrupt */
    status = Cy_SysInt_Init(&pwm_intr_config, &pwm_compare_isr);

    /* Interrupt initialization failed. */
    if(CY_SYSINT_SUCCESS != status)
    {
        CY_ASSERT(0);
    }

    /* Clearing any pending interrupt. */
    NVIC_EnableIRQ(pwm_intr_config.intrSrc);

    /* Initializing ATOP Analog reference */
    Cy_PPCA_AREF_Init(AREF_HW, &AREF_config);

    /* Enabling AREF */
    Cy_PPCA_AREF_Enable(AREF_HW);

    /* Initializing DCSG */
    Cy_PPCA_DCSG_Init(DCSG_HW, &DCSG_config);

    /* Enabling interrupts. */
    Cy_PPCA_DCSG_Enable(DCSG_HW);

    /* Enabling interrupts. */
    __enable_irq();

    /* Initializing and starting PPCA CPU Core 0. */
    Cy_System_Init_CPU0((void*)CORE0_IMAGE_ADDRESS, PPCA0_IMAGE_SIZE);

    /* Initializing and starting PPCA CPU Core 1. Use the below line to start PPCA Core 1 */
    /*Cy_System_Init_CPU1((void*)CORE1_IMAGE_ADDRESS, PPCA1_IMAGE_SIZE);*/

    /* Starting PWM */
    Cy_TCPWM_TriggerStart_Single(PWM_HW, PWM_NUM);

    for (;;)
    {
        Cy_SysLib_Delay(250);
    }
}

/*******************************************************************************
* Function Name: pwm_compare_isr
*********************************************************************************
* Summary:
* This is the interrupt service routine for the PWM. This ISR is used for generating
* triangular wave that is used for comparing with the potentiometer.
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
void pwm_compare_isr()
{
    /* Clearing the PWM capture interrupt. */
    Cy_TCPWM_ClearInterrupt(PWM_HW, PWM_NUM, CY_TCPWM_INT_ON_CC0_OR_TC);

    /* Updating DAC output for generating triangular wave */
    Cy_PPCA_DCSG_Set_Threshold(DCSG_HW, count);

    /* Triangular wave generation. */
    if(count_up)
    {
        if(count < COUNTER_MAX)
        {
            count++;
        }
        else
        {
            count_up = 0;
        }
    }
    else
    {
        if(count > 0)
        {
            count--;
        }
        else
        {
            count_up = 1;
        }
    }
}
