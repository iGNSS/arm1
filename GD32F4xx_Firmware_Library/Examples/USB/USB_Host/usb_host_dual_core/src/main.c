/*!
    \file    main.c
    \brief   this file realizes HID host(usbfs) and MSC host(usbhs)

    \version 2020-08-01, V3.0.0, firmware for GD32F4xx
    \version 2022-03-09, V3.1.0, firmware for GD32F4xx
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

#include "drv_usb_hw.h"
#include "drv_usb_core.h"
#include "usbh_msc_usr.h"
#include "usbh_msc_core.h"
#include "usbh_hid_usr.h"
#include "usbh_hid_core.h"

usbh_host usb_host_hid;
usbh_host usb_host_msc;

usb_core_driver usbfs_core;
usb_core_driver usbhs_core;

/*!
    \brief      main routine
    \param[in]  none
    \param[out] none
    \retval     int
*/
int main(void)
{
    gd_eval_com_init(EVAL_COM0);

    usb_gpio_config();
    usb_rcu_config();
    usb_timer_init();

    /* configure GPIO pin used for switching VBUS power and charge pump I/O */
    usb_vbus_config();

    /* register USBFS port device class */
    usbh_class_register(&usb_host_hid, &usbh_hid);

    usbh_init(&usb_host_hid,
              &usbfs_core,
              USB_CORE_ENUM_FS,
              &hid_usr_cb);

    /* register USBHS port device class */
    usbh_class_register(&usb_host_msc, &usbh_msc);

    usbh_init(&usb_host_msc,
              &usbhs_core,
              USB_CORE_ENUM_HS,
              &msc_usr_cb);

    /* enable interrupts */
    usb_intr_config();

    while(1){
        /* Host state handler */
        usbh_core_task(&usb_host_msc);

        /* Host state handler */
        usbh_core_task(&usb_host_hid);
    }
}

/* retarget the C library printf function to the USART */
int fputc(int ch, FILE *f)
{
    usart_data_transmit(EVAL_COM0, (uint8_t)ch);
    while(RESET == usart_flag_get(EVAL_COM0, USART_FLAG_TBE));
    return ch;
}
