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
#include "public.h"
#include "pin_numbers_def.h"
#include "frame_analysis.h"
#include "uartadapter.h"
#include "gnss.h"
#include "taskschedule.h"
#include "computerFrameParse.h"

//#include "fmc_operation.h"
#include "DRamAdapter.h"


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

/* SRAM */
uint8_t fpgaBuf[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
uint8_t fpga_comm2_rxbuffer[BUFFER_SIZE];
uint8_t fpga_comm3_rxbuffer[BUFFER_SIZE];

uint8_t fpga_syn = 0;//fpga同步


uint8_t fpga_dram_wr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
uint8_t fpga_dram_rd[10];

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

EventStatus system_1s_task(void)
{

    rtc_update();
    return ENABLE;
}

static uint16_t gnss_comm2_len = 0;
static uint16_t gnss_comm3_len = 0;

uint8_t  gnss_comm2_valid = 0;
uint8_t  gnss_comm3_valid = 0;

extern int cmdKind;

void gnss_comm2_rx(void)
{
	uint8_t temp;
	
	if(gnss_comm2_len >= BUFFER_SIZE)
    {
        gnss_comm2_len = 0;
    }

    Uart_RecvMsg(UART_RXPORT_COMPLEX_9, 1, &temp);

    if('$' == temp)
    {
        gnss_comm2_len = 0;
        fpga_comm2_rxbuffer[gnss_comm2_len] = temp;
        gnss_comm2_len++;
    }
    else if(0x0a == temp)
    {
    	if(fpga_comm2_rxbuffer[gnss_comm2_len - 1] == 0x0d)//回车换行结尾
    	{
	        fpga_comm2_rxbuffer[gnss_comm2_len] = temp;
	        gnss_comm2_len++;

	        if((0 == strncmp("$GPRMC", (char const *)fpga_comm2_rxbuffer, 6))
	                || (0 == strncmp("$GNRMC", (char const *)fpga_comm2_rxbuffer, 6))
	                || (0 == strncmp("$BDRMC", (char const *)fpga_comm2_rxbuffer, 6))
	                )
	        {
	            gnss_comm2_valid = 1;//数据有效
	            cmdKind = 2;
	        }
        }
    }
    else
    {
        fpga_comm2_rxbuffer[gnss_comm2_len] = temp;
        gnss_comm2_len++;
    }
}

void gnss_comm3_rx(void)
{
	uint8_t temp;
	
	if(gnss_comm3_len >= BUFFER_SIZE)
    {
        gnss_comm3_len = 0;
    }

    Uart_RecvMsg(UART_RXPORT_COMPLEX_10, 1, &temp);

    if('$' == temp)
    {
        gnss_comm3_len = 0;
        fpga_comm3_rxbuffer[gnss_comm3_len] = temp;
        gnss_comm3_len++;
    }
    else if(0x0a == temp)
    {
    	if(fpga_comm3_rxbuffer[gnss_comm3_len - 1] == 0x0d)//回车换行结尾
    	{
	        fpga_comm3_rxbuffer[gnss_comm3_len] = temp;
	        gnss_comm3_len++;

	        if((0 == strncmp("$GPGGA", (char const *)fpga_comm3_rxbuffer, 6))
	                || (0 == strncmp("$GNGGA", (char const *)fpga_comm3_rxbuffer, 6))
	                || (0 == strncmp("$BDGGA", (char const *)fpga_comm3_rxbuffer, 6))
	                || (0 == strncmp("$GPVTG", (char const *)fpga_comm3_rxbuffer, 6))
	                || (0 == strncmp("$GNVTG", (char const *)fpga_comm3_rxbuffer, 6))
	                || (0 == strncmp("$BDVTG", (char const *)fpga_comm3_rxbuffer, 6))
	                || (0 == strncmp("$GPZDA", (char const *)fpga_comm3_rxbuffer, 6))
	                || (0 == strncmp("$GNZDA", (char const *)fpga_comm3_rxbuffer, 6))
	                || (0 == strncmp("$BDZDA", (char const *)fpga_comm3_rxbuffer, 6))
	                )
	        {
	            gnss_comm3_valid = 1;//数据有效
	            cmdKind = 2;
	        }
        }
    }
    else
    {
        fpga_comm3_rxbuffer[gnss_comm3_len] = temp;
        gnss_comm3_len++;
    }
}

//////////////////////////////////////////////////////////////////////////////////
void fpga_int_hdr(void *args)
{
    fpga_syn = 1;

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
        .nvic_irq_pre_priority = 0,
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

    //gd32_timer_init();

    //rtc_configuration();
}

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
    ARM1_TO_KALAM_MIX_TypeDef*  pGnss = gnss_get_mix_dataPtr();
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

void uart_send(void)
{
//	uint16_t len = 0;
    //Uart_SendMsg(UART_TXPORT_COMPLEX_8, 0, 10, fpgaBuf);
    //Uart_RecvMsg(UART_RXPORT_COMPLEX_8, 8, fpga_rxbuffer);

    //DRam_Write(0, (uint16_t*)fpga_dram_wr, 5); //同时写入ram
    //DRam_Read(0, (uint16_t*)fpga_dram_rd, 5);
//    len = Uart_RecvMsg(UART_RXPORT_COMPLEX_8, BUFFER_SIZE, fpga_rxbuffer);

//    if(len)
//    {
//        Uart_SendMsg(UART_TXPORT_COMPLEX_8, 0, len, fpga_rxbuffer);
//    }
}
extern void strtok_test(void);
/*!
    \brief    main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{

    prvSetupHardware();

    delay_1ms(20);

    __set_PRIMASK(1);
    peripherals_init();
    __set_PRIMASK(0);
    //Public_SetTestTimer(uart_send, PUBLICSECS(4));
    strtok_test();

    for(;;)
    {

        if(gnss_comm2_valid)
        {
            gnss_comm2_valid = 0;
            frame_fill_gnss(fpga_comm2_rxbuffer, gnss_comm2_len); //send to fpga
            Uart_SendMsg(UART_TXPORT_COMPLEX_8, 0, gnss_comm2_len, fpga_comm2_rxbuffer);
            ARM1_TO_ARM2_SYN;	//syn call
        }
		if(gnss_comm3_valid)
        {
            gnss_comm3_valid = 0;
            frame_fill_gnss(fpga_comm3_rxbuffer, gnss_comm3_len); //send to fpga
            Uart_SendMsg(UART_TXPORT_COMPLEX_8, 0, gnss_comm3_len, fpga_comm3_rxbuffer);
            ARM1_TO_ARM2_SYN;	//syn call
        }
        //            UpdateGnssAndInsData();
//            UpdateCompensate();
//            InterfaceKalman();


        #if defined(INS_USING_UART4_DMA0)
        comm_handle();	//上位机通信
        #endif
        TimerTaskScheduler();
    }
}

/* retarget the C library printf function to the USART */
int fputc(int ch, FILE *f)
{
    usart_data_transmit(UART4, (uint8_t)ch);

    while(RESET == usart_flag_get(UART4, USART_FLAG_TBE));

    return ch;
}

