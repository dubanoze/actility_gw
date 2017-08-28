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


#ifndef _SPI_LINUXDEV_H
#define _SPI_LINUXDEV_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */

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
int spi_linuxdev_read( int spi_ref, uint8_t address, uint8_t *data, uint32_t size, uint8_t *status );

/**
@brief Write data to an SPI port
@param spi_ref  SPI port file descriptor index
@param address  SPI address (0 to 127)
@param data     Pointer to a data buffer storing the data to write
@param size     Size of the data to write
@param status   Pointer to return status byte, NULL to ignore
@return 0 if SPI data write is successful, -1 else
*/
int spi_linuxdev_write( int spi_ref, uint8_t address, const uint8_t *data, uint32_t size, uint8_t *status );

#endif

/* --- EOF ------------------------------------------------------------------ */
