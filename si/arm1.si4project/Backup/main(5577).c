/*!
    \file    main.c
    \brief   led spark with systick

    \version 2016-08-15, V1.0.0, firmware for GD32F4xx
    \version 2018-12-12, V2.0.0, firmware for GD32F4xx
    \version 2020-09-30, V2.1.0, firmware for GD32F4xx
    \version 2022-03-09, V3.0.0, firmware for GD32F4xx
*/

/*
    Copyright (c) 2022, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f4xx.h"
#include "systick.h"
#include <stdio.h>
#include "exmc_sram.h"
#include "config.h"
#include "drv_gpio.h"
#include "drv_usart.h"
#include "drv_rtc.h"
#include "drv_timer.h"
//#include "public.h"
#include "pin_numbers_def.h"
#include "frame_analysis.h"
#include "uartadapter.h"
#include "gnss.h"
//#include "taskschedule.h"
#include "computerFrameParse.h"
#include "fwdog.h"
//#include "fmc_operation.h"
#include "DRamAdapter.h"
#include "nav_task.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "event_groups.h"



/*****************kalman algorithm headers**************************/
#include "constant.h"
#include "matrix.h"
#include "inavgnss.h"
#include "inavins.h"
#include "inavlog.h"

#define	ARM1_TO_ARM2_IO		GD32F450_PA12_PIN_NUM
#define	SYN_ARM_IO			GD32F450_PE2_PIN_NUM
#define	FPGA_TO_ARM1_INT	GD32F450_PA2_PIN_NUM

#define	ARM1_TO_ARM2_SYN gd32_pin_write(SYN_ARM_IO, PIN_LOW)


#define GNSS_BUFFER_SIZE            1000
#define IMU_BUFFER_SIZE             100

#define TASK_GNSS_COMM2_BIT ( 1 << 0 )
#define TASK_GNSS_COMM3_BIT ( 1 << 1 )
#define TASK_IMU_BIT 		( 1 << 2 )
#define ALL_SYNC_BITS 		( TASK_GNSS_COMM2_BIT | TASK_GNSS_COMM3_BIT | TASK_IMU_BIT )

/* SRAM */
uint8_t fpgaBuf[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
static uint8_t fpga_comm2_rxbuffer[GNSS_BUFFER_SIZE];
static uint8_t fpga_comm2_parse_rxbuffer[GNSS_BUFFER_SIZE];
static uint16_t gnss_comm2_len = 0;
static uint16_t gnss_comm2_parse_len = 0;

static uint8_t fpga_comm3_rxbuffer[GNSS_BUFFER_SIZE];
static uint8_t fpga_comm3_parse_rxbuffer[GNSS_BUFFER_SIZE];
static uint16_t gnss_comm3_len = 0;
static uint16_t gnss_comm3_parse_len = 0;

static uint8_t fpga_comm5_rxbuffer[IMU_BUFFER_SIZE];
static uint16_t imu_comm5_len = 0;

TaskHandle_t task_start_handler;
TaskHandle_t task_gnss_comm2_handler, task_gnss_comm3_handler, task_imu_comm5_handler;
SemaphoreHandle_t xGnssComm2Semaphore = NULL;
SemaphoreHandle_t xGnssComm3Semaphore = NULL;
SemaphoreHandle_t xImuComm5Semaphore  = NULL;

EventGroupHandle_t xFwdogEventGroup = NULL;


uint8_t fpga_syn = 0;//fpga同步


//uint8_t fpga_dram_wr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
//uint8_t fpga_dram_rd[10];

static void prvSetupHardware( void )
{
    SystemInit();

    SystemCoreClockUpdate();

    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    systick_config();

    #ifdef	configUse_SEGGER_RTT
    SEGGER_RTT_Init();
    SEGGER_RTT_printf(0, "\r\nCK_SYS is %d", rcu_clock_freq_get(CK_SYS));
    SEGGER_RTT_printf(0, "\r\nCK_AHB is %d", rcu_clock_freq_get(CK_AHB));
    SEGGER_RTT_printf(0, "\r\nCK_APB1 is %d", rcu_clock_freq_get(CK_APB1));
    SEGGER_RTT_printf(0, "\r\nCK_APB2 is %d", rcu_clock_freq_get(CK_APB2));
    #endif
}
#if 0
/////////////////////////////power down///////////////////////////////////////////
void sleep_int_install(void)
{
//    uint8_t i;
    irq_priority priority =
    {
        .nvic_irq_pre_priority = 0,
        .nvic_irq_sub_priority = 0
    };

    pin_irq_install(  SYN_ARM_IO, PIN_MODE_INPUT_PULLDOWN,
                      PIN_IRQ_MODE_RISING,
                      NULL,
                      NULL,
                      &priority);
}

void sleep_int_uninstall(void)
{

    gd32_pin_irq_enable(SYN_ARM_IO, PIN_IRQ_DISABLE);
}

void system_config_before_sleep(void)
{
    NVIC_DisableIRQ(SysTick_IRQn);
    timer_disable(TIMER0);
    timer_disable(TIMER1);
    timer_disable(TIMER13);
    //adc_disable(ADC0);
}

void system_config_after_wakeup(void)
{

    prvSetupHardware();


}

void system_halt(void)
{
    system_config_before_sleep();
    sleep_int_install();
    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);
    sleep_int_uninstall();
    system_config_after_wakeup();

}
#endif
void gnss_comm2_rx(void)
{
    if(gnss_comm2_len >= (GNSS_BUFFER_SIZE - 100))//防止越界
    {
        gnss_comm2_len = 0;
    }

    gnss_comm2_len += Uart_RecvMsg(UART_RXPORT_COMPLEX_9, 100, fpga_comm2_rxbuffer + gnss_comm2_len);

    if(gnss_comm2_len >= 900)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        xSemaphoreGiveFromISR( xGnssComm2Semaphore, &xHigherPriorityTaskWoken );

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }

}

void gnss_comm2_parse(uint8_t* pData, uint16_t dataLen)
{
    uint8_t temp, *p = pData;

    do
    {
        temp = *p++;
        dataLen--;

        if(('$' == temp) || ('#' == temp))
        {
            gnss_comm2_parse_len = 0;
            fpga_comm2_parse_rxbuffer[gnss_comm2_parse_len] = temp;
            gnss_comm2_parse_len++;
        }
        else if(0x0a == temp)
        {
            if(fpga_comm2_parse_rxbuffer[gnss_comm2_parse_len - 1] == 0x0d)//回车换行结尾
            {
                fpga_comm2_parse_rxbuffer[gnss_comm2_parse_len] = temp;
                gnss_comm2_parse_len++;

                if((0 == strncmp("$GNRMC", (char const *)fpga_comm2_parse_rxbuffer, 6))                        
                        || (0 == strncmp("#AGRICA", (char const *)fpga_comm2_parse_rxbuffer, 7))
                        || (0 == strncmp("#BESTPOSA", (char const *)fpga_comm2_parse_rxbuffer, 9))
                        || (0 == strncmp("$GNTRA", (char const *)fpga_comm2_parse_rxbuffer, 6))
                  )
                {
                #ifdef configUse_debug
                    Uart_SendMsg(UART_TXPORT_COMPLEX_8, 0, gnss_comm2_parse_len, fpga_comm2_parse_rxbuffer);
                #endif
                	frame_fill_gnss(fpga_comm2_parse_rxbuffer, gnss_comm2_parse_len);
                    gnss_comm2_parse_len = 0;
                }
            }
        }
        else
        {
            fpga_comm2_parse_rxbuffer[gnss_comm2_parse_len] = temp;
            gnss_comm2_parse_len++;
        }
    }
    while(dataLen > 0);

    gnss_comm2_parse_len = 0;
    gnss_comm2_len = 0;
}

void gnss_comm3_rx(void)
{

    if(gnss_comm3_len >= (GNSS_BUFFER_SIZE - 100))//防止越界
    {
        gnss_comm3_len = 0;
    }

    gnss_comm3_len += Uart_RecvMsg(UART_RXPORT_COMPLEX_10, 100, fpga_comm3_rxbuffer + gnss_comm3_len);

    if(gnss_comm3_len >= 500)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        xSemaphoreGiveFromISR( xGnssComm3Semaphore, &xHigherPriorityTaskWoken );

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

void gnss_comm3_parse(uint8_t* pData, uint16_t dataLen)
{
    uint8_t temp, *p = pData;

    do
    {
        temp = *p++;
        dataLen--;

        if(('$' == temp) || ('#' == temp))
        {
            gnss_comm3_parse_len = 0;
            fpga_comm3_parse_rxbuffer[gnss_comm3_parse_len] = temp;
            gnss_comm3_parse_len++;
        }
        else if(0x0a == temp)
        {
            if(fpga_comm3_parse_rxbuffer[gnss_comm3_parse_len - 1] == 0x0d)//回车换行结尾
            {
                fpga_comm3_parse_rxbuffer[gnss_comm3_parse_len] = temp;
                gnss_comm3_parse_len++;

                if((0 == strncmp("$GNGGA", (char const *)fpga_comm3_parse_rxbuffer, 6))
                        || (0 == strncmp("$GNVTG", (char const *)fpga_comm3_parse_rxbuffer, 6))
                        || (0 == strncmp("$GNZDA", (char const *)fpga_comm3_parse_rxbuffer, 6))
                        || (0 == strncmp("$GNTRA", (char const *)fpga_comm3_parse_rxbuffer, 6))
                        || (0 == strncmp("#HEADINGA", (char const *)fpga_comm3_parse_rxbuffer, 9))
                  )
                {
                #ifdef configUse_debug
                    Uart_SendMsg(UART_TXPORT_COMPLEX_8, 0, gnss_comm3_parse_len, fpga_comm3_parse_rxbuffer);
                #endif
                	frame_fill_gnss(fpga_comm3_parse_rxbuffer, gnss_comm3_parse_len);
                    gnss_comm3_parse_len = 0;
                }
            }
        }
        else
        {
            fpga_comm3_parse_rxbuffer[gnss_comm3_parse_len] = temp;
            gnss_comm3_parse_len++;
        }
    }
    while(dataLen > 0);

    gnss_comm3_parse_len = 0;
    gnss_comm3_len = 0;
}

void imu_comm5_rx(void)
{

    imu_comm5_len = Uart_RecvMsg(UART_RXPORT_COMPLEX_12, IMU_BUFFER_SIZE, fpga_comm5_rxbuffer);

    if(imu_comm5_len > 45)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        xSemaphoreGiveFromISR( xImuComm5Semaphore, &xHigherPriorityTaskWoken );

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }

}

//////////////////////////////////////////////////////////////////////////////////
void fpga_int_hdr(void *args)
{
    fpga_syn = 1;

    imu_comm5_rx();

    gnss_comm2_rx();

    gnss_comm3_rx();
}

void arm_syn_int_hdr(void *args)
{

}

void gpio_init(void)
{
    irq_priority priority =
    {
        .nvic_irq_pre_priority = 4,
        .nvic_irq_sub_priority = 0
    };

    //gd32_pin_mode(ARM1_TO_ARM2_IO, PIN_MODE_OUTPUT);
    gd32_pin_mode(SYN_ARM_IO, PIN_MODE_OUTPUT);

    pin_irq_install(  FPGA_TO_ARM1_INT, PIN_MODE_INPUT_PULLDOWN,
                      PIN_IRQ_MODE_RISING,
                      fpga_int_hdr,
                      NULL,
                      &priority);
    pin_irq_install(  ARM1_TO_ARM2_IO, PIN_MODE_INPUT,
                      PIN_IRQ_MODE_RISING_FALLING,
                      arm_syn_int_hdr,
                      NULL,
                      &priority);
}

static void peripherals_init(void)
{
    exmc_sram_init();

    // Uart
    //Uart_TxInit(UART_TXPORT_RS232_1, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_DEFAULT, UART_ENABLE);
    //Uart_RxInit(UART_RXPORT_RS232_1, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_DEFAULT, UART_ENABLE);

    Uart_TxInit(UART_TXPORT_COMPLEX_8, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS422, UART_ENABLE);
    Uart_RxInit(UART_RXPORT_COMPLEX_8, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS422, UART_ENABLE);

    Uart_TxInit(UART_TXPORT_COMPLEX_9, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS232, UART_ENABLE);
    Uart_RxInit(UART_RXPORT_COMPLEX_9, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS232, UART_ENABLE);

    Uart_TxInit(UART_TXPORT_COMPLEX_10, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS232, UART_ENABLE);
    Uart_RxInit(UART_RXPORT_COMPLEX_10, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS232, UART_ENABLE);

    Uart_TxInit(UART_TXPORT_COMPLEX_11, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS232, UART_ENABLE);
    Uart_RxInit(UART_RXPORT_COMPLEX_11, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS232, UART_ENABLE);

    Uart_TxInit(UART_TXPORT_COMPLEX_12, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS232, UART_ENABLE);
    Uart_RxInit(UART_RXPORT_COMPLEX_12, UART_BAUDRATE_115200BPS, UART_PARITY_NONE, UART_STOPBIT_ONE, UART_RS232, UART_ENABLE);

    frame_init();

    gd32_hw_usart_init();

    gpio_init();

    gd32_timer_init();

    rtc_configuration();

//    fwdog_init();
}

void gnss_comm2_task(void* arg)
{

    for( ;; )
    {
        if(pdTRUE == xSemaphoreTake( xGnssComm2Semaphore, portMAX_DELAY))//pdMS_TO_TICKS(100)
        {
            gnss_comm2_parse(fpga_comm2_rxbuffer, gnss_comm2_len); //send to fpga
            gnss_comm2_len = 0;
            xEventGroupSetBits(
                xFwdogEventGroup, /* The event group being updated. */
                TASK_GNSS_COMM2_BIT);

            ARM1_TO_ARM2_SYN;	//syn call
        }

    }
}

void gnss_comm3_task(void* arg)
{

    for( ;; )
    {
        if(pdTRUE == xSemaphoreTake( xGnssComm3Semaphore, portMAX_DELAY))//pdMS_TO_TICKS(100)
        {
            gnss_comm3_parse(fpga_comm3_rxbuffer, gnss_comm3_len); //send to fpga
            xEventGroupSetBits(
                xFwdogEventGroup, /* The event group being updated. */
                TASK_GNSS_COMM3_BIT);

            ARM1_TO_ARM2_SYN;	//syn call
        }

    }
}

void imu_comm5_task(void* arg)
{
    uint8_t ucTimeOneSecondChangeSecond;

    for( ;; )
    {
        if(pdTRUE == xSemaphoreTake( xImuComm5Semaphore, portMAX_DELAY))//pdMS_TO_TICKS(100)
        {
            frame_fill_imu(fpga_comm5_rxbuffer, imu_comm5_len); //send to fpga

            if(ucTimeOneSecondChangeSecond != RSOFT_RTC_SECOND)
            {
                ucTimeOneSecondChangeSecond = RSOFT_RTC_SECOND;
                #ifdef configUse_debug
                //Uart_SendMsg(UART_TXPORT_COMPLEX_8, 0, imu_comm5_len, fpga_comm5_rxbuffer);
                #endif
            }

            xEventGroupSetBits(
                xFwdogEventGroup, /* The event group being updated. */
                TASK_IMU_BIT);

        }
    }
}

void fwdog_task(void* arg)
{
    EventBits_t uxBits;
    const TickType_t xTicksToWait = pdMS_TO_TICKS( 100 );

    while(1)
    {
        uxBits = xEventGroupWaitBits(
                     xFwdogEventGroup,
                     ALL_SYNC_BITS,
                     pdTRUE,
                     pdTRUE,
                     xTicksToWait );

        if( uxBits & ALL_SYNC_BITS == ALL_SYNC_BITS )
        {
            fwdog_feed();
        }
    }
}

void start_task(void* arg)
{
    taskENTER_CRITICAL();

    xTaskCreate( gnss_comm2_task,
                 "gnss_comm2_task",
                 configMINIMAL_STACK_SIZE,
                 ( void * ) NULL,
                 tskIDLE_PRIORITY + 6,
                 (TaskHandle_t*)&task_gnss_comm2_handler );
    xTaskCreate( gnss_comm3_task,
                 "gnss_comm3_task",
                 configMINIMAL_STACK_SIZE,
                 ( void * ) NULL,
                 tskIDLE_PRIORITY + 5,
                 (TaskHandle_t*)&task_gnss_comm3_handler );
    xTaskCreate( imu_comm5_task,
                 "imu_comm5_task",
                 configMINIMAL_STACK_SIZE,
                 ( void * ) NULL,
                 tskIDLE_PRIORITY + 4,
                 (TaskHandle_t*)&task_imu_comm5_handler );
//    xTaskCreate( fwdog_task,
//                 "fwdog_task",
//                 configMINIMAL_STACK_SIZE,
//                 ( void * ) NULL,
//                 tskIDLE_PRIORITY + 3,
//                 NULL);
	xTaskCreate( nav_task,
                 "nav_task",
                 configMINIMAL_STACK_SIZE,
                 ( void * ) NULL,
                 tskIDLE_PRIORITY + 7,
                 NULL );
    vTaskDelete(task_start_handler);

    taskEXIT_CRITICAL();
}
#if 0
void UpdateCompensate(void)
{
    COMPENSATE_PARAMS *pCompensate = NULL;

    pCompensate	=	GetCompensateParmsPointer();

    if(NULL == pCompensate)
    {
        inav_log(LOG_ERR, "pCompensate is NULL, UpdateCompensate fail");
    }

    //zhang
}

void match_imu_data(IMUDATA *data, MIX_DATA_TypeDef* mixd)
{
    data->accm[0] = mixd->accelGrp[0];
    data->accm[1] = mixd->accelGrp[1];
    data->accm[2] = mixd->accelGrp[2];

    data->gyro[0] = mixd->gyroGrp[0];
    data->gyro[1] = mixd->gyroGrp[1];
    data->gyro[2] = mixd->gyroGrp[2];

    data->roll 	  = mixd->roll;
    data->pitch   = mixd->pitch;
    data->heading = mixd->azimuth;

    data->second  = mixd->gps_sec;
}

void match_gnss_data(I_NAV_GNSS_RESULT  * data, ARM1_TO_KALAM_MIX_TypeDef* mixd)
{
    data->gpssecond = mixd->GPS_Sec;
    data->heading = mixd->Heading;
    data->pitch = mixd->pitch;
    data->roll = mixd->roll;
    data->latitude 	= mixd->lat;
    data->longitude = mixd->lon;
    data->altitude 	= mixd->alt;
    data->vn = mixd->vN;
    data->ve = mixd->vE;
    data->vu = mixd->vZ;
    data->gnssstatus = mixd->rtk_status;
    data->latstd = mixd->sigma_lat;
    data->logstd = mixd->sigma_lon;
    data->hstd = mixd->sigma_alt;
    data->vnstd = mixd->sigma_vn;
    data->vestd = mixd->sigma_ve;
    data->vustd = mixd->sigma_vz;

}

void UpdateGnssAndInsData(void)
{
    IMUDATA *pImuData = NULL;
    I_NAV_GNSS_RESULT *pGnssResult = NULL;
    MIX_DATA_TypeDef* pMix = frame_get_ptr();
    ARM1_TO_KALAM_MIX_TypeDef*  pGnss = gnss_get_algorithm_dataPtr();
    pImuData = GetNavIncImuPointer();

    if(NULL != pImuData)
    {
        //save last time data
        pImuData->gyro_pre[0] 	=	pImuData->gyro[0];
        pImuData->gyro_pre[1] 	=	pImuData->gyro[1];
        pImuData->gyro_pre[2] 	=	pImuData->gyro[2];
        pImuData->accm_pre[0] 	=	pImuData->accm[0];
        pImuData->accm_pre[1] 	=	pImuData->accm[1];
        pImuData->accm_pre[2] 	=	pImuData->accm[2];
        //zhang write fpga data to pImuData
//        inav_log(LOG_DEBUG, "update imu data from fpga data");
        match_imu_data(pImuData, pMix);
    }
    else
    {
        inav_log(LOG_ERR, "pImuData is NULL");
    }

    pGnssResult = GetGnssResultPointer();

    if(NULL != pGnssResult)
    {
        //zhang  write fpga data to pGnssResult
//        inav_log(LOG_DEBUG, "update gnss result from fpga data");
        match_gnss_data(pGnssResult, pGnss);
    }
    else
    {
        inav_log(LOG_ERR, "pGnssResult is NULL");
    }
}
#endif

int main(void)
{
    hw_interrupt_disable();

    xGnssComm2Semaphore = xSemaphoreCreateBinary();
    configASSERT( xGnssComm2Semaphore );
    xGnssComm3Semaphore = xSemaphoreCreateBinary();
    configASSERT( xGnssComm3Semaphore );
    xImuComm5Semaphore = xSemaphoreCreateBinary();
    configASSERT( xImuComm5Semaphore );    
    xnvaTaskSemaphore = xSemaphoreCreateBinary();
    configASSERT( xnvaTaskSemaphore );
    xFwdogEventGroup = xEventGroupCreate();
    configASSERT( xFwdogEventGroup );
    prvSetupHardware();
    peripherals_init();
#ifdef configUse_debug
    dbg_periph_enable(DBG_FWDGT_HOLD);
    dbg_periph_enable(DBG_WWDGT_HOLD);
#endif
    xTaskCreate( start_task,
                 "start_task",
                 configMINIMAL_STACK_SIZE,
                 ( void * ) NULL,
                 tskIDLE_PRIORITY + 1,
                 (TaskHandle_t*)&task_start_handler );

    /* Start the scheduler. */
    vTaskStartScheduler();

    for(;;);
}

/**
  * @}
  */

void vApplicationIdleHook( void )
{

    comm_handle();
    rtc_update();
}

void vApplicationTickHook( void )
{

}

void OS_PreSleepProcessing(uint32_t ulExpectedIdleTime)
{


}

void OS_PostSleepProcessing(uint32_t ulExpectedIdleTime)
{


}

void vApplicationMallocFailedHook( void )
{

    taskDISABLE_INTERRUPTS();

    for( ;; );
}

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();

    for( ;; );
}

/* retarget the C library printf function to the USART */
int fputc(int ch, FILE *f)
{
    usart_data_transmit(UART4, (uint8_t)ch);

    while(RESET == usart_flag_get(UART4, USART_FLAG_TBE));

    return ch;
}

