#ifndef __FRAME_ANALYSIS_H__
#define __FRAME_ANALYSIS_H__

#include "gd32f4xx.h"
#include "insdef.h"



#define RS422_FRAME_HEADER_L                  0xBD
#define RS422_FRAME_HEADER_M                  0xDB
#define RS422_FRAME_HEADER_H                  0x0B

#define	RS422_FRAME_LENGTH	100


typedef enum poll_data_type
{
	locating_info_prec = 0,
	speed_info_prec = 1,
	pos_info_prec = 2,
	dev_inter_temp = 22,
	gps_status     = 32,
	rotate_status = 33
}POLL_DATA_TypeDef;

typedef struct poll_data
{
    uint16_t	data1;
    uint16_t 	data2;
    uint16_t	data3;
    uint32_t	gps_time;
    uint8_t		type;
} POLL_DATA, pPOLL_DATA;

typedef union rs422_frame_define
{
    struct
    {
        uint8_t 			header[3];	//0xbd,0xdb,0x0b
        uint16_t 			roll;		//横滚角
        uint16_t 			pitch;		//俯仰角
        uint16_t			azimuth;	//方位角
        uint16_t 			gyroX;		//陀螺x轴
        uint16_t 			gyroY;		//陀螺y轴
        uint16_t			gyroZ;		//陀螺z轴
        uint16_t 			accelX;		//加表x轴
        uint16_t 			accelY;		//加表y轴
        uint16_t			accelZ;		//加表z轴
        uint16_t			latitude;	//纬度
        uint16_t			longitude;	//经度
        uint16_t			altitude;	//高度
        uint16_t			speed_N;	//北向速度
        uint16_t			speed_E;	//东向速度
        uint16_t			speed_G;	//地向速度
        uint8_t				status;
        struct poll_data	poll_frame;
        uint8_t				xor_verify1;
        uint32_t			gps_week;
        uint8_t				xor_verify2;

    } data_stream;
    uint8_t fpga_cache[RS422_FRAME_LENGTH];
} RS422_FRAME_DEF, *pRS422_FRAME_DEF;

typedef struct
{
	float accelGrp[3];
	float gyroGrp[3];
	float roll;
	float pitch;
	float azimuth;
	float latitude;
	float longitude;
	float altitude;
	float gps_sec;//周内秒
}MIX_DATA_TypeDef;

void frame_pack_and_send(uint8_t* pData, uint16_t dataLen);
void frame_init(void);
void frame_fill_gnss(uint8_t* pData, uint16_t dataLen);

MIX_DATA_TypeDef* frame_get_ptr(void);


#endif

