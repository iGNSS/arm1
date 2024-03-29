#ifndef gd32F4XX_SPI_H_INCLUDED
#define gd32F4XX_SPI_H_INCLUDED


#ifdef __cplusplus
extern "C" {
#endif


#include "insdef.h"
#include "gd32f4xx.h"

#define RT_SPI_CPHA     (1<<0)                             /* bit[0]:CPHA, clock phase */
#define RT_SPI_CPOL     (1<<1)                             /* bit[1]:CPOL, clock polarity */
/**
 * At CPOL=0 the base value of the clock is zero
 *  - For CPHA=0, data are captured on the clock's rising edge (low->high transition)
 *    and data are propagated on a falling edge (high->low clock transition).
 *  - For CPHA=1, data are captured on the clock's falling edge and data are
 *    propagated on a rising edge.
 * At CPOL=1 the base value of the clock is one (inversion of CPOL=0)
 *  - For CPHA=0, data are captured on clock's falling edge and data are propagated
 *    on a rising edge.
 *  - For CPHA=1, data are captured on clock's rising edge and data are propagated
 *    on a falling edge.
 */
#define RT_SPI_LSB      (0<<2)                             /* bit[2]: 0-LSB */
#define RT_SPI_MSB      (1<<2)                             /* bit[2]: 1-MSB */

#define RT_SPI_MASTER   (0<<3)                             /* SPI master device */
#define RT_SPI_SLAVE    (1<<3)                             /* SPI slave device */

#define RT_SPI_MODE_0       (0 | 0)                        /* CPOL = 0, CPHA = 0 */
#define RT_SPI_MODE_1       (0 | RT_SPI_CPHA)              /* CPOL = 0, CPHA = 1 */
#define RT_SPI_MODE_2       (RT_SPI_CPOL | 0)              /* CPOL = 1, CPHA = 0 */
#define RT_SPI_MODE_3       (RT_SPI_CPOL | RT_SPI_CPHA)    /* CPOL = 1, CPHA = 1 */

#define RT_SPI_MODE_MASK    (RT_SPI_CPHA | RT_SPI_CPOL | RT_SPI_MSB)

#define RT_SPI_BUS_MODE_SPI         (1<<0)
#define RT_SPI_BUS_MODE_QSPI        (1<<1)

#define RT_SPI_CS_HIGH  (1<<4)                             /* Chipselect active high */
#define RT_SPI_NO_CS    (1<<5)                             /* No chipselect */
#define RT_SPI_3WIRE    (1<<6)                             /* SI/SO pin shared */
#define RT_SPI_READY    (1<<7)                             /* Slave pulls low to pause */


#define	SPI3_NSS_PIN_NUM	GD32F450_PG14_PIN_NUM

/**
 * SPI message structure
 */
struct ins_spi_message
{
    const void *send_buf;
    void *recv_buf;
    uint32_t length;
    struct ins_spi_message *next;

    unsigned cs_take    : 1;
    unsigned cs_release : 1;
};

/**
 * SPI configuration structure
 */
struct ins_spi_configuration
{
    uint8_t mode;
    uint8_t data_width;
    uint16_t reserved;

    uint32_t max_hz;
};


struct gd32_spi_cs
{
	uint32_t spi_periph;
    uint32_t GPIOx;
    uint32_t GPIO_Pin;
};

void hw_spi_init(uint32_t spi_periph);

int spi_send(const void *send_buf, uint32_t length,uint8_t cs_take,uint8_t cs_release);

int spi_recv(void *recv_buf, uint32_t length,uint8_t cs_take,uint8_t cs_release);

#ifdef __cplusplus
}
#endif

#endif // gd32F20X_40X_SPI_H_INCLUDED
