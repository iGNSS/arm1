
#include "string.h"
#include "frame_analysis.h"
//#include "m_queue.h"
#include "serial.h"
#include "exmc_sram.h"
#include "uartadapter.h"
#include "gnss.h"
#include "DRamAdapter.h"
#include "computerFrameParse.h"
#include "inavins.h"
#include "inavgnss.h"


//static uint8_t m_uart_queue_buffer[USART_SERIAL_RB_BUFSZ];
//static uint8_t m_uart_rx_buffer[USART_SERIAL_RB_BUFSZ];
//pQueue	m_queue = NULL;

RS422_FRAME_DEF	rs422_frame;
MIX_DATA_TypeDef mixData;


uint8_t xor_check(uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;
    uint16_t x = 0;

    for(; i < len; i++)
    {
        x = x ^ (*(buf + i));
    }

    return x;
}

uint8_t sum_check(uint8_t *buf, uint16_t len )
{
    uint8_t checksum = 0, i;

    for (i = 0; i < len; i++)
    {
        checksum += buf[i];
    }

    checksum  = ~checksum;

    return checksum;

}
//参考PA-IMU-460.PDF
#define IMU_FRAME_HEADER_LSB                  0x7F
#define IMU_FRAME_HEADER_MSB                  0x80
IMU_DATA_TypeDef imu_info;
extern void Oscilloscope(RS422_FRAME_DEF* rs422);
uint8_t frame_fill_imu(uint8_t* pData, uint16_t dataLen)
{
#define	Accel_Scale 	20.0
#define	Rate_Scale 		1260.0
#define	Angle_Scale 	360.0
#define	Temp_Scale		200.0
#define	Sensor_Scale 	65536.0


    uint8_t  sum, calSum;
    uint8_t  data_l, data_h;
    //double	 sensors[10];
    uint8_t  length = 0;
    uint8_t  ret = INS_ERROR;
    uint8_t  *p = pData;

    while( 1 )
    {
        data_l = *p++;
        data_h = *p++;
        length ++;

        if((IMU_FRAME_HEADER_LSB == data_l) && (IMU_FRAME_HEADER_MSB == data_h))
        {
            sum = sum_check(p, 20 );
            calSum = *(p + 20);

            if(sum == calSum)
            {
                memcpy((void*)&imu_info.syn_low, p-2, sizeof(IMU_DATA_TypeDef));
                //送至上位机
                //gd32_usart_write(&imu_info.syn_low, sizeof(IMU_DATA_TypeDef));
                
                rs422_frame.data_stream.accelX = *p++;
                rs422_frame.data_stream.accelX |= (*p++) << 8;
                //rs422_frame.data_stream.accelX = rs422_frame.data_stream.accelX * Accel_Scale / Sensor_Scale;
                mixData.accelGrp[0] = rs422_frame.data_stream.accelX * Accel_Scale / Sensor_Scale;
                rs422_frame.data_stream.accelY = *p++;
                rs422_frame.data_stream.accelY |= (*p++) << 8;
                //rs422_frame.data_stream.accelY = rs422_frame.data_stream.accelY * Accel_Scale / Sensor_Scale;
                mixData.accelGrp[1] = rs422_frame.data_stream.accelX * Accel_Scale / Sensor_Scale;
                rs422_frame.data_stream.accelZ = *p++;
                rs422_frame.data_stream.accelZ |= (*p++) << 8;
                //rs422_frame.data_stream.accelZ = rs422_frame.data_stream.accelZ * Accel_Scale / Sensor_Scale;
                mixData.accelGrp[2] = rs422_frame.data_stream.accelX * Accel_Scale / Sensor_Scale;

                rs422_frame.data_stream.gyroX = *p++;
                rs422_frame.data_stream.gyroX |= (*p++) << 8;
                //rs422_frame.data_stream.gyroX = rs422_frame.data_stream.gyroX * Rate_Scale / Sensor_Scale;
                mixData.gyroGrp[0] = rs422_frame.data_stream.gyroX * Rate_Scale / Sensor_Scale;
                rs422_frame.data_stream.gyroY = *p++;
                rs422_frame.data_stream.gyroY |= (*p++) << 8;
                //rs422_frame.data_stream.gyroY = rs422_frame.data_stream.gyroY * Rate_Scale / Sensor_Scale;
                mixData.gyroGrp[1] = rs422_frame.data_stream.gyroX * Rate_Scale / Sensor_Scale;
                rs422_frame.data_stream.gyroZ = *p++;
                rs422_frame.data_stream.gyroZ |= (*p++) << 8;
                //rs422_frame.data_stream.gyroZ = rs422_frame.data_stream.gyroZ * Rate_Scale / Sensor_Scale;
                mixData.gyroGrp[2] = rs422_frame.data_stream.gyroX * Rate_Scale / Sensor_Scale;

                rs422_frame.data_stream.roll = *p++;
                rs422_frame.data_stream.roll |= (*p++) << 8;
                rs422_frame.data_stream.roll = rs422_frame.data_stream.roll * Angle_Scale / Sensor_Scale;
                rs422_frame.data_stream.pitch = *p++;
                rs422_frame.data_stream.pitch |= (*p++) << 8;
                rs422_frame.data_stream.pitch = rs422_frame.data_stream.pitch * Angle_Scale / Sensor_Scale;
                rs422_frame.data_stream.azimuth = *p++;
                rs422_frame.data_stream.azimuth |= (*p++) << 8;
                rs422_frame.data_stream.azimuth = rs422_frame.data_stream.azimuth * Angle_Scale / Sensor_Scale;

                rs422_frame.data_stream.poll_frame.data1 = *p++;
                rs422_frame.data_stream.poll_frame.data1 |= (*p++) << 8;
                rs422_frame.data_stream.poll_frame.type = dev_inter_temp;
                rs422_frame.data_stream.poll_frame.data1 = rs422_frame.data_stream.poll_frame.data1 * Temp_Scale / Sensor_Scale;
                ret = INS_EOK;
                
                break;
            }
            
        }

        if(length > dataLen)break;
    }

#undef	Accel_Scale
#undef	Rate_Scale
#undef	Angle_Scale
#undef	Temp_Scale
#undef	Sensor_Scale
  //Oscilloscope(&rs422_frame);
    return ret;
}

void frame_fill_gnss(uint8_t* pData, uint16_t dataLen)
{
    if(INS_EOK == gnss_parse(pData, dataLen))
    {
        gnss_fill_rs422(&rs422_frame);
    }
}

//数据封包
void frame_pack_and_send(uint8_t* pData, uint16_t dataLen)
{
    uint8_t xor;
    uint16_t len;
    uint16_t flg = 1;

    frame_fill_imu(pData, dataLen);
    frame_fill_gnss(pData, dataLen);
    rs422_frame.data_stream.header[0] = RS422_FRAME_HEADER_L;
    rs422_frame.data_stream.header[1] = RS422_FRAME_HEADER_M;
    rs422_frame.data_stream.header[2] = RS422_FRAME_HEADER_H;

    xor = xor_check(rs422_frame.data_stream.header, sizeof(rs422_frame.data_stream) - 6 );
    rs422_frame.data_stream.xor_verify1 = xor;

    xor = xor_check(rs422_frame.data_stream.header, sizeof(rs422_frame.data_stream) - 1 );
    rs422_frame.data_stream.xor_verify2 = xor;

    len = sizeof(rs422_frame.data_stream);
    //exmc_sram_writebuffer_8(rs422_frame.fpga_cache, WRITE_READ_ADDR, BUFFER_SIZE, BANK0_REGION1);
    Uart_SendMsg(UART_TXPORT_COMPLEX_8, 0, len, rs422_frame.fpga_cache);
    DRam_Write(0, (uint16_t*)rs422_frame.fpga_cache, RS422_FRAME_LENGTH / 2); //同时写入ram
    DRam_Write(51, &flg, 1);

    if(frame_setting_is_update())
    {
        DRam_Write(100, (uint16_t*)&hSetting.firstProgram, RS422_FRAME_LENGTH / 2);
        DRam_Write(151, &flg, 1);
    }
}

void frame_init(void)
{
    memset(rs422_frame.fpga_cache, 0x0, sizeof(rs422_frame.fpga_cache));
}


MIX_DATA_TypeDef* frame_get_ptr(void)
{

    return &mixData;
}

