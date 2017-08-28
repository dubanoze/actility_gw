/*
/ _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
\____ \| ___ |    (_   _) ___ |/ ___)  _ \
_____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
 (C)2013 Semtech-Cycleo

Description:
   SPI stress test

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf sprintf fopen fputs */

#include <signal.h>     /* sigaction */
#include <unistd.h>     /* getopt access */
#include <stdlib.h>     /* rand */

#include "loragw_reg.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define MSG(args...)    fprintf(stderr, args) /* message that is destined to the user */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define VERS                    103
#define READS_WHEN_ERROR        16 /* number of times a read is repeated if there is a read error */
#define BUFF_SIZE               1024 /* maximum number of bytes that we can write in sx1301 RX data buffer */
#define DEFAULT_TX_NOTCH_FREQ   129E3

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

/* signal handling variables */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void sig_handler(int sigio);

void usage (void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = 1;;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = 1;
    }
}

/* describe command line options */
void usage(void) {
    MSG( "Available options:\n");
    MSG( " -h print this help\n");
    MSG( " -l generate a new guid.json file\n");
    MSG( " -p print the pico cell gateway id\n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv)
{
    int i;
    uint8_t uid[8];  //unique id
    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    while ((i = getopt (argc, argv, "hplq:")) != -1) {
        switch (i) {
            case 'h':
                usage();
                return EXIT_FAILURE;
                break;

            case 'l':
                lgw_connect(false);
                lgw_reg_GetUniqueId(&uid[0]);
                FILE *f;
                f = fopen("guid.json", "w");
                fprintf(f, "/* Put there parameters that are different for each gateway (eg. pointing one gateway to a test server while the others stay in production) */\n");
                fprintf(f, "{\"gateway_conf\": {\n     \"gateway_ID\": \"%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x\" \n     }\n}", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);
                fclose(f);
                return EXIT_SUCCESS;

            case 'p':
                lgw_connect(false);
                lgw_reg_GetUniqueId(&uid[0]);
                printf("/* Put there parameters that are different for each gateway (eg. pointing one gateway to a test server while the others stay in production) */\n");
                printf("{\"gateway_conf\": {\n     \"gateway_ID\": \"%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x\" \n     }\n}", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);

                return EXIT_SUCCESS;


            default:
                MSG("ERROR: argument parsing use -h option for help\n");
                usage();
                return EXIT_FAILURE;
        }
    }
}

/* --- EOF ------------------------------------------------------------------ */


