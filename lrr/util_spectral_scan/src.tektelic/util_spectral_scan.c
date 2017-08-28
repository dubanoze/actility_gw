#include <stdint.h>     /* C99 types */
#include <stdio.h>      /* NULL printf */
#include <stdlib.h>     /* EXIT atoi */
#include <unistd.h>     /* getopt */
#include <string.h>

#include "sx1301ar_hal.h"

void use()
{
	printf( "util_spectral_scan [-a <antenna>] [-p <period>]\n" );
	printf( " -a <antenna>: select input antenna (0 or 1)\n");
	printf( " -p <period>: select sample period. Setting to 0 = 0.5us, each LSB = 0.5us\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int		ret, antenna=0, period=0, i;
	ssStatus	status;

	while( (i = getopt( argc, argv, "ha:p:" )) != -1 )
	{
		switch( i )
		{
		case 'h':
			use();
			break;
		case 'a':
			antenna = atoi(optarg);
			break;
		case 'p':
			period = atoi(optarg);
			break;
		default:
			use();
		}
		
	}

//	can't do that, return always 2 => restart a new scan when the previous is finished
//	while ((ret=runSpectralScan(&status, period, antenna)) != 0)

	runSpectralScan(&status, period, antenna);

	while ((ret=readSpectralScanProgress()) < 100)
		sleep(1);

	printf("util_spectral_scan done, check '%s' file.\n", SPECTRALSCAN_RESULTS_OUTPUT_FILE);
	return 0;
}

