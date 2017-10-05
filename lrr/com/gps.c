/*
* Copyright (C) Actility, SA. All Rights Reserved.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 only, as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License version 2 for more details (a copy is
* included at /legal/license.txt).
*
* You should have received a copy of the GNU General Public License
* version 2 along with this work; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*
* Please contact Actility, SA.,  4, rue Ampere 22300 LANNION FRANCE
* or visit www.actility.com if you need additional
* information or have any questions.
*/

/*! @file gps.c
 *
 * This thread is only for managing GPS device, not the data read.
 * It controls open, read, decode and close operation.
 * Once the data are read, they are available in the other threads to be used.
 */

#ifdef  WITH_GPS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <ctype.h>
#ifndef MACOSX
#include <malloc.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <termios.h>

#include "rtlbase.h"
#include "rtlimsg.h"
#include "rtllist.h"
#include "rtlhtbl.h"

#include "timeoper.h"

#include "semtech.h"
#include "headerloramac.h"

#include "xlap.h"
#include "define.h"
#include "infrastruct.h"
#include "struct.h"
#include "cproto.h"
#include "extern.h"

#define GPS_N_SAT_MIN   2
#define GPS_HDOP_MAX    10


int             GpsFd = -1;
char          * GpsDevice = "/dev/nmea";    /* Default GPS device if not defined in lrr.ini */

static int      count_thread_gps;
static int      _GpsStarted;
static struct   termios gps_ttyopt_restore; /* GPS serial port options used for saving/restoring */

static void     cleanup_thread_gps(void * a);
static void     InitGps();
static void     RemoveGps();
static void     GpsMainLoop();
static void     GpsUpdated(struct timespec *utc_from_gps, struct timespec * ubxtime_from_gps, LGW_COORD_T *loc_from_gps,char *rawbuff);
#ifdef REF_DESIGN_V2
static int      GpsEnable(char *gps_path, int *fd_gps);
static void     GpsDebugTermios(struct termios ttyopt, unsigned int verbose_lvl);
#endif /* REF_DESIGN_V2 */


/*
 * Functions required:
 *  GpsStarted() : return status of the gps thread
 *  GpsStart()   : start the gps thread
 *  GpsStop()    : stop the gps thread
 *  GpsRun()     : main loop of the gps thread
 * 
 *  In main.c, functions to control gps thread:
 *  StartGpsThread()
 *  CancelGpsThread()
 *  ReStartGpsThread()
 *  StopGpsThread()
 * 
 * In main.c, variable to control gps thread:
 *  int GpsThreadStarted
 *  int GpsThreadStopped
 *  pthread_attr_t GpsThreadAt
 *  pthread_t GpsThread;
 * 
 * In main.c, to test if gps thread is started:
 *  ret = GpsStarted();
 *  if (!ret || GpsThreadStopped || GpsThreadStarted) {
 *      // Do something
 *  }
 *
 *
 * What about:
 *  GpsGetInfo (and its equivalent for Tektelic) => Purpose is to use this function to get the current position data
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */



int GpsStart()
{
    _GpsStarted = 1;
    return 0;
}

int GpsStarted()
{
    return _GpsStarted;
}

void GpsStop()
{
    RTL_TRDBG(1, "GPS stopping...\n");
    RemoveGps();
    _GpsStarted = 0;

}

static void cleanup_thread_gps(void * a)
{
    RTL_TRDBG(0, "stop lrr.x/gps th=%lx pid=%d count=%d\n", (long)pthread_self(), getpid(), --count_thread_gps);
    RemoveGps();
}

void * GpsRun(void * param)
{

    pthread_cleanup_push(cleanup_thread_gps, NULL);
    RTL_TRDBG(0, "start lrr.x/gps th=%lx pid=%d count=%d\n", (long)pthread_self(), getpid(), ++count_thread_gps);

    InitGps();
    GpsMainLoop();
    pthread_cleanup_pop(0);

    return NULL;
}


static void InitGps()
{
// For tektelic no access to device, just use "/tmp/position" for coordinates
#ifdef  TEKTELIC
    return;
#endif

    if (GpsFd != -1)
        return;

    if (!UseGps) {
        //RTL_TRDBG(1,"GPS do not use gps device '%s'\n",GpsDevice);
        return;
    }

#ifdef REF_DESIGN_V2
    int err = GpsEnable(GpsDevice, &GpsFd);  
#else
    int err = lgw_gps_enable(GpsDevice, NULL, 0, &GpsFd);  
#endif /* REF_DESIGN_V2 */
    if (err != LGW_GPS_SUCCESS || GpsFd < 0)
    {
        RTL_TRDBG(0, "GPS cannot open gps device '%s'\n", GpsDevice);
        GpsFd = -1;
/*
        UseGps      = 0;
        UseGpsPosition  = 0;
        UseGpsTime  = 0;
*/
        return;
    }

    RTL_TRDBG(1, "GPS gps device '%s' opened fd=%d\n", GpsDevice, GpsFd);
    /*
    rtl_pollAdd(MainTbPoll, GpsFd, CB_GpsEvent, NULL, NULL, NULL);
    rtl_pollSetEvt(MainTbPoll, GpsFd, POLLIN);
    */
}

#ifdef REF_DESIGN_V2
static int GpsEnable(char *gps_path, int *fd_gps)
{
    int             x;      /* return value for sx1301ar_* functions */
    struct termios  ttyopt; /* serial port options */

    /* Init time reference (global val) */
    for (x=0; x<LgwBoard; x++)
        Gps_time_ref[x] = sx1301ar_init_tref();

    /* Open TTY device for GPS */
    *fd_gps = open(gps_path, O_RDWR | O_NOCTTY);
    if (*fd_gps <= 0) {
        RTL_TRDBG(0, "ERROR: opening TTY port for GPS failed - %s (%s)\n", strerror(errno), gps_path);
        return -1;
    }

    /* Save current TTY serial port parameters */
    RTL_TRDBG(4, "GPS saving current terminal attributes\n");
    x = tcgetattr(*fd_gps, &gps_ttyopt_restore);
    if (x != 0) {
        RTL_TRDBG(0, "ERROR: failed to get GPS serial port parameters\n");
        return -1;
    }
    /* Get TTY serial port parameters */
    x = tcgetattr(*fd_gps, &ttyopt);
    if ( x != 0 )
    {
        RTL_TRDBG(0, "ERROR: failed to get GPS serial port parameters\n");
        return -1;
    }

    /* Update TTY terminal parameters */
#ifdef HAL_VERSION_5
    /* The following configuration should allow to:
        - Get ASCII NMEA messages
        - Get UBX binary messages
        - Send UBX binary commands
        Note: as binary data have to be read/written, we need to disable
          various character processing to avoid loosing data */
    /* Control Modes */
    ttyopt.c_cflag |= CLOCAL;   /* local connection, no modem control */
    ttyopt.c_cflag |= CREAD;    /* enable receiving characters */
    ttyopt.c_cflag |= CS8;      /* 8 bit frames */
    ttyopt.c_cflag &= ~PARENB;  /* no parity */
    ttyopt.c_cflag &= ~CSTOPB;  /* one stop bit */
    /* Input Modes */
    ttyopt.c_iflag |= IGNPAR;   /* ignore bytes with parity errors */
    ttyopt.c_iflag &= ~ICRNL;   /* do not map CR to NL on input*/
    ttyopt.c_iflag &= ~IGNCR;   /* do not ignore carriage return on input */
    ttyopt.c_iflag &= ~IXON;    /* disable Start/Stop output control */
    ttyopt.c_iflag &= ~IXOFF;   /* do not send Start/Stop characters */
    /* Output Modes */
    ttyopt.c_oflag = 0;     /* disable everything on output as we only write binary */
    /* Local Modes */
    ttyopt.c_lflag &= ~ICANON;  /* disable canonical input - cannot use with binary input */
    ttyopt.c_lflag &= ~ISIG;    /* disable check for INTR, QUIT, SUSP special characters */
    ttyopt.c_lflag &= ~IEXTEN;  /* disable any special control character */
    ttyopt.c_lflag &= ~ECHO;    /* do not echo back every character typed */
    ttyopt.c_lflag &= ~ECHOE;   /* does not erase the last character in current line */
    ttyopt.c_lflag &= ~ECHOK;   /* do not echo NL after KILL character */
    /* Settings for non-canonical mode:
       read will block for until the lesser of VMIN or requested chars have been received */
    ttyopt.c_cc[VMIN]  = 1;     /* byte granularity at init time to ensure we get all UBX responses */
    ttyopt.c_cc[VTIME] = 0;

#else
    ttyopt.c_cflag |= CLOCAL; /* local connection, no modem control */
    ttyopt.c_cflag |= CREAD; /* enable receiving characters */
    ttyopt.c_cflag |= CS8; /* 8 bit frames */
    ttyopt.c_cflag &= ~PARENB; /* no parity */
    ttyopt.c_cflag &= ~CSTOPB; /* one stop bit */
    ttyopt.c_iflag |= IGNPAR; /* ignore bytes with parity errors */
    ttyopt.c_iflag |= ICRNL; /* map CR to NL */
    ttyopt.c_iflag |= IGNCR; /* Ignore carriage return on input */
    ttyopt.c_lflag |= ICANON; /* enable canonical input */
#endif /* HAL_VERSION_5 */
    GpsDebugTermios(ttyopt, 4); /* Log termios flag configuration at verbose lvl 4 */

    /* Set new TTY serial ports parameters */
    x = tcsetattr( *fd_gps, TCSANOW, &ttyopt );
    if (x != 0) {
        RTL_TRDBG(0, "ERROR: failed to update GPS serial port parameters\n");
        return -1;
    }

    tcflush(*fd_gps, TCIOFLUSH);
    RTL_TRDBG(1, "GPS enabled for synchronization using device %s\n", gps_path);
    
    return 0;
} /* GpsEnable() */
#endif /* REF_DESIGN_V2 */

static void RemoveGps()
{
    struct termios ttyopt;
    int x;

    if (GpsFd >= 0)
    {
        rtl_pollRmv(MainTbPoll,GpsFd);
        /* Restore terminal attributes */
        RTL_TRDBG(4, "GPS restoring terminal attributes\n");
        tcsetattr(GpsFd, TCSANOW, &gps_ttyopt_restore);
        x = tcgetattr(GpsFd, &ttyopt);
        if (x != 0) {
            RTL_TRDBG(0, "ERROR: failed to get GPS serial port parameters\n");
        }
        close(GpsFd);
        GpsFd = -1;
    }
/*
    UseGps      = 0;
    UseGpsPosition  = 0;
    UseGpsTime  = 0;
*/
} /* RemoveGps() */


static void GpsMainLoop()
{
    char            buff[256];
    char            svbuff[256];
    ssize_t         nb_char;
    int             x;
    LGW_COORD_T     loc_from_gps;
    LGW_GPSMSG_T    msg_type;
    struct timespec utc_from_gps;
#ifdef HAL_VERSION_5
    LGW_UBXMSG_T    ubxmsg_type;
    struct timespec gpstime;
    size_t          rd_idx = 0;
    size_t          wr_idx = 0; /* pointer to end of chars in buffer */
    size_t          frame_size = 0;
    size_t          frame_end_idx = 0;
    char            resstr[64] = {0};
    bool            trig_sync = false;
    bool            trig_loc = false;
#endif /* HAL_VERSION_5 */
#ifdef REF_DESIGN_V2
    int             nsat = 0;
    float           hdop = 0.0;
#endif /* REF_DESIGN_V2 */

#ifdef HAL_VERSION_5
    memset(&gpstime, 0, sizeof(gpstime));
#endif /* HAL_VERSION_5 */

    memset(buff, 0, sizeof(buff));
    /* Start thread main loop */
    while (!ServiceStopped && !GpsThreadStopped)
    {

#ifdef HAL_VERSION_5
        /* Wait for NMEA/UBX frames (blocking non-canonical read on serial port) */
        nb_char = read(GpsFd, buff + wr_idx, LGW_GPS_MIN_MSG_SIZE);
        /*
        RTL_TRDBG(4, "GPS read gps sz=%d '%s'\n", nb_char, (buff + wr_idx));
        sprintf(hexgps, "GPS read hexdump: ");
        for (i = 0; i < nb_char; i++) {
            sprintf(hexgps, "%02X ", buff[i + wr_idx]);
        }
        RTL_TRDBG(4, "%s\n", hexgps);
        */
#else
        nb_char = read(GpsFd, buff, sizeof(buff)-10);
        RTL_TRDBG(4, "GPS read gps sz=%d '%s'\n", nb_char, buff);
#endif /* HAL_VERSION_5 */

        if (nb_char <= 0)
        {
            RTL_TRDBG(0, "GPS device %s with fd=%d  read() returned %d\n", GpsDevice, GpsFd, nb_char);
            continue;
        }

        // FIXME, in case of HAL_VERSION_5 buffer fill method
        memset(svbuff, 0, sizeof (svbuff));
        //strcpy(svbuff, buff);
        memcpy(&svbuff, &buff, nb_char);

#ifdef REF_DESIGN_V2

#ifdef HAL_VERSION_5
        wr_idx += (size_t)nb_char;

        /*******************************************
         * Scan buffer for UBX/NMEA sync chars and *
         * attempt to decode frame if one is found *
         *******************************************/
        rd_idx = 0;
        frame_end_idx = 0;
        while (rd_idx < wr_idx)
        {
            frame_size = 0;
            /* Scan buffer for UBX sync char */
            if (buff[rd_idx] == (char)LGW_GPS_UBX_SYNC_CHAR)
            {
                /************************
                 *  Found UBX sync char *
                 ************************/

                /* Determine message type */
                ubxmsg_type = UBX_UNDEFINED;
                x = sx1301ar_get_ubx_type(&buff[rd_idx], (wr_idx - rd_idx), &ubxmsg_type, &frame_size);
                if (x != 0) {
                    RTL_TRDBG(1, "ERROR: sx1301ar_get_ubx_type failed; %s\n", sx1301ar_err_message(sx1301ar_errno));
                    GpsStop();
                }

                /* UBX parsing tree */
                /* /!\ This code works on uBlox GPS modules, you might have to modify the code if used with another brand of GPS receivers */
                if (ubxmsg_type == UBX_NAV_TIMEGPS) {
                    /* Parse NAV-TIMEGPS frame */
                    x = sx1301ar_parse_ubx_timegps(&buff[rd_idx], frame_size, &gpstime);
                    if (x != 0) {
                        RTL_TRDBG(1, "ERROR: sx1301ar_parse_ubx_timegps failed; %s\n", sx1301ar_err_message(sx1301ar_errno));
                        GpsStop();
                    }
                    RTL_TRDBG(4, "GPS UBX frame type UBX_NAV_TIMEGPS (%d bytes) gpstime=%lu,%09lu seconds\n", frame_size, gpstime.tv_sec, gpstime.tv_nsec);
                    //FIXME GpsUpdated(NULL, &gpstime, NULL, NULL);
                }
            }
            else if (buff[rd_idx] == (char)LGW_GPS_NMEA_SYNC_CHAR)
            {
                /************************
                 * Found NMEA sync char *
                 ************************/

                /* scan for NMEA end marker (LF = 0x0a) */
                char * nmea_end_ptr = memchr(&buff[rd_idx], (int)0x0a, (wr_idx - rd_idx));

                if (nmea_end_ptr)
                {
                    /* found end marker */
                    frame_size = nmea_end_ptr - &buff[rd_idx] + 1;
                    memcpy(&svbuff, &buff[rd_idx], frame_size-2);  /* -2 is to avoid the end of frame EOL char for display (0x0A 0x0D) */

                    /* Determine message type */
                    msg_type = NMEA_UNDEFINED;
                    x = sx1301ar_get_nmea_type(&buff[rd_idx], frame_size, &msg_type);
                    if (x != 0) {
                        RTL_TRDBG(1, "ERROR: sx1301ar_get_nmea_type failed; %s\n", sx1301ar_err_message(sx1301ar_errno));
                        GpsStop();
                    }

                    /* NMEA parsing tree */
                    /* /!\ This code works on uBlox GPS modules, you might have to modify the code if used with another brand of GPS receivers */
                    if (msg_type == NMEA_RMC)
                    {
                        RTL_TRDBG(4, "GPS NMEA RMC frame (%d bytes): \"%s\"\n", frame_size, svbuff);
                        //utc_fresh = false;

                        /* Parse RMC frame */
                        x = sx1301ar_parse_rmc(&buff[rd_idx], frame_size, &utc_from_gps, &loc_from_gps);
                        if (x == 0)
                        {
                            //FIXME GpsUpdated(&utc_from_gps, NULL, &loc_from_gps, svbuff);
                            //utc_fresh = true;
                            trig_sync = true; /* We need to trigger a sync each time we get an RMC sentence */
                        }
                        else if( x > 0 )
                        {
                            switch (x)
                            {
                                case 1:
                                    sprintf(resstr, "Bad checksum");
                                    break;
                                case 2:
                                    sprintf(resstr, "Bad format");
                                    break;
                                case 4:
                                    sprintf(resstr, "Invalid");
                                    break;
                                case 8:
                                    sprintf(resstr, "Incomplete");
                                    break;
                                default:
                                    break;
                            }
                            RTL_TRDBG(1, "GPS NMEA_RMC: parsing RMC sentence returned - %s\n", resstr);
                        }
                        else if( x < 0 )
                        {
                            RTL_TRDBG(0, "ERROR: sx1301ar_parse_rmc failed; %s\n", sx1301ar_err_message(sx1301ar_errno));
                            GpsStop();
                        }
                    } /* end if msg_type == NMEA_RMC */
                    else if (msg_type == NMEA_GGA)
                    {
                        RTL_TRDBG(4, "GPS NMEA GGA frame (%d bytes): \"%s\"\n", frame_size, svbuff);

                        /* Parse GGA frame */
                        x = sx1301ar_parse_gga(&buff[rd_idx], frame_size, &loc_from_gps, &nsat, &hdop);
                        if (x == 0)
                        {
                            /* If metrics are good enough, trigger a sync */
                            /* /!\ ASSUMES GPS SENDS RMC BEFORE GGA, CHECK YOUR GPS MODULE */
                            if ( (nsat >= GPS_N_SAT_MIN) && (hdop < GPS_HDOP_MAX) )
                            {
                                trig_loc = true;
                                RTL_TRDBG(4, "GPS NMEA GGA: nbsat=%d hdop=%f\n", nsat, hdop);
                            }
                        }
                        else if (x > 0)
                        {
                            switch (x)
                            {
                                case 1:
                                    sprintf(resstr, "Bad checksum");
                                    break;
                                case 2:
                                    sprintf(resstr, "Bad format");
                                    break;
                                case 4:
                                    sprintf(resstr, "Invalid");
                                    break;
                                case 8:
                                    sprintf(resstr, "Incomplete");
                                    break;
                                default:
                                    break;
                            }
                            RTL_TRDBG(1, "GPS NMEA_GGA: parsing GGA sentence returned - %s\n", resstr);
                        }
                        else if (x < 0)
                        {
                            RTL_TRDBG(0, "ERROR: sx1301ar_parse_gga failed; %s\n", sx1301ar_err_message(sx1301ar_errno));
                            GpsStop();
                        }
                    } /* end if msg_type == NMEA_GGA */
                } /* end if nmea_end_ptr */
            } /* end if LGW_GPS_NMEA_SYNC_CHAR */

            if (frame_size > 0) {
                /* At this point message is a checksum verified frame
                we're processed or ignored. Remove frame from buffer */
                rd_idx += frame_size;
                frame_end_idx = rd_idx;
            }
            else {
                rd_idx++;
            }
        } /* end while */

        if (frame_end_idx) {
            /* Frames have been processed. Remove bytes to end of last processed frame */
            memcpy(buff, &buff[frame_end_idx], wr_idx - frame_end_idx );
            wr_idx -= frame_end_idx;
        }

        /* Prevent buffer overflow */
        if ( (sizeof (buff) - wr_idx) < LGW_GPS_MIN_MSG_SIZE ) {
            memcpy(buff, &buff[LGW_GPS_MIN_MSG_SIZE], wr_idx - LGW_GPS_MIN_MSG_SIZE );
            wr_idx -= LGW_GPS_MIN_MSG_SIZE;
        }
        
        if (trig_sync == true)
        {
            trig_sync = false;
            GpsUpdated(&utc_from_gps, &gpstime, &loc_from_gps, svbuff);
        }
        if (trig_loc == true)
        {
            trig_loc = false;
            GpsUpdated(NULL, NULL, &loc_from_gps, svbuff);
        }



#else /* == if not HAL_VERSION_5 */

        /* Determine message type */
        msg_type = NMEA_UNDEFINED;
        x = sx1301ar_get_nmea_type(buff, sizeof (buff), &msg_type );
        if (x != 0)
            GpsStop();

        /* NMEA parsing tree */
        /* /!\ This code works on uBlox GSP modules, you might have to modify the code if used with another brand of GPS receivers */
        if (msg_type == NMEA_RMC)
        {
            /* Parse RMC frame */
            x = sx1301ar_parse_rmc(buff, sizeof (buff), &utc_from_gps, &loc_from_gps);
            if (x == 0)
            {
                GpsUpdated(&utc_from_gps, NULL, &loc_from_gps, svbuff);
            }
            else
            {
                x = sx1301ar_parse_rmc(buff, sizeof (buff), &utc_from_gps, NULL);
                if (x == 0) {
                    GpsUpdated(&utc_from_gps, NULL, NULL, NULL);
                }
            }
        }
        else if (msg_type == NMEA_GGA)
        {
            /* Parse GGA frame, to get altitude */
            x = sx1301ar_parse_gga(buff, sizeof (buff), &loc_from_gps, &nsat, &hdop);
            if (x == 0)
            {
                GpsUpdated(NULL, NULL, &loc_from_gps, svbuff);
                RTL_TRDBG(4, "GPS NMEA_GGA: nbsat=%d hprecision=%f\n", nsat, hdop);
            }
        }
#endif /* HAL_VERSION_5 */
#else /* == if not REF_DESIGN_V2 */
        msg_type = lgw_parse_nmea(buff, sizeof (buff));
        if (msg_type == NMEA_RMC)
        { // gps + time
            RTL_TRDBG(4, "GPS NMEA_RMC='%s'\n", buff);

            x = lgw_gps_get(&utc_from_gps, &loc_from_gps, NULL);
            if (x != LGW_GPS_ERROR)
            {   // we have time and loc
                GpsUpdated(&utc_from_gps, NULL, &loc_from_gps,svbuff);
            }
            else
            {   // can not get time + loc => try time only
                x = lgw_gps_get(&utc_from_gps, NULL, NULL);
                if (x != LGW_GPS_ERROR)
                {   // we have time only
                    GpsUpdated(&utc_from_gps, NULL, NULL, NULL);
                }
            }
        }

        if (msg_type == NMEA_GGA) {
            RTL_TRDBG(4, "GPS NMEA_GGA='%s'\n", buff);
        }
#endif /* REF_DESIGN_V2 */


    } /* while main loop */

}


// we assume than at least utc_from_gps is not NULL loc_from_gps can be NULL
// if not used, ubxtime_from_gps (HAL v5 feature) should be passed to NULL
static void GpsUpdated(struct timespec * utc_from_gps, struct timespec * ubxtime_from_gps, LGW_COORD_T * loc_from_gps, char * rawbuff)
{
    static  time_t  prevhlinux      = 0;
    time_t          currhlinux      = rtl_tmmsmono();
    static  time_t  prevhgps        = 0;
    time_t          currhgps        = 0;
    static  u_int   nbcall          = UINT_MAX;      // 1sec
    static  float   deviation       = 0.0;
    static  int     savedata        = -1;
    static  u_int   TotUtcUpdate    = 0;
    static  u_int   TotLocUpdate    = 0;

#ifdef HAL_VERSION_5
    static struct timespec ubx_t;
    if (ubxtime_from_gps) {
        memcpy(&ubx_t, ubxtime_from_gps, sizeof (*ubxtime_from_gps));
    }
#endif

    if (savedata == -1)
    {
        //FIX3993: now ~/var/log/lrr is a ramdir we can write data 
        savedata = CfgInt(HtVarLrr, "lrr", -1, "savegpsdata", 0);
    }

    if (utc_from_gps == NULL && loc_from_gps == NULL)
        return;

    if (utc_from_gps)
        currhgps    = utc_from_gps->tv_sec;

    RTL_TRDBG(3, "GPSUpdated(time=%u,loc=%s) cnt=%u nbcall=%u\n", currhgps,
            loc_from_gps?"ok":"ko", GpsUpdateCnt, nbcall);

    if (utc_from_gps)
    {
        //
        //  each time signals the radio thread
        //
        GpsUpdateCnt++;
        nbcall++;
        TotUtcUpdate++;
        if (UseGpsTime)
        {
            t_imsg * msg;

            msg = rtl_imsgAlloc(IM_DEF,IM_LGW_GPS_TIME,NULL,0);
            if (msg)
            {
#ifdef HAL_VERSION_5
                t_gpstime gpstime;
                memcpy(&gpstime.utc, utc_from_gps, sizeof (*utc_from_gps));
                memcpy(&gpstime.ubx, &ubx_t, sizeof (ubx_t));
                /*RTL_TRDBG(4, "GPS sync: utc: %lu, %lu, ubx: %lu, %lu\n", utc_from_gps->tv_sec, utc_from_gps->tv_nsec, \
                            ubx_t.tv_sec, ubx_t.tv_nsec);*/
                void * data = &gpstime;
                int sz = sizeof(gpstime);
#else
                void    *data   = utc_from_gps;
                int sz  = sizeof(*utc_from_gps);
#endif
                if (rtl_imsgCpyData(msg,data,sz) != msg) {
                    rtl_imsgFree(msg);
                } else {
                    rtl_imsgAdd(LgwQ, msg);
                }
            }
        }
        //
        //  each 60s compute the linux time vs GPS deviation
        //
        if (nbcall && (nbcall % 60) == 0)
        {
            deviation  = (float)((currhlinux-prevhlinux)) / (float)((currhgps-prevhgps)*1000);
            RTL_TRDBG(3, "GPS linux/tmmsmono vs GPS deviation=%f\n", deviation);
        }
        prevhlinux  = currhlinux;
        prevhgps    = currhgps;
        //FIX3993: now ~/var/log/lrr is a ramdir we can write data 
        if (savedata && (savedata == 1 || (nbcall % savedata) == savedata))
        {
            FILE    *f;
            char    tmp[512];
            char    dst[512];
            sprintf(tmp,"%s/var/log/lrr/gpstime.tmp",RootAct);
            sprintf(dst,"%s/var/log/lrr/gpstime.txt",RootAct);
            f = fopen(tmp,"w");
            if (f)
            {
                struct  timeval tv;
                char    when[128];

                memset(&tv, 0, sizeof (tv));
                tv.tv_sec = currhgps;
                rtl_gettimeofday_to_iso8601date(&tv, NULL, when);
                fprintf(f, "gpsutcsec=%u\n", (u_int)currhgps);
                fprintf(f, "gpsutciso=%s\n", when);

                memset(&tv, 0, sizeof (tv));
                gettimeofday(&tv, NULL);
                rtl_gettimeofday_to_iso8601date(&tv, NULL, when);
                fprintf(f, "hostutcsec=%u\n", (u_int)tv.tv_sec);
                fprintf(f, "hostutciso=%s\n", when);

                fprintf(f, "clkdev=%f\n", deviation);
                fprintf(f, "ppscnt=%u\n", TotUtcUpdate);
                fclose(f);
                rename(tmp, dst);
            }
        }
    } /* end if utc_from_gps */

    /*  now we treat GPS location */
    if (loc_from_gps == NULL)
        return;

    GpsLatt = loc_from_gps->lat;
    GpsLong = loc_from_gps->lon;
#ifdef REF_DESIGN_V2
    // using sxarray HAL, altitude is not provided when decoding RMC frames
    // utc_from_gps == NULL means GGA frame received
    if (utc_from_gps == NULL)
    {
        RTL_TRDBG(4, "GPS set alt from GGA frame alt=%d\n", loc_from_gps->alt);
        GpsAlti = loc_from_gps->alt;
        // GGA frame used only to get altitude
        return;
    }
#else
    GpsAlti = loc_from_gps->alt;
#endif /* defined(REF_DESIGN_V2) */

    GpsPositionOk = 1;
    TotLocUpdate++;

    RTL_TRDBG(4, "GPS location lat=%f lon=%f alt=%d\n", loc_from_gps->lat, loc_from_gps->lon, loc_from_gps->alt);

    /* each 120s verify if the GPS position has changed */
    if (UseGpsPosition && (nbcall % 120) == 0)
    {
        if (
            ABS(LrrLat - loc_from_gps->lat) > 0.0005 
            ||
            ABS(LrrLon - loc_from_gps->lon) > 0.0005 
            ||
            ABS(LrrAlt - loc_from_gps->alt) > 10)
        {
            GpsPositionUpdated(loc_from_gps);
            LrrLat = loc_from_gps->lat;
            LrrLon = loc_from_gps->lon;
#ifdef REF_DESIGN_V2
            if (utc_from_gps == NULL)
                LrrAlt = loc_from_gps->alt;
#else
            LrrAlt = loc_from_gps->alt;
#endif
        }
    }

    //FIX3993: now ~/var/log/lrr is a ramdir we can write data 
    if (UseGpsPosition && savedata 
        && (savedata == 1 || (nbcall % savedata) == savedata))
    {
        FILE    *f;
        char    tmp[512];
        char    dst[512];
        sprintf(tmp, "%s/var/log/lrr/gpslocation.tmp", RootAct);
        sprintf(dst, "%s/var/log/lrr/gpslocation.txt", RootAct);
        f = fopen(tmp, "w");
        if (f)
        {
            fprintf(f, "rawlat=%f\n", GpsLatt);
            fprintf(f, "rawlon=%f\n", GpsLong);
            fprintf(f, "rawalt=%d\n", GpsAlti);
            fprintf(f, "lrrlat=%f\n", LrrLat);
            fprintf(f, "lrrlon=%f\n", LrrLon);
            fprintf(f, "lrralt=%d\n", LrrAlt);
            fprintf(f, "loccnt=%u\n", TotLocUpdate);

            if (rawbuff && strlen(rawbuff))
                fprintf(f, "rawbuf=%s\n", rawbuff);

            fclose(f);
            rename(tmp, dst);
        }
    }
} /* GpsUpdated() */



#ifdef REF_DESIGN_V2
static void GpsDebugTermios(struct termios ttyopt, unsigned int verbose_lvl)
{
    RTL_TRDBG(verbose_lvl, "GPS tty flags CLOCAL = %d\n", (ttyopt.c_cflag & CLOCAL)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags CREAD  = %d\n", (ttyopt.c_cflag & CREAD)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags CS8    = %d\n", (ttyopt.c_cflag & CS8)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags PARENB = %d\n", (ttyopt.c_cflag & PARENB)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags CSTOPB = %d\n", (ttyopt.c_cflag & CSTOPB)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags IGNPAR = %d\n", (ttyopt.c_iflag & IGNPAR)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags ICRNL  = %d\n", (ttyopt.c_iflag & ICRNL)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags IGNCR  = %d\n", (ttyopt.c_iflag & IGNCR)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags IXON   = %d\n", (ttyopt.c_iflag & IXON)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags IXOFF  = %d\n", (ttyopt.c_iflag & IXOFF)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags ICANON = %d\n", (ttyopt.c_lflag & ICANON)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags ISIG   = %d\n", (ttyopt.c_lflag & ISIG)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags IEXTEN = %d\n", (ttyopt.c_lflag & IEXTEN)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags ECHO   = %d\n", (ttyopt.c_lflag & ECHO)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags ECHOE  = %d\n", (ttyopt.c_lflag & ECHOE)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags ECHOK  = %d\n", (ttyopt.c_lflag & ECHOK)?1:0);
    RTL_TRDBG(verbose_lvl, "GPS tty flags cc[VMIN] = %d\n", ttyopt.c_cc[VMIN]);
    RTL_TRDBG(verbose_lvl, "GPS tty flags cc[VTIME] = %d\n", ttyopt.c_cc[VTIME]);
}
#endif /* REF_DESIGN_V2 */


#else /* If NOT defined WITH_GPS */
int             GpsFd = -1;
char          * GpsDevice = "/dev/null";    /* Default device if no WITH_GPS flag defined when compiling */
#endif /* WITH_GPS */

/* vim: ft=c: set et ts=4 sw=4: */
