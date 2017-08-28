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

Note: SPI_ATMEL compilation flag has been introduced to remove the control of
cs_change when using linux spidev spi-atmel driver. It seems that this driver
wrongly interpret the cs_change which leads to unfunctionnal SPI transfer when
used. So, as a workaround, we don't do it. The default case is to use it.
*/


// spidevice is handled in TEKTELIC HAL
#ifndef TEKTELIC
/* -------------------------------------------------------------------------- */
/* --- DEPENDENCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdio.h>      /* printf fprintf */
#include <fcntl.h>      /* open */
#include <unistd.h>     /* lseek, close */
#include <string.h>     /* memset */
#include <sys/mman.h>

#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "spi_linuxdev_ar.h"

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
#define MAX_SPI_BLK_SIZE	4095
#define FAST_SIZE       10 /* 4 for SMTC regs, 10 to also accelerate AD reg access */

#define SPI_DEFAULT_SPEED   2000000

/* SPI mux header constants */
#define SPI_HDR_REG_FPGA 0x02 /* SPI header for FPGA registers */

/* Mem Mapping constants */
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

#ifdef SPI_ATMEL
/* ATMEL CPU registers for SPI setup */
#define ATMEL_REG_ADDR_SPI_MR_0 0xF0000004
#define ATMEL_REG_ADDR_SPI_MR_1 0xF0004004
#define ATMEL_REG_ADDR_SPI_CR_0 0xF0000000
#define ATMEL_REG_ADDR_SPI_CR_1 0xF0004000
#define SPI_NO_CSCHANGE
#endif

#ifdef SPI_IMX
#define SPI_NO_CSCHANGE
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

#ifdef SPI_ATMEL
int spi_set_mode( uint8_t spi_idx, spi_mode_t mode )
{
    int ret = 0;

    int fd;
    void *map_base, *virt_addr;
    unsigned long read_result, writeval;
    off_t target;

    /* Select register address */
    switch (spi_idx)
    {
        case 0:
            target = (off_t)ATMEL_REG_ADDR_SPI_MR_0;
            break;
        case 1:
            target = (off_t)ATMEL_REG_ADDR_SPI_MR_1;
            break;
        default:
            MSG( "ERROR", "SPI mode not supported (%d)", mode );
            return SPI_ERROR;
    }

    if( ( fd = open("/dev/mem", O_RDWR | O_SYNC) ) == -1 )
    {
        MSG( "ERROR", "Failed to open /dev/mem to access CPU registers" );
        return SPI_ERROR;
    }

    /* Map one page */
    map_base = mmap( 0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target & ~MAP_MASK );
    if( map_base == (void *) -1 )
    {
        MSG( "ERROR", "Failed to map memory" );
        close( fd );
        return SPI_ERROR;
    }

    /* Read current register value */
    virt_addr = map_base + (target & MAP_MASK);
    read_result = *( (unsigned long *) virt_addr );
    printf("Value at address 0x%X (%p): 0x%lX\n", (unsigned int)target, virt_addr, read_result);

    /* Change only MSTR bit to requested mode (last bit of the register) */
    switch (mode)
    {
        case SPI_MODE_MASTER:
            writeval = (read_result |= (1 << 0));
            printf( "INFO: Setting SPI %d to MASTER mode\n", spi_idx );
            break;
        case SPI_MODE_SLAVE:
            writeval = (read_result &= ~(1 << 0));
            printf( "INFO: Setting SPI %d to SLAVE mode\n", spi_idx );
            break;
        default:
            writeval = read_result; /* no change */
            MSG( "ERROR", "SPI mode not supported (%d)", mode );
            ret = SPI_ERROR;
            break;
    }
    *((unsigned long *) virt_addr) = writeval;

    /* Read back */
    read_result = *( (unsigned long *) virt_addr );
    printf("Written 0x%lX; readback 0x%lX\n", writeval, read_result);

    /* Select SPI_CR register address */
	switch (spi_idx)
	{
		case 0:
			target = (off_t)ATMEL_REG_ADDR_SPI_CR_0;
			break;
		case 1:
			target = (off_t)ATMEL_REG_ADDR_SPI_CR_1;
			break;
		default:
			MSG( "ERROR", "SPI mode not supported (%d)", mode );
			return SPI_ERROR;
	}

	virt_addr = map_base + (target & MAP_MASK);

	/* Disable the SPI */
	printf( "INFO: Disable the SPI %d \n", spi_idx );
	printf("Writing 0x02 at address 0x%X (%p)\n", (unsigned int)target, virt_addr);
	*((unsigned char  *) virt_addr) = 0x02;
	if( mode == SPI_MODE_MASTER )
	{
		/* Now enable the SPI */
		sleep(1);
		printf( "INFO: Enable the SPI %d \n", spi_idx );
		printf("Writing 0x01 at address 0x%X (%p)\n", (unsigned int)target, virt_addr);
		*((unsigned char  *) virt_addr) = 0x01;
	}

    /* Close and exit */
	if( munmap(map_base, MAP_SIZE) == -1 )
    {
        MSG( "ERROR", "Failed to unmap memory" );
    }
    close(fd);

    return ret;
}
#endif

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
        MSG( "ERROR", "open() function failed - %s (%s)", strerror(errno), path);
        return SPI_ERROR;
    }

    /* Setting SPI mode to 'mode 0' */
    val = SPI_MODE_0;
    i = ioctl( dev, SPI_IOC_WR_MODE, &val );
    j = ioctl( dev, SPI_IOC_RD_MODE, &val );
    if( (i < 0) || (j < 0) )
    {
        MSG( "ERROR", "ioctl() failed to set SPI mode - %s", strerror(errno) );
        close( dev );
        return SPI_ERROR;
    }

    /* Setting SPI maximum clock speed (in Hz) */
    val = ( speed_mhz <= 0 ) ? SPI_DEFAULT_SPEED : speed_mhz * 1000000;
    i = ioctl( dev, SPI_IOC_WR_MAX_SPEED_HZ, &val );
    j = ioctl( dev, SPI_IOC_RD_MAX_SPEED_HZ, &val );
    if( (i < 0) || (j < 0) )
    {
        MSG( "ERROR", "ioctl() failed to set SPI maximum clock speed - %s", strerror(errno) );
        close( dev );
        return SPI_ERROR;
    }
    else
    {
        printf( "INFO: SPI speed set to %d Hz\n", val );
    }

    /* Setting SPI bit endianness (most significant bit first) */
    val = 0;
    i = ioctl( dev, SPI_IOC_WR_LSB_FIRST, &val );
    j = ioctl( dev, SPI_IOC_RD_LSB_FIRST, &val );
    if( (i < 0) || (j < 0) )
    {
        MSG( "ERROR", "ioctl() failed to set SPI bit endianness - %s", strerror(errno) );
        close( dev );
        return SPI_ERROR;
    }

    /* Setting SPI word size (8-bit words) */
    val = 0;
    i = ioctl( dev, SPI_IOC_WR_BITS_PER_WORD, &val );
    j = ioctl( dev, SPI_IOC_RD_BITS_PER_WORD, &val );
    if( (i < 0) || (j < 0) )
    {
        MSG( "ERROR", "ioctl() failed to set SPI word size - %s", strerror(errno) );
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

int spi_linuxdev_read( uint8_t header, int spi_ref, uint16_t address, uint8_t *data, uint32_t size, uint8_t *status )
{
    int i;
    uint8_t  bufo[FAST_SIZE + 3]; /* add header and address bytes */
    uint8_t  bufi[FAST_SIZE + 3];
    struct spi_ioc_transfer k[2];
    uint8_t  addr_size; /* number of bytes to code address (1 or 2) */
    uint32_t size_to_do, chunk_size, offset;
    uint32_t byte_transfered = 0;

    /* Check input variables */
    if( (size > 0) && (data == NULL) )
    {
        MSG( "ERROR", "null pointer data" );
        return SPI_ERROR;
    }
    /* 16bits address for FPGA, 8bits address for everyhting else */
    if ( ((header == SPI_HDR_REG_FPGA) && (address > 32768)) || ((header != SPI_HDR_REG_FPGA) && (address > 128)) )
    {
        MSG( "ERROR", "invalid SPI address %i", address );
        return SPI_ERROR;
    }

    /* Clear spi_ioc_transfer structure (ensure compatibility) */
    memset( &k, 0, sizeof k );

    /* Setup command/status transfer */
    bufo[0] = header;
    if (header == SPI_HDR_REG_FPGA)
    {
        /* 16 bits address */
        addr_size = 2;
        bufo[1] = READ_ACCESS | ((address >> 8) & 0x7F);
        bufo[2] = address & 0xFF;
    }
    else
    {
        /* 8 bits address */
        addr_size = 1;
        bufo[1] = READ_ACCESS | (address & 0x7F);
    }
    memset( bufo + addr_size + 1, 0, FAST_SIZE );

    if( size <= FAST_SIZE )
    {
        /* Accelerate registers access (transfers of 0 to FAST_SIZE bytes ) */
        k[0].tx_buf = (uintptr_t)bufo;
        k[0].rx_buf = (uintptr_t)bufi;
        k[0].len = 1 + addr_size + size; /* add header + address bytes */
#ifndef SPI_NO_CSCHANGE
        k[0].cs_change = 1;
#endif
        i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
        if( i != (int)k[0].len )
        {
            MSG( "ERROR", "spi_linuxdev_read failed, ioctl returned %i (errno %i)", i, errno );
            return SPI_ERROR;
        }
        if( size > 0 )
        {
            memcpy( (void *)data, (void *)(bufi + 1 + addr_size), size ); /* skip first 2 or 3 bytes */
        }
        if( status != NULL )
        {
            *status = bufi[1];
        }
    }
    else
    {
        /* I/O transaction */
        k[0].tx_buf = (uintptr_t)&bufo[0];
        k[0].len = 1 + addr_size;
#ifndef SPI_NO_CSCHANGE
        k[0].cs_change = 0;
        k[1].cs_change = 1;
#endif
        size_to_do = size;
        for ( i = 0; size_to_do > 0; ++i )
        {
            chunk_size = (size_to_do < MAX_CHUNK) ? size_to_do : MAX_CHUNK;
            offset = i * MAX_CHUNK;
            k[1].rx_buf = (uintptr_t)(data + offset);
            k[1].len = chunk_size;
            byte_transfered += (ioctl(spi_ref, SPI_IOC_MESSAGE(2), &k) - k[0].len);
            MSG("INFO", "BURST READ: to trans %d # chunk %d # transferred %u \n", size_to_do, chunk_size, byte_transfered);
            size_to_do -= chunk_size;  /* subtract the quantity of data already transferred */
        }

		/* determine return code */
		if ( byte_transfered != size )
		{
		    MSG("ERROR", "SPI BURST READ FAILURE\n");
		    return SPI_ERROR;
		}
	}
	
	return SPI_SUCCESS;
}

int spi_linuxdev_read_flash_mem( int spi_ref, uint8_t cmd, uint8_t *address, uint8_t address_size, uint8_t *data, uint32_t size )
{
	int i;
	uint8_t  bufo[FAST_SIZE + 4]; /* add header and address bytes */
	uint8_t  bufi[FAST_SIZE + 4];
	struct spi_ioc_transfer k[2];
	
	/* Check input variables */
	if( (size > 0) && (data == NULL) )
	{
		MSG( "ERROR", "null pointer data" );
		return SPI_ERROR;
	}
	
	if( size >= MAX_SPI_BLK_SIZE )
	{
		MSG( "ERROR", "Maximum SPI block size" );
		return SPI_ERROR;
	}

	/* Clear spi_ioc_transfer structure (ensure compatibility) */
	memset( &k, 0, sizeof k );
	
	/* Setup command transfer */
	bufo[0] = cmd;
	if( address_size )
		memcpy( bufo + 1, address, address_size);

	memset( bufo + 1 + address_size, 0, FAST_SIZE );

	k[0].delay_usecs = 0;
	if( size <= FAST_SIZE )
	{
		/* Accelerate registers access (transfers of 0 to FAST_SIZE bytes ) */
		k[0].tx_buf = (uintptr_t)bufo;
		k[0].rx_buf = (uintptr_t)bufi;
		k[0].len = 1 + address_size + size; /* address bytes */
#ifndef SPI_NO_CSCHANGE
        k[0].cs_change = 1;
#endif

		i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), k );
		if( i != (int)k[0].len )
		{
			MSG( "ERROR", "spi_linuxdev_read failed, ioctl returned %i (errno %i)", i, errno );
			return SPI_ERROR;
		}
		if( size > 0 )
		{
			memcpy( (void *)data, (void *)(bufi + 1 + address_size), size ); /* skip first 2 or 3 bytes */
		}
	}
	else
	{
		/* I/O transaction */
		k[0].tx_buf = (uintptr_t) &bufo[0];
		k[1].rx_buf = (uintptr_t) data;
		k[0].len = 1 + address_size;
#ifndef SPI_NO_CSCHANGE
        k[0].cs_change = 0;
        k[1].cs_change = 1;
#endif
        k[1].len = size;

		i = ioctl( spi_ref, SPI_IOC_MESSAGE( 2 ), &k );
		if( i != (int)(k[0].len + k[1].len) )
		{
			MSG( "ERROR", "spi_linuxdev_read failed, ioctl returned %i (errno %i)", i, errno );
			return SPI_ERROR;
		}
	}
	
	return SPI_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int spi_linuxdev_write( uint8_t header, int spi_ref, uint16_t address, const uint8_t *data, uint32_t size, uint8_t *status )
{
    int i;
    uint8_t  bufo[FAST_SIZE + 3]; /* add header and address bytes */
    uint8_t  bufi[FAST_SIZE + 3];
    struct spi_ioc_transfer k[2];
    uint8_t addr_size;
    uint32_t size_to_do, chunk_size, offset;
    uint32_t byte_transfered = 0;

    /* Check input variables */
    if( (size > 0) && (data == NULL) )
    {
        MSG( "ERROR", "null pointer data" );
        return SPI_ERROR;
    }
    /* 16bits address for FPGA, 8bits address for everyhting else */
    if ( ((header == SPI_HDR_REG_FPGA) && (address > 32768)) || ((header != SPI_HDR_REG_FPGA) && (address > 128)) )
    {
        MSG( "ERROR", "invalid SPI address %i", address );
        return SPI_ERROR;
    }

    /* Clear spi_ioc_transfer structure (ensure compatibility) */
    memset( &k, 0, sizeof k );

    /* Setup command/status transfer */
    bufo[0] = header;
    if (header == SPI_HDR_REG_FPGA)
    {
        /* 16 bits address */
        addr_size = 2;
        bufo[1] = WRITE_ACCESS | ((address >> 8) & 0x7F);
        bufo[2] = address & 0xFF;
    }
    else
    {
        /* 8 bits address */
        addr_size = 1;
        bufo[1] = WRITE_ACCESS | (address & 0x7F);
    }

    if( size <= FAST_SIZE )
    {
        /* Accelerate registers access (transfer of 0 to FAST_SIZE bytes ) */
        if( size > 0 )
        {
            memcpy( (void *)(bufo + 1 + addr_size), (void *)data, size );
        }
        k[0].tx_buf = (uintptr_t)bufo;
        k[0].rx_buf = (uintptr_t)bufi;
        k[0].len = 1 + addr_size + size; /* add header + address bytes */
#ifndef SPI_NO_CSCHANGE
        k[0].cs_change = 1;
#endif
        i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
        if( i != (int)k[0].len )
        {
            MSG( "ERROR", "spi_linuxdev_write failed, ioctl returned %i (errno %i)", i, errno );
            return SPI_ERROR;
        }
        if( status != NULL )
        {
            *status = bufi[1];
        }
    }
    else
    {
        /* I/O transaction */
        k[0].tx_buf = (uintptr_t)&bufo[0];
        k[0].len = 1 + addr_size;
#ifndef SPI_NO_CSCHANGE
        k[0].cs_change = 0;
        k[1].cs_change = 1;
#endif
        size_to_do = size;
        for (i = 0; size_to_do > 0; ++i)
        {
            chunk_size = (size_to_do < MAX_CHUNK) ? size_to_do : MAX_CHUNK;
            offset = i * MAX_CHUNK;
            k[1].tx_buf = (uintptr_t)(data + offset);
            k[1].len = chunk_size;
            byte_transfered += (ioctl(spi_ref, SPI_IOC_MESSAGE(2), &k) - k[0].len);
            MSG("INFO", "BURST WRITE: to trans %d # chunk %d # transferred %u \n", size_to_do, chunk_size, byte_transfered);
            size_to_do -= chunk_size; /* subtract the quantity of data already transferred */
        }

        /* determine return code */
        if (byte_transfered != size) {
            MSG("ERROR", "SPI BURST WRITE FAILURE\n");
            return SPI_ERROR;
        }
    }

    return SPI_SUCCESS;
}

int spi_linuxdev_write_flash_mem( int spi_ref, uint8_t cmd, uint8_t *address, uint8_t address_size, const uint8_t *data, uint32_t size )
{
	int i;
	uint8_t  bufo[FAST_SIZE + 3]; /* add header and address bytes */
	uint8_t  bufi[FAST_SIZE + 3];
	struct spi_ioc_transfer k[2];
	
	/* Check input variables */
	if( (size > 0) && (data == NULL) )
	{
		MSG( "ERROR", "null pointer data" );
		return SPI_ERROR;
	}
	
	if( size >= MAX_SPI_BLK_SIZE )
	{
		MSG( "ERROR", "Maximum SPI block size" );
		return SPI_ERROR;
	}

	/* Clear spi_ioc_transfer structure (ensure compatibility) */
	memset( &k, 0, sizeof k );
	
	/* Setup command transfer */
	bufo[0] = cmd;
	if( address_size )
		memcpy( bufo + 1, address, address_size);

	memset( bufo + 1 + address_size, 0, FAST_SIZE );

	k[0].delay_usecs = 0;

	if( size <= FAST_SIZE )
	{
		/* Accelerate registers access (transfer of 0 to FAST_SIZE bytes ) */
		if( size > 0 )
		{
			memcpy( (void *)(bufo + 1 + address_size), (void *)data, size );
		}
		k[0].tx_buf = (uintptr_t)bufo;
		k[0].rx_buf = (uintptr_t)bufi;
		k[0].len = 1 + size + address_size; /* cmd + address bytes */
#ifndef SPI_NO_CSCHANGE
        k[0].cs_change = 1;
#endif

		i = ioctl( spi_ref, SPI_IOC_MESSAGE( 1 ), &k );
		if( i != (int)k[0].len )
		{
			MSG( "ERROR", "spi_linuxdev_write failed, ioctl returned %i (errno %i)", i, errno );
			return SPI_ERROR;
		}
	}
	else
	{
		/* I/O transaction */
		k[0].tx_buf = (unsigned long) &bufo[0];
		k[0].len = 1 + address_size; /* cmd + address bytes */
		k[0].cs_change = 0;
		k[1].tx_buf = (unsigned long) data;
		k[1].len = size;

		i = ioctl( spi_ref, SPI_IOC_MESSAGE( 2 ), &k );
		if( i != (int)(k[0].len + k[1].len) )
		{
			MSG( "ERROR", "spi_linuxdev_write failed, ioctl returned %i (errno %i)", i, errno );
			return SPI_ERROR;
		}
	}

	return SPI_SUCCESS;
}

#endif	// TEKTELIC
/* --- EOF ------------------------------------------------------------------ */
