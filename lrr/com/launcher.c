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

/*! @file main.c
 *
 */
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
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>

#include "rtlbase.h"
#include "rtlimsg.h"
#include "rtllist.h"
#include "rtlhtbl.h"

char	*CfgStr(void *ht,char *sec,int index,char *var,char *def);
int	CfgInt(void *ht,char *sec,int index,char *var,int def);
int CfgCBIniLoad(void *user,const char *section,const char *name,const char *value);

char	*RootAct	= NULL;
char	*System		= NULL;

char	*SerialMode	= NULL;
char	*BoardType	= NULL;
char	*ServiceLrr	= NULL;

void	*HtVarLrr;

#ifdef	WIRMAV2
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System		= "wirmav2";
	SerialMode	= "spi";
	BoardType	= "x1";
	RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		RootAct	= "/mnt/fsuser-1/actility";
		setenv	("ROOTACT",strdup(RootAct),1);
	}
}
#endif

#ifdef	WIRMAMS
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System		= "wirmams";
	SerialMode	= "spi";
	BoardType	= "ms";
	RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		RootAct	= "/user/actility";
		setenv	("ROOTACT",strdup(RootAct),1);
	}
}
#endif

#ifdef	WIRMAAR
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System		= "wirmaar";
	SerialMode	= "spi";
	BoardType	= "x8";
	RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		RootAct	= "/user/actility";
		setenv	("ROOTACT",strdup(RootAct),1);
	}
}
#endif

#ifdef	WIRMANA
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System		= "wirmana";
	SerialMode	= "spi";
	BoardType	= "x1";
	RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		RootAct	= "/user/actility";
		setenv	("ROOTACT",strdup(RootAct),1);
	}
}
#endif

#ifdef	NATRBPI
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System		= "natrbpi";
	SerialMode	= "usb";
	BoardType	= "x1";
}
#endif

#ifdef	IR910
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "ir910";
	SerialMode	= "usb";
	BoardType	= "x1";
}
#endif

#ifdef	CISCOMS
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "ciscoms";
	SerialMode	= "spi";
	BoardType	= "x8";
}
#endif

#ifdef	TEKTELIC
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "tektelic";
	SerialMode	= "spi";
	BoardType	= "x8";
}
#endif

#ifdef	MTAC
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "mtac";
	SerialMode	= "spi";
	BoardType	= "x1";
}
#endif

#ifdef	MTAC_USB
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "mtac";
	SerialMode	= "spi";
	BoardType	= "x1";
}
#endif

#ifdef	MTAC_USB
void	InitSystem()
{
	System	= "mtac";
	SerialMode	= "usb";
	BoardType	= "x1";
}
#endif

#ifdef	MTCAP
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "mtcap";
	SerialMode	= "spi";
	BoardType	= "x1";
}
#endif

#ifdef  FCMLB
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System          = "fcmlb";
        SerialMode      = "spi";
        BoardType       = "x1";
        RootAct = getenv("ROOTACT");
        if      (!RootAct)
        {
                RootAct = "/home/actility";
                setenv  ("ROOTACT",strdup(RootAct),1);
        }
}
#endif

#ifdef  FCPICO
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System          = "fcpico";
        SerialMode      = "spi";
        BoardType       = "x1";
        RootAct = getenv("ROOTACT");
        if      (!RootAct)
        {
                RootAct = "/home/actility";
                setenv  ("ROOTACT",strdup(RootAct),1);
        }
}
#endif

#ifdef  FCLAMP
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System          = "fclamp";
        SerialMode      = "spi";
        BoardType       = "x1";
        RootAct = getenv("ROOTACT");
        if      (!RootAct)
        {
                RootAct = "/home/actility";
                setenv  ("ROOTACT",strdup(RootAct),1);
        }
}
#endif

#ifdef  FCLOC
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System          = "fcloc";
        SerialMode      = "spi";
        BoardType       = "x8";
        RootAct = getenv("ROOTACT");
        if      (!RootAct)
        {
                RootAct = "/home/actility";
                setenv  ("ROOTACT",strdup(RootAct),1);
        }
}
#endif

#ifdef	RFILR
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "rfilr";
	SerialMode	= "usb";
	BoardType	= "x1";
}
#endif

#ifdef  OIELEC
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System  = "oielec";
        SerialMode      = "spi";
        BoardType       = "x1";
        RootAct = getenv("ROOTACT");
        if      (!RootAct)
        {
                RootAct = "/home/actility";
                setenv  ("ROOTACT",strdup(RootAct),1);
        }
}
#endif

#ifdef  GEMTEK
#define	SYSTEM_DEFINED
void    InitSystem()
{
       System  = "gemtek";
       SerialMode      = "spi";
       BoardType       = "x1";
       RootAct = getenv("ROOTACT");
       if      (!RootAct)
       {
               RootAct = "/home/actility";
               setenv  ("ROOTACT",strdup(RootAct),1);
       }
}
#endif

#ifndef SYSTEM_DEFINED
void    InitSystem()
{
#warning "you are compiling the LRR for linux-x86 generic target system"
#warning "this implies the use of a Semtech Picocell connected with ttyACMx"
        System  = "linux-x86";
        SerialMode      = "tty";
        BoardType       = "x1";
        RootAct = getenv("ROOTACT");
        if      (!RootAct)
        {
                RootAct = "/home/actility";
                setenv  ("ROOTACT",strdup(RootAct),1);
        }
}
#endif

int     main(int argc,char *argv[])
{
	char	fdefaultexe[PATH_MAX];
	char	factiveexe[PATH_MAX];
	char	*fexe;
	char	fparams[PATH_MAX];
	FILE	*log;
	FILE	*f;

/*
	freopen	("/tmp/lrrlauncher.log","w",stdout);
	freopen	("/tmp/lrrlauncher.log","w+",stderr);
*/

	log	= fopen("/tmp/lrrlauncher.log","w");

	HtVarLrr	= rtl_htblCreateSpec(25,NULL,
						HTBL_KEY_STRING|HTBL_FREE_DATA);
	if	(!HtVarLrr)
	{
		fprintf(log,"cannot alloc internal resources (htables)\n");
		exit(1);
	}

	InitSystem();

	if	(!System)
	{
		fprintf(log,"SYSTEM not defined => definitve failure\n");
		exit(1);
	}
	if	(!RootAct)
		RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		fprintf	(log,"$ROOTACT not set\n");
		setenv	("ROOTACT",strdup("/home/actility"),1);
		RootAct	= getenv("ROOTACT");
	}

	sprintf	(fdefaultexe,"%s/lrr/com/exe_%s_%s/lrr.x",RootAct,SerialMode,BoardType);

	fprintf	(log,"SYSTEM=%s\n",System);
	fprintf	(log,"ROOTACT=%s\n",RootAct);

	fprintf	(log,"SERIALMODE=%s (default)\n",SerialMode);
	fprintf	(log,"BOARDTYPE=%s (default)\n",BoardType);
	fprintf	(log,"SERVICELRR=%s (default)\n",ServiceLrr);
	fprintf	(log,"file to launch '%s' (default)\n",fdefaultexe);

	if	(rtl_openDir(RootAct) == NULL)
	{
		fprintf	(log,"ROOTACT does not exist or can not be opened => definitve failure\n");
		exit(1);
	}

	sprintf	(fparams,"%s/usr/etc/lrr/_parameters.sh",RootAct);
	f	= fopen(fparams,"r");
	if	(f)
	{
		int	err;

		err	= rtl_iniParse(fparams,CfgCBIniLoad,HtVarLrr);
		if	(err < 0)
		{
			fprintf	(log,"'%s' parse error=%d => definitve failure \n",
				fparams,err);
			exit(1);
		}
		fclose(f);
	}
	else
	{
		fprintf	(log,"'%s' does not exist : use default values\n",fparams);
	}

	SerialMode	= CfgStr(HtVarLrr,"",-1,"SERIALMODE",SerialMode);
	BoardType	= CfgStr(HtVarLrr,"",-1,"BOARDTYPE",BoardType);
	ServiceLrr	= CfgStr(HtVarLrr,"",-1,"SERVICELRR",ServiceLrr);
#ifdef	WIRMAV2
	char	file[1024];
	char	cmd[1024];
	sprintf	(file,"%s/usr/etc/lrr/_system.sh",RootAct);
	if	(access(file,R_OK) != 0)
	{
		fprintf	(log,"file '%s' does not exist => try to create it\n",
							file);
		sprintf	(cmd,"/usr/local/bin/get_version > %s",file);
		system(cmd);
	}

#endif
#ifdef	CISCOMS
	char	file[1024];
	char	cmd[1024];
	sprintf	(file,"%s/usr/etc/lrr/_system.sh",RootAct);
	if	(access(file,R_OK) != 0)
	{
		fprintf	(log,"file '%s' does not exist => try to create it\n",
							file);
		sprintf	(cmd,"[ -z \"$(grep CISCOSN %s 2>/dev/null)\" ] && echo CISCOSN=$(getsn) >> %s", file, file);
		system(cmd);
	}

#endif

	sprintf	(factiveexe,"%s/lrr/com/exe_%s_%s/lrr.x",RootAct,SerialMode,BoardType);

	fprintf	(log,"-----------\n");
	fprintf	(log,"----------- load '%s'\n",fparams);
	fprintf	(log,"-----------\n");
	fprintf	(log,"SERIALMODE=%s (active)\n",SerialMode);
	fprintf	(log,"BOARDTYPE=%s (active)\n",BoardType);
	fprintf	(log,"SERVICELRR=%s (active)\n",ServiceLrr);
	fprintf	(log,"file to launch '%s' (active)\n",factiveexe);

	fexe	= factiveexe;
	if	(access(fexe,X_OK) != 0)
	{
		fprintf	(log,"file to launch '%s' does not exist or !X_OK\n",fexe);
		fexe	= fdefaultexe;
		if	(access(fexe,X_OK) != 0)
		{
			fprintf	(log,"file to launch '%s' does not exist or !X_OK\n"
						,fexe);
			exit(1);
		}
	}

	setenv	("SERIALMODE",strdup(SerialMode),1);
	setenv	("BOARDTYPE",strdup(BoardType),1);
	setenv	("SERVICELRR",strdup(ServiceLrr),1);

	fprintf	(log,"-----------\n");
	fprintf	(log,"----------- launch '%s'\n",fexe);
	fprintf	(log,"-----------\n");

	fflush	(log);
	fclose	(log);

	execv	(fexe,argv);

	printf	("launch error errno=%d\n",errno);
	
	exit(1);
}
