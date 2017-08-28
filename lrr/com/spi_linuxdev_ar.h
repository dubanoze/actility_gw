/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2014 Semtech-Cycleo

Description:
	SPI functions using Linux device driver (typically /dev/spidev{number})

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


#ifndef TEKTELIC

#ifndef _SPI_LINUXDEV_H
#define _SPI_LINUXDEV_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

typedef enum
{
	SPI_MODE_UNDEFINED = 0,
	SPI_MODE_MASTER,
	SPI_MODE_SLAVE
}
spi_mode_t;

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief Open SPI port
@param path       Path to the SPI device driver (absolute or relative)
@param speed_mhz  SPI clock speed in MHz (<= 0 for default value)
@param spi_ref    Pointer to receive SPI port file descriptor index
@return 0 if SPI port was open successfully, -1 else
*/
int spi_linuxdev_open( const char *path, int speed_mhz, int *spi_ref );

/**
@brief Close SPI port
@param spi_ref  SPI port file descriptor index
@return 0 if SPI port was closed successfully, -1 else
*/
int spi_linuxdev_close( int spi_ref );

/**
@brief Read data from an SPI port
@param spi_ref  SPI port file descriptor index
@param address  SPI address (0 to 127)
@param data     Pointer to a data buffer to store read data
@param size     Size of the data to read
@param status   Pointer to return status byte, NULL to ignore
@return 0 if SPI data read is successful, -1 else
*/
int spi_linuxdev_read( uint8_t header, int spi_ref, uint16_t address, uint8_t *data, uint32_t size, uint8_t *status );

/**
@brief Read data from flash memory, through an SPI port
@param spi_ref  SPI port file descriptor index
@param cmd		Command of the flash memory
@param address  SPI address up to 32 bits
@param address_size  SPI address size in byte
@param data     Pointer to a data buffer to store read data
@param size     Size of the data to read
@return 0 if SPI data read is successful, -1 else
*/
int spi_linuxdev_read_flash_mem( int spi_ref, uint8_t cmd, uint8_t *address, uint8_t address_size, uint8_t *data, uint32_t size );

/**
@brief Write data to an SPI port
@param spi_ref  SPI port file descriptor index
@param address  SPI address (0 to 127)
@param data     Pointer to a data buffer storing the data to write
@param size     Size of the data to write
@param status   Pointer to return status byte, NULL to ignore
@return 0 if SPI data write is successful, -1 else
*/
int spi_linuxdev_write( uint8_t header, int spi_ref, uint16_t address, const uint8_t *data, uint32_t size, uint8_t *status );

/**
@brief Write data to flash memory, through an SPI port
@param spi_ref  SPI port file descriptor index
@param cmd		Command of the flash memory
@param address  SPI address up to 32 bits
@param address_size  SPI address size in byte
@param data     Pointer to a data buffer storing the data to write
@param size     Size of the data to write
@return 0 if SPI data write is successful, -1 else
*/
int spi_linuxdev_write_flash_mem( int spi_ref, uint8_t cmd, uint8_t *address, uint8_t address_size, const uint8_t *data, uint32_t size );

#ifdef SPI_ATMEL
/**
@brief Configure SPI to Master/Slave mode
@param spi_idx  SPI device index (0 to 255)
@param mode     SPI mode, master or slace
@return 0 if SPI set mode is successful, -1 else
*/
int spi_set_mode( uint8_t spi_idx, spi_mode_t mode );
#endif

#endif

#endif // TEKTELIC
/* --- EOF ------------------------------------------------------------------ */
