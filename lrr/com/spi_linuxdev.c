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


/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdio.h>      /* printf fprintf */
#include <fcntl.h>      /* open */
#include <unistd.h>     /* lseek, close */
#include <string.h>     /* memset */

#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "spi_linuxdev.h"

#if DEBUG == 1
	#include <errno.h>
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#if DEBUG == 1
	#define MSG( typ, txt, args... )   fprintf( stderr, typ" [%s:%d] "txt"\n", __FUNCTION__, __LINE__, ##args )
#else
	#define MSG( typ, txt, args... )
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS & TYPES -------------------------------------------- */

#define SPI_SUCCESS     0
#define SPI_ERROR       -1

#define READ_ACCESS     0x00
#define WRITE_ACCESS    0x80
#define MAX_CHUNK       1024
#define FAST_SIZE       10 /* 4 for SMTC regs, 10 to also accelerate AD reg access */

#define SPI_DEFAULT_SPEED   5000000

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int spi_linuxdev_open( const char *path, int speed_mhz, int *spi_ref )
{
	int i, j; /* return values */
	int dev;
	int val; /* setting value */
	
	/* Check input variables */
	if( path == NULL )
	{
		MSG( "ERROR", "null pointer path" );
		return SPI_ERROR;
	}
	if( spi_ref == NULL )
	{
		MSG( "ERROR", "null pointer spi_ref" );
		return SPI_ERROR;
	}
	
	/* Open SPI device */
	dev = open( path, O_RDWR );
	if( dev < 0 )
	{
		MSG( "ERROR", "open() function failed" );
		return SPI_ERROR;
	}
	
	/* Setting SPI mode to 'mode 0' */
	val = SPI_MODE_0;
	i = ioctl( dev, SPI_IOC_WR_MODE, &val );
	j = ioctl( dev, SPI_IOC_RD_MODE, &val );
	if( (i < 0) || (j < 0) )
	{
		MSG( "ERROR", "ioctl() failed to set SPI mode" );
		close( dev );
		return SPI_ERROR;
	}
	
	/* Setting SPI maximum clock speed (in Hz) */
	val = ( speed_mhz <= 0 ) ? SPI_DEFAULT_SPEED : speed_mhz * 1000000;
	i = ioctl( dev, SPI_IOC_WR_MAX_SPEED_HZ, &val );
	j = ioctl( dev, SPI_IOC_RD_MAX_SPEED_HZ, &val );
	if( (i < 0) || (j < 0) )
	{
		MSG( "ERROR", "ioctl() failed to set SPI maximum clock speed" );
		close( dev );
		return SPI_ERROR;
	}
	
	/* Setting SPI bit endianness (most significant bit first) */
	val = 0;
	i = ioctl( dev, SPI_IOC_WR_LSB_FIRST, &val );
	j = ioctl( dev, SPI_IOC_RD_LSB_FIRST, &val );
	if( (i < 0) || (j < 0) )
	{
		MSG( "ERROR", "ioctl() failed to set SPI bit endianness" );
		close( dev );
		return SPI_ERROR;
	}
	
	/* Setting SPI word size (8-bit words) */
	val = 0;
	i = ioctl( dev, SPI_IOC_WR_BITS_PER_WORD, &val );
	j = ioctl( dev, SPI_IOC_RD_BITS_PER_WORD, &val );
	if( (i < 0) || (j < 0) )
	{
		MSG( "ERROR", "ioctl() failed to set SPI word size" );
		close( dev );
		return SPI_ERROR;
	}
	
	MSG( "INFO", "SPI port opened successfully" );
	*spi_ref = dev; /* return file descriptor index */
	return SPI_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int spi_linuxdev_close( int spi_ref )
{
	int i;
	
	i = close( spi_ref );
	
	if( i == 0 )
	{
		MSG( "INFO", "SPI port closed successfully" );
		return SPI_SUCCESS;
	}
	else
	{
		MSG( "ERROR", "close( ) function failed" );
		return SPI_ERROR;
	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int spi_linuxdev_read( int spi_ref, uint8_t address, uint8_t *data, uint32_t size, uint8_t *status )
{
	int i, j;
	uint8_t  bufo[FAST_SIZE + 1];
	uint8_t  bufi[FAST_SIZE + 1];
	struct spi_ioc_transfer k;
	
	/* Check input variables */
	if( (size > 0) && (data == NULL) )
	{
		MSG( "ERROR", "null pointer data" );
		return SPI_ERROR;
	}
	if( address > 128 )
	{
		MSG( "ERROR", "invalid SPI address %i", address );
		return SPI_ERROR;
	}
	
	/* Clear spi_ioc_transfer structure (ensure compatibility) */
	memset( &k, 0, sizeof k );
	
	/* Setup command/status transfer */
	bufo[0] = READ_ACCESS | (address & 0x7F);
	memset( bufo + 1, 0, FAST_SIZE );
	
	if( size <= FAST_SIZE )
	{
		/* Accelerate registers access (transfers of 0 to FAST_SIZE bytes ) */
		k.tx_buf = (uintptr_t)bufo;
		k.rx_buf = (uintptr_t)bufi;
		k.len = 1 + size; /* < 5 */
		k.cs_change = 1;
		i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
		if( i != (int)k.len )
		{
			MSG( "ERROR", "spi_linuxdev_read failed, ioctl returned %i (errno %i)", i, errno );
			return SPI_ERROR;
		}
		if( size > 0 )
		{
			memcpy( (void *)data, (void *)(bufi + 1), size );
		}
		if( status != NULL )
		{
			*status = bufi[0];
		}
	}
	else
	{
		/* Send read command for 'long' transfer */
		k.tx_buf = (uintptr_t)&bufo[0];
		k.rx_buf = (uintptr_t)status; /* will not return status if pointer is null */
		k.len = 1;
		k.cs_change = 0;
		i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
		if( i != 1 )
		{
			MSG( "ERROR", "spi_linuxdev_read failed, command ioctl returned %i (errno %i)", i, errno );
			return SPI_ERROR;
		}
		
		/* Data read loop */
		k.tx_buf = (uintptr_t)NULL;
		for( j=0; size > 0; ++j )
		{
			k.rx_buf = (uintptr_t)(data + (j * MAX_CHUNK));
			if( size <= MAX_CHUNK )
			{ /* last chunk */
				k.len = size;
				k.cs_change = 1; /* de-assert CS at the end of this transfer */
			}
			else
			{ /* more chucks todo */
				k.len = MAX_CHUNK;
				k.cs_change = 0;
			}
			i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
			if( (i < 0) || ((uint32_t)i != k.len) )
			{
				MSG( "ERROR", "spi_linuxdev_read failed, data ioctl returned %i (errno %i)", i, errno );
				return SPI_ERROR;
			}
			size -= k.len;
		}
	}
	
	return SPI_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int spi_linuxdev_write( int spi_ref, uint8_t address, const uint8_t *data, uint32_t size, uint8_t *status )
{
	int i, j;
	uint8_t  bufo[FAST_SIZE + 1];
	uint8_t  bufi[FAST_SIZE + 1];
	struct spi_ioc_transfer k;
	
	/* Check input variables */
	if( (size > 0) && (data == NULL) )
	{
		MSG( "ERROR", "null pointer data" );
		return SPI_ERROR;
	}
	if( address > 128 )
	{
		MSG( "ERROR", "invalid SPI address %i", address );
		return SPI_ERROR;
	}
	
	/* Clear spi_ioc_transfer structure (ensure compatibility) */
	memset( &k, 0, sizeof k );
	
	/* Setup command/status transfer */
	bufo[0] = WRITE_ACCESS | (address & 0x7F);
	
	if( size <= FAST_SIZE )
	{
		/* Accelerate registers access (transfer of 0 to FAST_SIZE bytes ) */
		if( size > 0 )
		{
			memcpy( (void *)(bufo + 1), (void *)data, size );
		}
		k.tx_buf = (uintptr_t)bufo;
		k.rx_buf = (uintptr_t)bufi;
		k.len = 1 + size; /* < 5 */
		k.cs_change = 1;
		i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
		if( i != (int)k.len )
		{
			MSG( "ERROR", "spi_linuxdev_write failed, ioctl returned %i (errno %i)", i, errno );
			return SPI_ERROR;
		}
		if( status != NULL )
		{
			*status = bufi[0];
		}
	}
	else
	{
		/* Send write command for 'long' transfer */
		k.tx_buf = (uintptr_t)&bufo[0];
		k.rx_buf = (uintptr_t)status; /* will not return status if pointer is null */
		k.len = 1;
		k.cs_change = 0;
		i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
		if( i != 1 )
		{
			MSG( "ERROR", "spi_linuxdev_write failed, command ioctl returned %i (errno %i)", i, errno );
			return SPI_ERROR;
		}
	
		/* Data write loop */
		k.rx_buf = (uintptr_t)NULL;
		for( j=0; size > 0; ++j )
		{
			k.tx_buf = (uintptr_t)(data + (j * MAX_CHUNK));
			if( size <= MAX_CHUNK )
			{ /* last chunk */
				k.len = size;
				k.cs_change = 1; /* de-assert CS at the end of this transfer */
			}
			else
			{ /* more chucks todo */
				k.len = MAX_CHUNK;
				k.cs_change = 0;
			}
			i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
			if( (i < 0) || ((uint32_t)i != k.len) )
			{
				MSG( "ERROR", "spi_linuxdev_write failed, data ioctl returned %i (errno %i)", i, errno );
				return SPI_ERROR;
			}
			size -= k.len;
		}
	}
	
	return SPI_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
