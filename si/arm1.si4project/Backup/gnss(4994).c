#include "gnss.h"
#include "insdef.h"
#include "Time_Unify.h"
#include "arm_math.h"


GPSDataTypeDef hGPSData;						//GPS数据

GNSS_BestPos_DataTypeDef hGPSBestPosData;		//最佳位置
GNSS_Heading_DataTypeDef hGPSHeadingData;		//方位角信息
GNSS_ZDA_DataTypeDef hGPSZdaData;				//时间日期信息
GNSS_TRA_DataTypeDef hGPSTraData;				//方向角信息
GNSS_VTG_DataTypeDef hGPSVtgData;				//地面速度
GNSS_RMC_DataTypeDef hGPSRmcData;				//推荐定位信息
GNSS_GGA_DataTypeDef hGPSGgaData;				//定位信息
ARM1_TO_KALAM_MIX_TypeDef g_arm1MixData;

uint8_t gCmdIndex = 0;
uint8_t gCmdTypeIndex = 0;
int cmdKind = 0;


uint8_t gnss_dataIsOk(uint8_t *pBuffer, uint16_t BufferLen)
{
    uint16_t  i;
    uint16_t  j = 0;
    uint8_t  temp;
    uint8_t  sum = 0;
    uint16_t  count = 0;
    uint8_t  High;
    uint8_t  Low;
    uint8_t  verify;

    for(i = 1; i < BufferLen; i++)
    {
        temp = *(pBuffer + i);

        if((0x0a == temp) || (0x0d == temp))
        {
            break;
        }
        else if('*' == temp)
        {
            j = i;
            break;
        }
        else
        {
            sum ^= temp;

            if(',' == temp)
            {
                count++;
            }
        }
    }

    High = *(pBuffer + j + 1);
    Low = *(pBuffer + j + 2);

    if((High >= '0') && (High <= '9'))
    {
        High = High - 0x30;
    }
    else if((High >= 'A') && (High <= 'F'))
    {
        High = High - 0x37;
    }
    else
    {
        return 0;
    }

    if((Low >= '0') && (Low <= '9'))
    {
        Low = Low - 0x30;
    }
    else if((Low >= 'A') && (Low <= 'F'))
    {
        Low = Low - 0x37;
    }
    else
    {
        return INS_ERROR;
    }

    verify = (High << 4) | Low;

    if(verify == sum)
    {
        return INS_EOK;
    }
    else
    {
        return INS_ERROR;
    }
}

uint8_t gnss_gprmcIsLocation(uint8_t *pBuffer, uint8_t BufferLen)
{
    uint8_t  i;
    uint8_t  temp;
    uint8_t  count = 0;

    for(i = 0; i < BufferLen; i++)
    {
        temp = *(pBuffer + i);

        if(',' == temp)
        {
            count++;
        }
        else if('A' == temp)
        {
            if(2 == count)
            {
                return INS_EOK;
            }
        }
    }

    return INS_ERROR;
}

int GNSS_Cmd_Kind_Parser(char* pData, uint16_t dataLen)
{
    int cmdKind = 0;

    if(*pData == '#')
    {
        cmdKind = 1;
    }
    else if(*pData == '$')
    {
        cmdKind = 2;
    }

    return cmdKind;
}

int GNSS_Cmd_Parser(char* pData)
{
    if(0 == strncmp("$GPRMC", (char const *)pData, 6))
        gCmdTypeIndex = 1;
    else if(0 == strncmp("$GLRMC", (char const *)pData, 6))
        gCmdTypeIndex = 1;
    else if(0 == strncmp("$GNRMC", (char const *)pData, 6))
        gCmdTypeIndex = 1;
    else if(0 == strncmp("$GPGGA", (char const *)pData, 6))
        gCmdTypeIndex = 2;
    else if(0 == strncmp("$GLGGA", (char const *)pData, 6))
        gCmdTypeIndex = 2;
    else if(0 == strncmp("$GNGGA", (char const *)pData, 6))
        gCmdTypeIndex = 2;
    else if(0 == strncmp("$BDGGA", (char const *)pData, 6))
        gCmdTypeIndex = 2;
    else if(0 == strncmp("$GPVTG", (char const *)pData, 6))
        gCmdTypeIndex = 3;
    else if(0 == strncmp("$GLVTG", (char const *)pData, 6))
        gCmdTypeIndex = 3;
    else if(0 == strncmp("$GNVTG", (char const *)pData, 6))
        gCmdTypeIndex = 3;
    else if(0 == strncmp("$GPZDA", (char const *)pData, 6))
        gCmdTypeIndex = 4;
    else if(0 == strncmp("$GLZDA", (char const *)pData, 6))
        gCmdTypeIndex = 4;
    else if(0 == strncmp("$GNZDA", (char const *)pData, 6))
        gCmdTypeIndex = 4;
    else if(0 == strncmp("#HEADINGA", (char const *)pData, 9))
        gCmdTypeIndex = 5;
    else if(0 == strncmp("#BESTPOSA", (char const *)pData, 9))
        gCmdTypeIndex = 6;
    else if(0 == strncmp("$GPTRA", (char const *)pData, 6))
        gCmdTypeIndex = 7;
    else if(0 == strncmp("#AGRICA", (char const *)pData, 7))
        gCmdTypeIndex = 8;

    return gCmdTypeIndex;
}

void GNSS_Buff_Parser(char* pData, uint16_t nCmdIndex)
{
    switch(gCmdTypeIndex)
    {
        case 1:
            gnss_RMC_Buff_Parser(pData);
            break;

        case 2:
            gnss_GGA_Buff_Parser(pData);
            break;

        case 3:
            gnss_VTG_Buff_Parser(pData);
            break;

        case 4:
            gnss_ZDA_Buff_Parser(pData);
            break;

        case 5:
            gnss_Heading_Buff_Parser(pData);
            break;

        case 6:
            gnss_BESTPOS_Buff_Parser(pData);
            break;

        case 7:
            gnss_TRA_Buff_Parser(pData);
            break;

        case 8:
            gnss_AGRIC_Buff_Parser(pData);
            break;

        default:
            break;
    }
}

//推荐定位
int gnss_RMC_Buff_Parser(char* pData)
{
    switch(gCmdIndex)
    {
        case 0:			//字段0
            if((NULL != strstr(pData, "$GPRMC")) \
            ||(NULL != strstr(pData, "$GLRMC")) \
            ||(NULL != strstr(pData, "$GNRMC")))
            {

            }
            break;

        case 1:			//字段1		UTC时间
            //dtoc((double*)&hGPSRmcData.timestamp, (uint8_t*)pData, 3);
            hGPSRmcData.timestamp = atof(pData);
            hGPSRmcData.hour = ((uint32_t)hGPSRmcData.timestamp) / 10000;
            hGPSRmcData.minute = ((uint32_t)hGPSRmcData.timestamp) % 10000 / 100;
            hGPSRmcData.second = ((uint32_t)hGPSRmcData.timestamp) % 100;
            //gCmdIndex = 2;
            break;

        case 2:			//字段2		定位状态
            hGPSRmcData.valid = *pData;
            //gCmdIndex = 3;
            break;

        case 3:			//字段3		纬度
            //dtoc((double*)&hGPSRmcData.latitude, (uint8_t*)pData, 8);
            hGPSRmcData.latitude = atof(pData);
            //gCmdIndex = 4;
            break;

        case 4:			//字段4		纬度半球
            hGPSRmcData.LatHemisphere = *pData;
            //gCmdIndex = 5;
            break;

        case 5:			//字段5		经度
            //dtoc((double*)&hGPSRmcData.longitude, (uint8_t*)pData, 8);
            hGPSRmcData.longitude = atof(pData);
            //gCmdIndex = 6;
            break;

        case 6:			//字段6		经度半球
            hGPSRmcData.LonHemisphere = *pData;
            //gCmdIndex = 7;
            break;

        case 7:			//字段7		地面速率
            //dtoc((double*)&hGPSRmcData.rate, (uint8_t*)pData, 2);
            hGPSRmcData.rate = atof(pData);
            //gCmdIndex = 8;
            break;

        case 8:			//字段8		地面航向
            //dtoc((double*)&hGPSRmcData.courseAngle, (uint8_t*)pData, 2);
            hGPSRmcData.courseAngle = atof(pData);
            //gCmdIndex = 9;
            break;

        case 9:			//字段9		UTC日期
            hGPSRmcData.date = atol(pData);
            hGPSRmcData.day = hGPSRmcData.date / 10000;
            hGPSRmcData.month = hGPSRmcData.date % 10000 / 100;
            hGPSRmcData.year = hGPSRmcData.date % 100;
            //gCmdIndex = 10;
            break;

        case 10:		//字段10	磁偏角
            //dtoc((double*)&hGPSRmcData.magDeclination, (uint8_t*)pData, 2);
            hGPSRmcData.magDeclination = atof(pData);
            //gCmdIndex = 11;
            break;

        case 11:		//字段11	磁偏角方向
            //dtoc((double*)&hGPSRmcData.MagDeclinationDir, (uint8_t*)pData, 2);
            hGPSRmcData.MagDeclinationDir = atof(pData);
            //gCmdIndex = 12;
            break;

        case 12:		//字段12	模式指示
            hGPSRmcData.mode = *pData;
            //gCmdIndex = 13;
            break;

        case 13:		//字段13	校验值
            break;

        default:
            break;
    }

    if(gCmdIndex == 13)
        return 1;
    else
        return 0;
}

//定位信息
int gnss_GGA_Buff_Parser(char* pData)
{
//	gtime_t t;
    switch(gCmdIndex)
    {
        case 0:				//字段0
            if(NULL != strstr(pData, "$GNGGA"))
                //gCmdIndex = 1;

            break;

        case 1:				//字段1		UTC时间
            //dtoc((double*)&hGPSGgaData.timestamp, (uint8_t*)pData, 3);
            hGPSGgaData.timestamp = atof(pData);
            //str2time(pData, 2, 8, &t);
            //hGPSGgaData.gps_time = (uint32_t)time2gpst(t, (int*)&hGPSGgaData.gps_week);
            //gCmdIndex = 2;
            break;

        case 2:				//字段2		纬度
            //dtoc((double*)&hGPSGgaData.latitude, (uint8_t*)pData, 8);
            hGPSGgaData.latitude = atof(pData);
            //gCmdIndex = 3;
            break;

        case 3:				//字段3		纬度半球
            hGPSGgaData.LatHemisphere = *pData;
            //gCmdIndex = 4;
            break;

        case 4:				//字段4		经度
            //dtoc((double*)&hGPSGgaData.longitude, (uint8_t*)pData, 8);
            hGPSGgaData.longitude = atof(pData);
            //gCmdIndex = 5;
            break;

        case 5:				//字段5		经度半球
            hGPSGgaData.LonHemisphere = *pData;
            //gCmdIndex = 6;
            break;

        case 6:				//字段6		GPS状态
            hGPSGgaData.GPSState = (GNSS_State_TypeDef)( *pData - '0');
            //gCmdIndex = 7;
            break;

        case 7:				//字段7		正使用的卫星数量
            hGPSGgaData.numSatellitesUsed = atoi(pData);
            //gCmdIndex = 8;
            break;

        case 8:				//字段8		HDOP水平精度因子
            //dtoc((double*)&hGPSGgaData.HDOP, (uint8_t*)pData, 2);
            hGPSGgaData.HDOP = atof(pData);
            //gCmdIndex = 9;
            break;

        case 9:				//字段9		海拔高度
            //dtoc((double*)&hGPSGgaData.height, (uint8_t*)pData, 2);
            hGPSGgaData.height = atof(pData);
            //gCmdIndex = 10;
            break;

        case 10:			//字段10	地球椭球面相对大地水准面的高度
            //dtoc((double*)&hGPSGgaData.height_herizon, (uint8_t*)pData, 2);
            hGPSGgaData.height_herizon = atof(pData);
            //gCmdIndex = 11;
            break;

        case 11:			//字段11	差分时间
            //dtoc((double*)&hGPSGgaData.differTime, (uint8_t*)pData, 2);
            hGPSGgaData.differTime = atof(pData);
            //gCmdIndex = 12;
            break;

        case 12:			//字段12	差分站ID号
            hGPSGgaData.differStationID = atoi(pData);
            //gCmdIndex = 13;
            break;

        case 13:			//字段13	校验值
            break;

        default:
            break;
    }

    if(gCmdIndex == 13)
        return 1;
    else
        return 0;
}

//地面速度信息
int gnss_VTG_Buff_Parser(char* pData)
{
    switch(gCmdIndex)
    {
        case 0:				//字段0
            if((NULL != strstr(pData, "$GPVTG")) \
            	||(NULL != strstr(pData, "$GLVTG")) \
            	||(NULL != strstr(pData, "$GNVTG")))
                {

                }
            break;

        case 1:				//字段1			以真北为参考基准的地面航向
            //dtoc((double*)&hGPSVtgData.courseNorthAngle, (uint8_t*)pData, 4);
            hGPSVtgData.courseNorthAngle = atof(pData);
            //gCmdIndex = 2;
            break;

        case 2:				//字段2			以磁北为参考基准的地面航向
            //dtoc((double*)&hGPSVtgData.courseMagAngle, (uint8_t*)pData, 4);
            hGPSVtgData.trueNorth = *pData;
            //gCmdIndex = 3;
            break;

        case 3:				//字段2			以磁北为参考基准的地面航向
            //dtoc((double*)&hGPSVtgData.courseMagAngle, (uint8_t*)pData, 4);
            hGPSVtgData.courseMagAngle = atof(pData);;
            //gCmdIndex = 4;
            break;

        case 4:				//字段2			以磁北为参考基准的地面航向
            //dtoc((double*)&hGPSVtgData.courseMagAngle, (uint8_t*)pData, 4);
            hGPSVtgData.magneticNorth = *pData;
            //gCmdIndex = 5;
            break;

        case 5:				//字段3			地面速率 节
            //dtoc((double*)&hGPSVtgData.rateKnots, (uint8_t*)pData, 4);
            hGPSVtgData.rateKnots = atof(pData);
            //gCmdIndex = 6;
            break;

        case 6:				//字段2			以磁北为参考基准的地面航向
            //dtoc((double*)&hGPSVtgData.courseMagAngle, (uint8_t*)pData, 4);
            hGPSVtgData.N = *pData;
            //gCmdIndex = 7;
            break;

        case 7:				//字段4			地面速率 公里/小时
            //dtoc((double*)&hGPSVtgData.rateKm, (uint8_t*)pData, 4);
            hGPSVtgData.rateKm = atof(pData);
            //gCmdIndex = 8;
            break;

        case 8:				//字段2			以磁北为参考基准的地面航向
            //dtoc((double*)&hGPSVtgData.courseMagAngle, (uint8_t*)pData, 4);
            hGPSVtgData.K = *pData;
            //gCmdIndex = 9;
            break;

        case 9:				//字段5			模式指示
            hGPSVtgData.mode = *pData;
            //gCmdIndex = 10;
            break;

        case 10:				//字段6			校验值
            break;

        default:
            break;
    }

    if(gCmdIndex == 10)
        return 1;
    else
        return 0;
}

//日期
int gnss_ZDA_Buff_Parser(char* pData)
{
    gtime_t t;
    char str_time[6], year[4], month[2], day[2];
    char str[30] = {'\0'};

    switch(gCmdIndex)
    {
        case 0:					//字段0
            if((NULL != strstr(pData, "$GPZDA"))
             ||(NULL != strstr(pData, "$GLZDA"))
             ||(NULL != strstr(pData, "$GNZDA")))
             {

             }
            break;

        case 1:					//字段1			UTC时间
            //dtoc((double*)&hGPSZdaData.timestamp, (uint8_t*)pData, 3);
            hGPSZdaData.timestamp = atof(pData);
            hGPSZdaData.hour = ((uint32_t)hGPSZdaData.timestamp) / 10000;
            hGPSZdaData.minute = ((uint32_t)hGPSZdaData.timestamp) % 10000 / 100;
            hGPSZdaData.second = ((uint32_t)hGPSZdaData.timestamp) % 100;
            //gCmdIndex = 2;
            strncpy(str_time, pData, 6);
            break;

        case 2:					//字段2			日期
            hGPSZdaData.day = atoi(pData);
            strncpy(day, pData, 2);
            //gCmdIndex = 3;
            break;

        case 3:					//字段3			月份
            hGPSZdaData.month = atoi(pData);
            strncpy(month, pData, 2);
            //gCmdIndex = 4;
            break;

        case 4:					//字段4			年份
            hGPSZdaData.year = atoi(pData);
            strncpy(year, pData, 4);

            sprintf(str, "%0.4d %0.2d %0.2d %0.2d %0.2d %0.2d", \
                    hGPSZdaData.year, hGPSZdaData.month, hGPSZdaData.day, \
                    hGPSZdaData.hour, hGPSZdaData.minute, hGPSZdaData.second);

            str2time(str, 0, strlen(str), &t);
            hGPSGgaData.gps_time = time2gpst(t, (int*)&hGPSGgaData.gps_week);

            //gCmdIndex = 5;
            break;

        case 5:					//字段5			当地时域描述  小时
            hGPSZdaData.hour_timeDomain = atoi(pData);
            //gCmdIndex = 6;
            break;

        case 6:					//字段6			当地时域描述  分钟
            hGPSZdaData.minute_timeDomain = atoi(pData);
            //gCmdIndex = 7;
            break;

        case 7:					//字段7			校验和
            break;

        default:
            break;
    }

    if(gCmdIndex == 7)
        return 1;
    else
        return 0;
}

//方位角信息
int gnss_Heading_Buff_Parser(char* pData)
{
    switch(gCmdIndex)
    {
        case 0:					//字段0
            if(NULL != strstr(pData, "#HEADINGA"))
            {

            }
            break;

        case 1:					//字段1				解算状态
            //dtoc((double*)&hGPSGgaData.timestamp, (uint8_t*)pData, 3);
            hGPSHeadingData.calcState = (Resolve_TypeDef)atoi(pData);
            //gCmdIndex = 2;
            break;

        case 2:					//字段2				位置类型
        	hGPSHeadingData.posType = (GNSS_POS_TypeDef)atoi(pData);
        	//gCmdIndex = 3;
            break;

        case 3:					//字段3				基线长
        hGPSHeadingData.baseLen = atof(pData);
        	//gCmdIndex = 3;
            break;

        case 4:					//字段4				航向角
        hGPSHeadingData.courseAngle = atof(pData);
        	//gCmdIndex = 4;
            break;

        case 5:					//字段5				俯仰角
        hGPSHeadingData.pitchAngle = atof(pData);
        	//gCmdIndex = 4;
            break;

        case 6:					//字段6				保留
            break;

        case 7:					//字段7				航向标准偏差
        hGPSHeadingData.courseStandardDeviation = atof(pData);
            break;

        case 8:					//字段8				俯仰标准偏差
        hGPSHeadingData.pitchStandardDeviation = atof(pData);
            break;

        case 9:					//字段9				基站ID
        hGPSHeadingData.baseStationID = atol(pData);
            break;

        case 10:				//字段10			跟踪的卫星数量
        hGPSHeadingData.numSatellitesTracked = atoi(pData);
            break;

        case 11:				//字段11			使用的卫星数量
        hGPSHeadingData.numSatellitesUsed = atoi(pData);
            break;

        case 12:				//字段12			截止高度角以上的卫星数
        hGPSHeadingData.numSatellitesAboveCutoffAngle = atoi(pData);
            break;

        case 13:				//字段13			截止高度角以上有L2观测的卫星数
        hGPSHeadingData.numSatellitesAboveCutoffAngleL2 = atoi(pData);
            break;

        case 14:				//字段14			保留
            break;

        case 15:				//字段15			解状态
        hGPSHeadingData.solutionState = atoi(pData);
            break;

        case 16:				//字段16			保留
            break;

        case 17:				//字段17			信号掩码
        hGPSHeadingData.sigMask = (Single_Mask_TypeDef)atoi(pData);
            break;

        case 18:				//字段14			校验码
            break;

        default:
            break;
    }

    if(gCmdIndex == 18)
        return 1;
    else
        return 0;
}

//最佳位置
int gnss_BESTPOS_Buff_Parser(char * pData)
{
    switch(gCmdIndex)
    {
        case 0:				//字段0
            if(0 == strncmp("#BESTPOSA", (char const *)pData, 9))
            {

            }

            break;

        case 1:				//字段1			解算状态
        hGPSBestPosData.calcState = (Resolve_TypeDef)atoi(pData);
            break;

        case 2:				//字段2			位置类型
        hGPSBestPosData.posType = (GNSS_POS_TypeDef)atoi(pData);
            break;

        case 3:				//字段3			纬度
        hGPSBestPosData.latitude = atof(pData);
            break;

        case 4:				//字段4			经度
        hGPSBestPosData.longitude = atof(pData);
            break;

        case 5:				//字段5			海拔高
        hGPSBestPosData.height = atof(pData);
            break;

        case 6:				//字段6			高程异常
        hGPSBestPosData.heightVariance = atof(pData);
            break;

        case 7:				//字段7			坐标系id号
        hGPSBestPosData.coordinateID = (coordinateIDTypeDef)atoi(pData);
            break;

        case 8:				//字段8			纬度标准差
        hGPSBestPosData.latitudeStandardDeviation = atof(pData);
            break;

        case 9:				//字段9			经度标准差
        hGPSBestPosData.longitudeStandardDeviation = atof(pData);
            break;

        case 10:				//字段10			高度标准差
        hGPSBestPosData.heightStandardDeviation = atof(pData);
            break;

        case 11:				//字段11			基站ID
        hGPSBestPosData.stationID = atoi(pData);
            break;

        case 12:				//字段12			差分龄期
        hGPSBestPosData.differPeriod = atof(pData);
            break;

        case 13:				//字段13			解得龄期
        hGPSBestPosData.solutionPeriod = atof(pData);
            break;

        case 14:				//字段14			跟踪的卫星数
        hGPSBestPosData.numSatellitesTracked = atoi(pData);
            break;

        case 15:				//字段15			解算使用的卫星数
        hGPSBestPosData.numSatellitesUsed = atoi(pData);
            break;

        case 16:				//字段16			保留
            break;

        case 17:				//字段17			保留
            break;

        case 18:				//字段18			保留
            break;

        case 19:				//字段19			保留
            break;

        case 20:				//字段20			保留
            break;

        case 21:				//字段21			保留
            break;

        case 22:				//字段22			信号掩码
            break;

        case 23:				//字段23			校验值
            break;

        default:
            break;
    }

    if(gCmdIndex == 23)
        return 1;
    else
        return 0;
}

//方向角输出
int gnss_TRA_Buff_Parser(char * pData)
{
    switch(gCmdIndex)
    {
        case 0:					//字段0
            if(NULL != strstr(pData, "$GPTRA"))
            {

            }

            break;

        case 1:					//字段1				UTC时间
            dtoc((double*)&hGPSTraData.timestamp, (uint8_t*)pData, 3);
            hGPSZdaData.hour = ((uint32_t)hGPSTraData.timestamp) / 10000;
            hGPSZdaData.minute = ((uint32_t)hGPSTraData.timestamp) % 10000 / 100;
            hGPSZdaData.second = ((uint32_t)hGPSTraData.timestamp) % 100;
            //gCmdIndex = 2;
            break;

        case 2:					//字段2				方向角
            dtoc((double*)&hGPSTraData.headingAngle, (uint8_t*)pData, 8);
            //gCmdIndex = 3;
            break;

        case 3:					//字段3				俯仰角
            dtoc((double*)&hGPSTraData.pitchAngle, (uint8_t*)pData, 8);
            //gCmdIndex = 4;
            break;

        case 4:					//字段4				横滚角
            dtoc((double*)&hGPSTraData.rollAngle, (uint8_t*)pData, 8);
            //gCmdIndex = 5;
            break;

        case 5:					//字段5				GPS质量指示符
            //gCmdIndex = 6;
            break;

        case 6:					//字段6				卫星数
            hGPSTraData.starNum = atoi(pData);
            //gCmdIndex = 7;
            break;

        case 7:					//字段7				差分延迟
            dtoc((double*)&hGPSTraData.Age, (uint8_t*)pData, 8);
            //gCmdIndex = 8;
            break;

        case 8:					//字段8				基站号
            hGPSTraData.stationID = atoi(pData);
            //gCmdIndex = 9;
            break;

        case 9:					//字段9				校验和
            break;

        default:
            break;
    }

    if(gCmdIndex == 9)
        return 1;
    else
        return 0;
}

//AGRIC 信息
int gnss_AGRIC_Buff_Parser(char * pData)
{
    switch(gCmdIndex)
    {
        case 0:					//字段0
            if(NULL != strstr(pData, "#AGRICA"))
            {

            }

            break;

        case 1:					//字段1
            if(NULL != strstr(pData, "GNSS"))
            {

            }

            break;

        case 2:					//字段2				指令长度 不解析
            //gCmdIndex = 3;
            break;

        case 3:					//字段3				utc时间 年
            g_arm1MixData.utc_year = atoi(pData);
            //gCmdIndex = 4;
            break;

        case 4:					//字段4				utc时间 月
            g_arm1MixData.utc_month = atoi(pData);
            //gCmdIndex = 5;
            break;

        case 5:					//字段5				utc时间 日
            g_arm1MixData.utc_day = atoi(pData);
            //gCmdIndex = 6;
            break;

        case 6:					//字段6				utc时间 时
            g_arm1MixData.utc_hour = atoi(pData);
            //gCmdIndex = 7;
            break;

        case 7:					//字段7				utc时间 分
            g_arm1MixData.utc_min = atoi(pData);
            //gCmdIndex = 8;
            break;

        case 8:					//字段8				utc时间 秒
            g_arm1MixData.utc_sec = atoi(pData);
            //gCmdIndex = 9;
            break;

        case 9:					//字段9				流动站定位状态
            g_arm1MixData.rtk_status = atoi(pData);
            //gCmdIndex = 10;
            break;

        case 10:					//字段10				主从天线 Heading 解状态 未解析
            //gCmdIndex = 11;
            break;

        case 11:					//字段11				参与解算 GPS 卫星数
            g_arm1MixData.num_gps_star = atoi(pData);
            //gCmdIndex = 12;
            break;

        case 12:					//字段12				参与解算 BDS 卫星数
            g_arm1MixData.num_bd_star = atoi(pData);
            //gCmdIndex = 13;
            break;

        case 13:					//字段13				参与解算 GLO 卫星数
            g_arm1MixData.num_glo_ar = atoi(pData);
            //gCmdIndex = 14;
            break;

        case 14:					//字段14~19  未解析
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            //gCmdIndex++;
            break;

        case 20:					//字段20				航向角
            g_arm1MixData.Heading = strtod(pData, NULL);
            //gCmdIndex = 21;
            break;

        case 21:					//字段21			俯仰角
            g_arm1MixData.Heading = strtod(pData, NULL);
            //gCmdIndex = 22;
            break;

        case 22:					//字段22			横滚角
            g_arm1MixData.Heading = strtod(pData, NULL);
            //gCmdIndex = 23;
            break;

        case 23:					//字段23			未解析
            //gCmdIndex = 24;
            break;

        case 24:					//字段24			北向速度
            g_arm1MixData.vN = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 25:					//字段25			东向速度
            g_arm1MixData.vE = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 26:					//字段26		天顶速度
            g_arm1MixData.vZ = strtod(pData, NULL);
           // gCmdIndex++;
            break;

        case 27:					//字段27		北向速度标准差
            g_arm1MixData.sigma_vn = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 28:					//字段28			东向速度标准差
            g_arm1MixData.sigma_ve = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 29:					//字段29		天顶速度标准差
            g_arm1MixData.sigma_vz = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 30:					//字段30	  纬度
            g_arm1MixData.lat = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 31:					//字段31	  经度
            g_arm1MixData.lon = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 32:					//字段32   高程
            g_arm1MixData.alt = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 33:					//字段33~35  未解析
        case 34:
        case 35:
            //gCmdIndex++;
            break;

        case 36:					//字段36 纬度标准差
            g_arm1MixData.sigma_lat = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 37:					//字段37 纬度标准差
            g_arm1MixData.sigma_lon = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 38:					//字段38 纬度标准差
            g_arm1MixData.sigma_alt = strtod(pData, NULL);
            //gCmdIndex++;
            break;

        case 39:					//字段14~19  未解析
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
            //gCmdIndex++;
            break;

        case 48:					//字段48 GPS 周内毫秒
            g_arm1MixData.GPS_Sec = atol(pData);
            g_arm1MixData.GPS_Sec /= 1000;
            //gCmdIndex++;
            break;

        default:
            break;
    }

    if(gCmdIndex == 48)
        return 1;
    else
        return 0;
}

void gnss_config_movingbase(void)
{
    // 发送 "mode movingbase\r"

}

void gnss_request_GGA(void)
{
    // 发送 "GNGGA COM2 x\r"

}

void gnss_request_GSA(void)
{
    // 发送 "GNGSA COM2 x\r"

}

void gnss_request_RMC(void)
{
    // 发送 "GNRMC COM2 x\r"

}

void gnss_request_HDT(void)
{
    // 发送 "GPHDT COM2 x\r"

}

void gnss_request_HEADING(void)
{
    // 发送 "HEADING COM2 x\r"

}

void gnss_request_OTG(void)
{
    // 发送 "GPOTG COM2 x\r"

}

void gnss_request_ZDA(void)
{
    // 发送 "GPZDA COM2 x\r"

}

void gnss_request_saveconfig(void)
{
    // 发送 "SAVECONFIG"

}

ARM1_TO_KALAM_MIX_TypeDef*  gnss_get_mix_dataPtr(void)
{
    return &g_arm1MixData;
}

void gnss_Fetch_Data(void)
{
    hGPSData.timestamp = hGPSZdaData.timestamp;
    hGPSData.StarNum = hGPSHeadingData.numSatellitesTracked;
    hGPSData.ResolveState = hGPSHeadingData.calcState;
    hGPSData.PositioningState = hGPSRmcData.valid;
    hGPSData.PositionType = hGPSHeadingData.posType;
    hGPSData.LonHemisphere = hGPSRmcData.LonHemisphere;
    hGPSData.Lon = hGPSRmcData.longitude;
    hGPSData.LatHemisphere = hGPSRmcData.LatHemisphere;
    hGPSData.Lat = hGPSRmcData.latitude;
    hGPSData.Altitude = hGPSBestPosData.height;
    hGPSData.Heading = hGPSHeadingData.courseAngle;
    hGPSData.Pitch = hGPSHeadingData.pitchAngle;
    hGPSData.LonStd = hGPSBestPosData.longitudeStandardDeviation;
    hGPSData.LatStd = hGPSBestPosData.latitudeStandardDeviation;
    hGPSData.AltitudeStd = hGPSBestPosData.heightStandardDeviation;
    hGPSData.HeadingStd = hGPSHeadingData.courseStandardDeviation;
    hGPSData.PitchStd = hGPSHeadingData.pitchStandardDeviation;
    hGPSData.HDOP = hGPSGgaData.HDOP;
    hGPSData.GroundSpeed = hGPSRmcData.rate;
}

//void gnss_parse_unicore(uint8_t* pData, uint16_t len)
//{
//    uint8_t valid = 0;
//    uint8_t* pDat = pData;
//    uint16_t tCmd = 0;
//    uint16_t dataLen = len;
//    uint8_t  header[3] = {0xAA, 0x44, 0xB5};

//    //找到帧头
//    do
//    {
//        if(0 == memcmp(pDat, header, 3))
//        {
//            valid = 1;
//            break;
//        }

//        pDat++;
//        dataLen--;
//    }
//    while(dataLen > 0);
//}

uint8_t gnss_parse(uint8_t* pData, uint16_t dataLen)
{
    char * pBuf = NULL;
    char * pRxBuf = (char*)pData;
    char gnssCommaSeperator[] = ",";
    char gnssSemicolonSeperator[] = ";";
    int cmdKind = 0;
    uint8_t ret = INS_EOK;

//    ret = gnss_dataIsOk(pData, dataLen);

//    if(INS_ERROR == ret)
//    {
//    	//数据校验是否通过
//        return INS_ERROR;
//    }

    if(gCmdIndex == 0)
    {
        cmdKind = GNSS_Cmd_Kind_Parser(pRxBuf, dataLen); //命令起始符
        gCmdTypeIndex = GNSS_Cmd_Parser(pRxBuf);//命令类型
    }

    if(gCmdTypeIndex == 1)
    {
        ret = gnss_gprmcIsLocation(pData, dataLen);//判断是否为有效数据

        if(INS_ERROR == ret)
        {
            //到这里说明数据是无效的，这里对数据做相应处理

            return INS_ERROR;
        }
    }

    if(cmdKind != 0)
    {
        if(cmdKind == 1)
            pBuf = strtok(pRxBuf, gnssSemicolonSeperator);
        else if(cmdKind == 2)
            pBuf = strtok(pRxBuf, gnssCommaSeperator);

        gCmdIndex = 0;

        while(NULL != pBuf)
        {
            GNSS_Buff_Parser(pBuf, gCmdTypeIndex);
            pBuf = strtok(NULL, gnssCommaSeperator);
            gCmdIndex ++;
        }

        gCmdIndex = 0;

    }

    return ret;
}

void gnss_fill_rs422(RS422_FRAME_DEF* rs422)
{
    if(gCmdTypeIndex == 2)
    {
        rs422->data_stream.latitude = hGPSGgaData.latitude;
        rs422->data_stream.longitude = hGPSGgaData.longitude;
        rs422->data_stream.altitude = hGPSGgaData.height;
        rs422->data_stream.status |= BIT(0);
    }
    else if(gCmdTypeIndex == 4)
    {
        rs422->data_stream.gps_week = hGPSGgaData.gps_week;
        rs422->data_stream.poll_frame.gps_time = hGPSGgaData.gps_time;
    }
    else if(gCmdTypeIndex == 8)
    {
        rs422->data_stream.gps_week = hGPSGgaData.gps_week;
        rs422->data_stream.poll_frame.gps_time = hGPSGgaData.gps_time;
    }
}

void strtok_test(void)
{
    char *ch = "#AGRICA,98,GPS,UNKNOWN,1,8338000,0,0,18,2;GNSS,";
    char *buf;
    char *delim = ",";

    buf = strtok(ch, delim);

    while(NULL != buf)
    {
        buf = strtok(NULL, delim);
    }

}

