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

/*
 * \file    main.c
 * \brief   Support login
 * \author  BG, Actility
 *
 *  Handle support login
 */

// Needed for getresuid
#define _GNU_SOURCE

#include <newt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <libgen.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "_suplogver.h"
 #include "htbl_suplog.h"

#ifdef WIRMAV2
#define TRACEAGDIR  "/mnt/fsuser-1/trace_agent"
#else
#define TRACEAGDIR  ""
#endif

#if defined(WIRMAAR) || defined(WIRMANA)
#define	LOGDIR	"/user/actility/traces"
#endif
#ifdef WIRMAV2
#define	LOGDIR	"/mnt/mmcblk0p1/actility/traces"
#endif
#if defined(FCPICO) || defined(FCLAMP) || defined(FCLOC) || defined(FCMLB) || defined(CISCOMS) || defined(GEMTEK)
#define LOGDIR "/home/actility/traces"
#endif

#define CMDTYPE_MENU        -1
#define CMDTYPE_STATIC      0
#define CMDTYPE_DYNAMIC     1
#define CMDTYPE_TAIL        2
#define CMDTYPE_GREP        3
#define CMDTYPE_FUNCTION    4

#define ROOT        1
#define NONROOT     0

#define GREPALLOWED     ".*[] ^$-_#+=?"
#define IPALLOWED       "."
#define NUMBERALLOWED   "0123456789"
#define ALPHAALLOWED    ""
#define COMMANOTALLOWED		";|\\`><&$()-[]{} "
#define COMMANOTALLOWED2	";|\\`><&$()-[]{}"

#define CMD_SHELLS  "$ROOTACT/lrr/com/cmd_shells/"
#define SHELLS      "$ROOTACT/lrr/com/shells/"

#if defined(WIRMAV2)
#define CMD_SHELLS_DEVICE   CMD_SHELLS "wirmav2/"
#define SHELLS_DEVICE       SHELLS "wirmav2/"
#define IPSEC_LOC			"/usr/sbin/"
#define TERMINFO			"export TERMINFO=/usr/share/terminfo/"
#define NETWORK 			"/etc/network/ifcfg-eth0"
#define APN 				"/etc/sysconfig/network"
#define NETSTAT_OPT			"-anp"
#define VIEW_NTP 			"ntpq -pn && cat /etc/ntp.conf"
#define LOCALTIME			"/etc/localtime"
#define TIMEZONE_DEST		"$ROOTACT/usr/etc/zoneinfo"
#elif defined(WIRMAAR)
#define CMD_SHELLS_DEVICE   CMD_SHELLS "wirmaar/"
#define SHELLS_DEVICE       SHELLS "wirmaar/"
#define IPSEC_LOC			"/usr/sbin/"
#define TERMINFO			"export TERMINFO=/etc/terminfo/"
#define NETWORK 			"/etc/network/interfaces"
#define APN					"/user/rootfs_rw/etc/network/gsm.conf" 
#define NETSTAT_OPT			"-an"
#define VIEW_NTP 			"cat /etc/ntp.conf"
#define LOCALTIME			"/user/rootfs_rw/etc/localtime"
#define TIMEZONE_DEST		"$ROOTACT/usr/etc/zoneinfo"
#elif defined(WIRMANA)
#define CMD_SHELLS_DEVICE   CMD_SHELLS "wirmana/"
#define SHELLS_DEVICE       SHELLS "wirmana/"
#define IPSEC_LOC			"/usr/sbin/"
#define TERMINFO			"export TERMINFO=/etc/terminfo/"
#define NETWORK 			"/etc/network/interfaces"
#define APN					"/user/rootfs_rw/etc/network/netctl.cfg"
#define NETSTAT_OPT			"-an"
#define VIEW_NTP 			"cat /etc/ntp.conf"
#define LOCALTIME			"/user/rootfs_rw/etc/localtime"
#define TIMEZONE_DEST		"$ROOTACT/usr/etc/zoneinfo"
#elif defined(FCPICO)
#define CMD_SHELLS_DEVICE   CMD_SHELLS "fcpico/"
#define TERMINFO			"export TERMINFO=/etc/terminfo/"
#define NETWORK 			"/etc/network/interfaces"
#define NETSTAT_OPT			"-an"
#elif defined(FCLAMP) || defined (FCLOC) || defined (GEMTEK)
#define TERMINFO			"export TERMINFO=/etc/terminfo/"
#define NETWORK 			"/etc/network/interfaces"
#define	NETCONFIGTMPFILE	"/tmp/_lrrnetconfig"
#define NETSTAT_OPT			"-an"
#elif defined(WIRMAMS)
#define CMD_SHELLS_DEVICE   CMD_SHELLS "wirmams/"
#define SHELLS_DEVICE       SHELLS "wirmams/"
#elif defined(CISCOMS)
#define CMD_SHELLS_DEVICE   CMD_SHELLS "ciscoms/"
#define SHELLS_DEVICE       SHELLS "ciscoms/"
#define IPSEC_LOC			"/usr/sbin/"
#define TERMINFO			"export TERMINFO=/usr/share/terminfo/"
#define APN 				"/etc/sysconfig/network"
#define NETSTAT_OPT			"-anp"
#define VIEW_NTP 			"ntpq -pn"
#elif defined(TEKTELIC)
#define CMD_SHELLS_DEVICE   CMD_SHELLS "tektelic/"
#define SHELLS_DEVICE       SHELLS "tektelic/"
#endif

#define	NETCONFIGTMPFILE	"/tmp/_lrrnetconfig"

#ifndef CMD_SHELLS_DEVICE
#define CMD_SHELLS_DEVICE   CMD_SHELLS 
#endif

#ifndef SHELLS_DEVICE
#define SHELLS_DEVICE       SHELLS
#endif

#ifndef IPSEC_LOC
#define IPSEC_LOC			"/usr/local/arm-none-linux-gnueabi/sbin/"
#endif

#ifndef TERMINFO
#define TERMINFO			"export TERMINFO=/usr/share/terminfo/"
#endif

#ifndef NETWORK
#define NETWORK 			"/etc/network/ifcfg-eth0"
#endif

#ifndef APN
#define APN					"/etc/sysconfig/network"
#endif

#ifndef NETSTAT_OPT
#define NETSTAT_OPT			"-an"
#endif

#ifndef VIEW_NTP
#define VIEW_NTP 			"cat /etc/ntp.conf"
#endif

#ifndef LOCALTIME
#define LOCALTIME 			"/etc/localtime"
#endif

#ifndef TIMEZONE_DEST
#define TIMEZONE_DEST		"$ROOTACT/usr/etc/zoneinfo"
#endif

#define SAFESTRCPY(arg1, arg2)  strncpy(arg1, arg2, sizeof(arg1)-1); arg1[sizeof(arg1)-1]='\0';

#if defined(WIRMAAR) || defined(WIRMAV2) || defined(FCPICO) || defined(FCLAMP) || defined(FCLOC) || defined(FCMLB) || defined(CISCOMS) || defined(GEMTEK) || defined (WIRMANA)
#define RAMDIR
#endif

char	*System	= "";

/* Structure definition */
typedef struct {
	char   *text;
	char   *command;
	int    commandType;
	int    rootPermission;
} tMenuItem;

/* used by callback to enable/disable writability depending on checkbox value */
struct cbInfoCompo {
    newtComponent en;
    char * state;
};

int	NetConfEditable		= 0;
int	NetConfTPE		= 0;	// network configuration for TP Enterprise allowed
char *	NetConfInterFile	= "";
char *	NetConfNtpFile		= "";
char *	NetConfVpnFile		= "";

/* Function definitions */
void userRoot(int giveRootPerm);
void dispResult(char *title, char *command, int isTail, int runOnce, const int giveRootPerm);
void endDisplay();
void initDisplay();
int filterPassword(newtComponent ent, void *data, int ch, int cursor);
int checkString(char *s, const char *allowed, const int checkAlphaNum);
void sendLogFiles(char *srv, char *port, char *user, char *pwd);
void getGrepInfos(char *file);
void getTransferInfos(char *title);
void processCommandType(tMenuItem *choice);
void createConfirmation(char *title, char *message, char *cmd, int commandType, int rootPermission);
void ping(char *title);
void reverseSSH(char *title);
void backup_restore(char *title, const int restore);
void powerTransmissionAdjustement(char *title);
void transmitPower(char *title);
void changeTimezone(char *title);
void setOwnTimezone(char *title);
void functionIface(char *title);
void runMenu(char *title, tMenuItem *menu, int szMenu);
void textEditor(char *title, char *file);
void pingLRCS(char *title);

/* This variable is used for displaying ongoing development */
#define DEBUG 0

/* Global variables */

// Menu entry
typedef tMenuItem   tMenu[];

// Main menu
tMenuItem MenuMain[] = {
	{ " Logs", "MenuLog", CMDTYPE_MENU },
	{ " LRR configuration", "MenuLRRConfig", CMDTYPE_MENU },
	{ " Network", "MenuNetwork", CMDTYPE_MENU },
	{ " Troubleshooting", "MenuTrouble", CMDTYPE_MENU },
	{ " Services", "MenuServices", CMDTYPE_MENU },
#if !defined(FCPICO) && !defined(FCLAMP) && !defined (FCLOC)
	{ " VPN Configuration", "MenuVPN", CMDTYPE_MENU },
#endif
#if DEBUG
	{ " DEBUG", "MenuDebug", CMDTYPE_MENU },
#endif
};

// main menu for logs
tMenuItem MenuLog[] = {
	{ " Visualize log files", "MenuVizLog", CMDTYPE_MENU },
	{ " Watch activity on log files (tail)", "MenuTailLog", CMDTYPE_MENU },
	{ " Search in log files (grep)", "MenuGrepLog", CMDTYPE_MENU },
	{ " Export configuration and log files via ftp", "functionExportLog", CMDTYPE_FUNCTION },
};

// menu visualize log files
#define	TAILCMD	"tail -100 "
tMenuItem MenuVizLog[] = {
	{ " Current lrr log file", TAILCMD "$ROOTACT/var/log/lrr/TRACE.log", CMDTYPE_STATIC, NONROOT },
	{ " Monday lrr log file", TAILCMD "_LOGDIR_/TRACE_01.log", CMDTYPE_STATIC, NONROOT },
	{ " Tuesday lrr log file", TAILCMD "_LOGDIR_/TRACE_02.log", CMDTYPE_STATIC, NONROOT },
	{ " Wednesday lrr log file", TAILCMD "_LOGDIR_/TRACE_03.log", CMDTYPE_STATIC, NONROOT },
	{ " Thursday lrr log file", TAILCMD "_LOGDIR_/TRACE_04.log", CMDTYPE_STATIC, NONROOT },
	{ " Friday lrr log file", TAILCMD "_LOGDIR_/TRACE_05.log", CMDTYPE_STATIC, NONROOT },
	{ " Saturday lrr log file", TAILCMD "_LOGDIR_/TRACE_06.log", CMDTYPE_STATIC, NONROOT },
	{ " Sunday lrr log file", TAILCMD "_LOGDIR_/TRACE_07.log", CMDTYPE_STATIC, NONROOT },
	{ " Last shell results log file", TAILCMD "$ROOTACT/var/log/lrr/SHELL.log", CMDTYPE_STATIC, NONROOT },
#if !defined(CISCOMS)
	{ " /var/log/messages log file", TAILCMD "/var/log/messages", CMDTYPE_STATIC, NONROOT },
#endif
	{ " Strongswan log file", TAILCMD "/var/log/charon.log", CMDTYPE_STATIC, NONROOT },
};

// menu tail log files
tMenuItem MenuTailLog[] = {
	{ " Current lrr log file", "tail -<HEIGHTTB> $ROOTACT/var/log/lrr/TRACE.log", CMDTYPE_TAIL, NONROOT },
	{ " Last shell results log file", "tail -<HEIGHTTB> $ROOTACT/var/log/lrr/SHELL.log", CMDTYPE_TAIL, NONROOT },
#if !defined(CISCOMS)
	{ " /var/log/messages log file", "tail -<HEIGHTTB> /var/log/messages", CMDTYPE_TAIL, NONROOT },
#endif
	{ " Strongswan log file", "tail -<HEIGHTTB> /var/log/charon.log", CMDTYPE_TAIL, NONROOT },
};

// menu grep log files
tMenuItem MenuGrepLog[] = {
	{ " Current lrr log file", "$ROOTACT/var/log/lrr/TRACE.log", CMDTYPE_GREP, NONROOT },
	{ " Monday lrr log file", "_LOGDIR_/TRACE_01.log", CMDTYPE_GREP, NONROOT },
	{ " Tuesday lrr log file", "_LOGDIR_/TRACE_02.log", CMDTYPE_GREP, NONROOT },
	{ " Wednesday lrr log file", "_LOGDIR_/TRACE_03.log", CMDTYPE_GREP, NONROOT },
	{ " Thursday lrr log file", "_LOGDIR_/TRACE_04.log", CMDTYPE_GREP, NONROOT },
	{ " Friday lrr log file", "_LOGDIR_/TRACE_05.log", CMDTYPE_GREP, NONROOT },
	{ " Saturday lrr log file", "_LOGDIR_/TRACE_06.log", CMDTYPE_GREP, NONROOT },
	{ " Sunday lrr log file", "_LOGDIR_/TRACE_07.log", CMDTYPE_GREP, NONROOT },
	{ " Last shell results log file", "$ROOTACT/var/log/lrr/SHELL.log", CMDTYPE_GREP, NONROOT },
#if !defined(CISCOMS)
	{ " /var/log/messages log file", "/var/log/messages", CMDTYPE_GREP, NONROOT },
#endif
	{ " Strongswan log file", "/var/log/charon.log", CMDTYPE_GREP, NONROOT },
};


// menu LRR configuration
tMenuItem MenuLRRConfig[] = {
	{ " Get the current radio configuration", "cat $ROOTACT/var/log/lrr/radioparams.txt", CMDTYPE_STATIC, NONROOT },
	{ " Get LRR UID", CMD_SHELLS "getuid.sh --readfile", CMDTYPE_STATIC, ROOT },
	{ " Dump configuration files ", CMD_SHELLS "showconfig.sh", CMDTYPE_STATIC, ROOT },
	{ " Set Transmit Power", "functionTrPower", CMDTYPE_FUNCTION },
	{ " Set Power Transmission Adjustement", "functionPowerTrAd", CMDTYPE_FUNCTION },
};

// menu Network
tMenuItem MenuNetwork[] = {
	{ " View eth0 network", "ifconfig eth0", CMDTYPE_STATIC, NONROOT },
#if !defined(CISCOMS)
	{ " Change network", "editEth", CMDTYPE_FUNCTION },
#endif
#if defined(FCPICO)
	{ " Change network", "editEthTPE", CMDTYPE_FUNCTION },
#endif
#if !defined(FCPICO) && !defined (FCLAMP) && !defined (FCLOC)
	{ " View Acces Point Name network (APN)", "cat " APN, CMDTYPE_STATIC, NONROOT },
	{ " Change APN", "editAPN", CMDTYPE_FUNCTION },
#endif
#if !defined(CISCOMS)
	{ " View /etc/hosts", "cat /etc/hosts", CMDTYPE_STATIC, NONROOT },
	{ " Change /etc/hosts", "editETCHOSTS", CMDTYPE_FUNCTION },
#endif
#if !defined(FCPICO) && !defined (FCLAMP) && !defined (FCLOC) && !defined (CISCOMS)
	{ " View timezone", "date && echo -n 'Timezone is '; date +%Z", CMDTYPE_STATIC, NONROOT },
	{ " Change timezone", "functionTimezone", CMDTYPE_FUNCTION },
#endif
	{ " Domain Name System server address (DNS)", "cat /etc/resolv.conf", CMDTYPE_STATIC, NONROOT },
	{ " Change DNS", "editDNS", CMDTYPE_FUNCTION },
#if !defined(FCPICO) && !defined (FCLAMP) && !defined (FCLOC) && !defined (CISCOMS)
	{ " Network Time Procotol (NTP)", VIEW_NTP, CMDTYPE_STATIC, NONROOT },
	{ " Change NTP", "editNTP", CMDTYPE_FUNCTION },
#endif
};

// menu troubleshooting
tMenuItem MenuTrouble[] = {
	{ " Ping", "functionPing", CMDTYPE_FUNCTION},
	{ " Ping LRCs", "functionPingLRCs", CMDTYPE_FUNCTION },
	{ " Disk usage", "df -h", CMDTYPE_STATIC, NONROOT },
	{ " CPU / Memory / Processes", "top -b -n 1", CMDTYPE_DYNAMIC, NONROOT },
	{ " Netstat", "netstat " NETSTAT_OPT, CMDTYPE_STATIC, ROOT },
	{ " Show iptables", "/usr/sbin/iptables -L", CMDTYPE_STATIC, ROOT },
	{ " Show versions", "echo 'lrr version:' && \
						cat $ROOTACT/lrr/Version && \
						echo '\nUser versions in lrr.ini:' && \
						cat $ROOTACT/usr/etc/lrr/lrr.ini | grep '_version'", CMDTYPE_STATIC, NONROOT },
#if defined(WIRMAV2)
	{ " Show kerlink versions", "echo '\nkerlink version:' && \
								cat /etc/klk-version && \
								echo '\nkerlink printenv:' && \
								printenv && \
								echo '\nkerlink fw_printenv:' && \
								/usr/sbin/fw_printenv && \
								echo '\nkerlink get_version:' && \
								/usr/local/bin/get_version -v", CMDTYPE_STATIC, ROOT },
#endif
	{ " uptime", "uptime", CMDTYPE_STATIC, NONROOT },
	{ " Network interfaces status", "ifconfig", CMDTYPE_STATIC, NONROOT },
	{ " Network routes", "route -n", CMDTYPE_STATIC, NONROOT },
};

// menu services
tMenuItem MenuServices[] = {
	{ " Halt LRR gateway", "functionHalt", CMDTYPE_FUNCTION },
	{ " Reboot LRR gateway", "functionReboot", CMDTYPE_FUNCTION },
	{ " Restart LRR Process", "functionRestartLRR", CMDTYPE_FUNCTION },
	{ " Open reverse ssh", "functionReverseSSH", CMDTYPE_FUNCTION },
#if defined(WIRMAAR) || defined(WIRMANA) || defined(WIRMAMS) || defined(CISCOMS)
	{ " Close reverse ssh", "pids='pidof sshpass.x' && if eval $pids >/dev/null; then\necho -n 'Closing all reverse SSH with PIDs: ' && eval $pids && " CMD_SHELLS_DEVICE "closessh.sh\nelse\necho No reverse SSH to close\nfi", CMDTYPE_STATIC, ROOT },
#else
	{ " Close reverse ssh", "pids='pidof sshpass.x' && if eval $pids >/dev/null; then\necho -n 'Closing all reverse SSH with PIDs: ' && eval $pids && killall sshpass.x\nelse\necho No reverse SSH to close\nfi", CMDTYPE_STATIC, ROOT },
#endif
#if defined(WIRMAV2) || defined(WIRMAAR) || defined(WIRMANA) || defined(WIRMAMS)
	{ " Execute backup", "functionBackup", CMDTYPE_FUNCTION },
	{ " Execute restore", "functionRestore", CMDTYPE_FUNCTION },
#endif
};

// main menu for VPN configuration
tMenuItem MenuVPN[] = {
	{ " Show ipsec statusall", IPSEC_LOC "ipsec statusall", CMDTYPE_STATIC, ROOT },
	{ " Show ipsec status", IPSEC_LOC "ipsec status", CMDTYPE_STATIC, ROOT },
	{ " Show ipsec listcacerts", IPSEC_LOC "ipsec listcacerts", CMDTYPE_STATIC, ROOT },
	{ " Show ipsec listcrls", IPSEC_LOC "ipsec listcrls", CMDTYPE_STATIC, ROOT },
	{ " Show ipsec listcerts", IPSEC_LOC "ipsec listcerts", CMDTYPE_STATIC, ROOT },
	{ " Show ipsec version", IPSEC_LOC "ipsec version", CMDTYPE_STATIC, ROOT },
};

// main menu for debug
tMenuItem MenuDebug[] = {
  { " Test whoami as non-root", "whoami", CMDTYPE_STATIC, NONROOT },
  { " Test whoami as root", "whoami", CMDTYPE_STATIC, ROOT },
  { " Debug whoami", "functionWhoami", CMDTYPE_FUNCTION, NONROOT },
};

// screen size
int MaxX;
int MaxY;

// Maximum data read
int MaxData = 102400;

// Maximum command size
int MaxCmd = 512;

// Temporary working file name
char WorkFile[256];

// Used for changing from suplog user to root
uid_t ruid, euid, suid;


 /**
  * @brief      Change permission to root
  *
  * @param[in]  giveRootPerm  1=root, 0=suplog
  */
void userRoot(int giveRootPerm)
{
	if (giveRootPerm)
		setreuid(euid, euid);
	else
		setreuid(ruid, euid);
}

/*
 *  \brief Run and display result of command
 *
 *  \param title window title
 *  \param command command to execute
 *  \param isTail if non zero, replace "<HEIGHTTB>" in the command line
 *  \param runOnce run command only one time
 *  \param giveRootPerm if 1 it gives root permissions
 */
void dispResult(char *title, char *command, int isTail, int runOnce, const int giveRootPerm)
{
	struct newtExitStruct es;
	newtComponent   tb, form, b;
	struct stat st;
	char *result=NULL, tmp[80], runCmd[MaxCmd], *pt, *cmd;
	FILE *f;
	int  res, sz;

	// create display stuff
	newtCenteredWindow(MaxX-10, MaxY-5, title);
	form = newtForm(NULL, NULL, 0);
	// if display is refreshed periodically, do not show scroll bar because scrolling
	// is reset at each refresh
	if (runOnce)
		tb = newtTextbox(1, 1, MaxX-12, MaxY-8, NEWT_FLAG_WRAP | NEWT_FLAG_SCROLL);
	else
	{
		newtFormSetTimer(form, 3000);
		tb = newtTextbox(1, 1, MaxX-12, MaxY-8, 0);
	}
	b = newtCompactButton(MaxX/2 - 8, MaxY-6, "Ok");
	newtFormAddComponents(form, tb, b, NULL);
	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);

	// Tail, need to set number of lines returned
	if (isTail)
	{
		newtFormSetTimer(form, 1000);
		strncpy(runCmd, command, sizeof(runCmd));
		runCmd[sizeof(runCmd)-1] = '\0';
		if ((pt = strstr(runCmd, "<HEIGHTTB>")))
		{
			sprintf(tmp, "%d", MaxY-8);
			strcpy(pt, tmp);
			strcat(runCmd, command + (pt-runCmd) + 10);
		}
		cmd = runCmd;
	}
	else
		cmd = command;

	/* Become root for this command */
	if (giveRootPerm)
		userRoot(1);

	// execute the command
	system(cmd);

	/* Back to suplog user */
	if (giveRootPerm)
		userRoot(0);

	while (1)
	{
		// get result in working file
		f = fopen(WorkFile, "r");
		if (!f)
			break;

		fstat(fileno(f), &st);
		if (st.st_size > MaxData)
			sz = MaxData;
		else
			sz = st.st_size;

		if (result)
			free(result);
		result = (char *) malloc(sz+1);
		fseek(f, -MaxData, SEEK_END);
		res = fread(result, 1, sz, f);
		result[res] = '\0';
		fclose(f);

		// display the result
		newtTextboxSetText(tb, result);

		newtFormRun(form, &es);

		// stop if button 'Ok' pressed
		if (es.reason != NEWT_EXIT_TIMER)
			break;

		// execute command again
		if (!runOnce)
		{
			/* Become root for this command */
			if (giveRootPerm)
				userRoot(1);

			// execute the command
			system(cmd);

			/* Back to suplog user */
			if (giveRootPerm)
				userRoot(0);

		}
	}

	newtFormDestroy(form);
	newtPopWindow();
}

/*
 * \brief Terminate display
 */
void endDisplay()
{
	newtFinished();
}

/*
 * \brief Initialize display
 */
void initDisplay()
{
	newtInit();
	newtCls();
	newtGetScreenSize(&MaxX, &MaxY);
}

/*
 * \brief Initialize System variable
 */
void initSystem()
{
#if	defined(WIRMAAR)
	System = "wirmaar";
#elif	defined(WIRMAV2)
	System = "wirmav2";
#elif	defined(FCPICO)
	System = "fcpico";
#elif   defined(FCLAMP)
	System = "fclamp";
#elif	defined(FCLOC)
	System = "fcloc";
#elif	defined(FCMLB)
	System = "fcmlb";
#elif	defined(CISCOMS)
	System = "ciscoms";
#elif   defined(GEMTEK)
	System = "gemtek";
#elif   defined(WIRMANA)
	System = "wirmana";
#endif
}

/*
 *  \brief filter password. Not used actually
 *
 *  \param ent entry component
 *  \param data user data
 *  \param ch character entered
 *  \param cursor cursor position
 *  \return result
 */
int filterPassword(newtComponent ent, void *data, int ch, int cursor)
{
	char tmp[20], *stored;
	int ln;

	stored = (char *) data;
	sprintf(tmp, "%c", ch);
	if (ch == NEWT_KEY_BKSPC)
		return 0;

	ln = strlen(stored);
	stored[ln] = (char) ch;
	stored[ln+1] = '\0';
	newtPushHelpLine(stored);
	newtRefresh();
	return '*';
}


/**
 * @brief      Check string to prevent malicious commands being executed
 *
 * @param      s                The string to check
 * @param[in]  allowed          The string containing allowed caracters
 * @param[in]  checkAlphaNum    1: also check if alphanumerique, 0: check only for allowed string
 *
 * @return     1 if ok, 0 if string use unauthorized characters
 */
int checkString(char *s, const char *allowed, const int checkAlphaNum)
{
	int ln, i;

	ln = strlen(s);
	for (i=0; i<ln; i++)
	{
		if (!(isalnum(s[i]) && checkAlphaNum) && !strchr(allowed, s[i]))
			return 0;
	}
	return 1;
}

/**
 * @brief      Check string to prevent malicious commands being executed
 *
 * @param      s                The string to check
 * @param[in]  notAllowed       The string containing not allowed characters
 *
 * @return     1 if ok, 0 if string use unauthorized characters
 */
int checkNotString(char *s, const char *notAllowed)
{
	int ln, i;

	ln = strlen(s);
	for (i=0; i<ln; i++)
	{
		if (strchr(notAllowed, s[i]))
			return 0;
	}
	return 1;
}

/**
 * @brief      Check if using ram disk to store log files is activated
 *
 * @return     1 if activated, else return 0
 */
int isLogInRamDir()
{
	char	*trdir1, *trdir2;

	loadIni();

	// read lrr.ini:[trace].ramdir
	trdir1	= CfgStr(HtVarLrr,"trace",-1,"ramdir",NULL);
	// read default lrr.ini:[<system>].ramdir
#if	defined(WIRMAAR)
	trdir2	= CfgStr(HtVarLrr,"wirmaar",-1,"ramdir",NULL);
#elif	defined(WIRMAV2)
	trdir2	= CfgStr(HtVarLrr,"wirmav2",-1,"ramdir",NULL);
#elif	defined(FCPICO)
	trdir2	= CfgStr(HtVarLrr,"fcpico",-1,"ramdir",NULL);
#elif   defined(FCLAMP)
	trdir2  = CfgStr(HtVarLrr,"fclamp",-1,"ramdir",NULL);
#elif	defined(FCLOC)
	trdir2	= CfgStr(HtVarLrr,"fcloc",-1,"ramdir",NULL);
#elif	defined(FCMLB)
	trdir2	= CfgStr(HtVarLrr,"fcmlb",-1,"ramdir",NULL);
#elif	defined(CISCOMS)
	trdir2	= CfgStr(HtVarLrr,"ciscoms",-1,"ramdir",NULL);
#elif   defined(GEMTEK)
	trdir2  = CfgStr(HtVarLrr,"gemtek",-1,"ramdir",NULL);
#elif   defined(WIRMANA)
	trdir2  = CfgStr(HtVarLrr,"wirmana",-1,"ramdir",NULL);
#else
	trdir2 = NULL;
#endif

	// ramdir set to empty in lrr.ini -> feature disabled
	if (trdir1 && !*trdir1)
		return 0;

	// ramdir not set in lrr.ini and also not set in default lrr.ini
	if (!trdir1 && (!trdir2 || !*trdir2))
		return 0;

	return 1;
}

/*
 *  \brief display result of command
 *
 *  \param title window title
 */
void sendLogFiles(char *srv, char *port, char *user, char *pwd)
{
	struct tm *st;
	char    tmp[1024], cmd[1024], fn[80], *logdir;
	time_t  now;

	// set target filename
	now = time(NULL);
	st = localtime(&now);
	sprintf(fn, "LOGS_%02d%02d%02d_%02d%02d.tar.gz", st->tm_year%100, st->tm_mon+1, st->tm_mday, st->tm_hour, st->tm_min);

	logdir = "";
#if defined(RAMDIR)
	if (isLogInRamDir())
		logdir = LOGDIR;
#endif
#if defined(CISCOMS)
	sprintf(cmd, "tar cvhf /tmp/sendlog.tar $ROOTACT/var/log/lrr $ROOTACT/usr/etc/lrr $ROOTACT/lrr/config %s %s; "
#else
	sprintf(cmd, "tar cvhf /tmp/sendlog.tar /var/log/messages $ROOTACT/var/log/lrr $ROOTACT/usr/etc/lrr $ROOTACT/lrr/config %s %s; "
#endif
#ifdef WIRMAV2
		"gzip -f /tmp/sendlog.tar; ftpput -u %s -p \"%s\" -P %s %s /tmp/%s /tmp/sendlog.tar.gz;"
		"if [ $? == 0 ]; then echo \"end of transfer of /tmp/%s\"; else echo \"ERROR\"; fi",
		TRACEAGDIR, logdir, user, pwd, port, srv, fn, fn);
#elif defined(WIRMAAR) || defined(CISCOMS) || defined(FCLOC) || defined(FCMLB) || defined(FCPICO) || defined(FCLAMP) || defined(WIRMANA) || defined(TEKTELIC) || defined(GEMTEK)
		"gzip -f /tmp/sendlog.tar; curl -T /tmp/sendlog.tar.gz ftp://%s:\"%s\"@%s:%s/%s;"
		"if [ $? == 0 ]; then echo \"end of transfer of /tmp/%s\"; else echo \"ERROR\"; fi",
		TRACEAGDIR, logdir, user, pwd, srv, port, fn, fn);
#else
		"gzip -f /tmp/sendlog.tar; ftp -n <<END_FTP ;\nopen %s %s\nquote user %s\n"
		"quote pass \"%s\"\nbinary\nput /tmp/sendlog.tar.gz /tmp/%s\nquit\nEND_FTP\n"
		" if [ $? == 0 ]; then echo \"end of transfer of /tmp/%s\"; else echo \"ERROR\"; fi", TRACEAGDIR, srv, port, user, pwd, fn, fn);
#endif
	snprintf(tmp, sizeof(tmp), "( %s ) > %s 2>&1", cmd, WorkFile);
	tmp[sizeof(tmp)-1] = '\0';

	dispResult("Sending log files ...", tmp, 0, 1, NONROOT);
}


/*
 *  \brief Get string to use with grep command
 *
 *  \param file file used for grep
 */
void getGrepInfos(char *file)
{
	struct newtExitStruct es;
	newtComponent   form, btnConf, btnCan, entS, lblS;
	char command[256], tmp[80], newfn[256], *pt;

	newtCenteredWindow(50, 5, "Search in file");
	form = newtForm(NULL, NULL, 0);
	lblS = newtLabel(1, 1, "String to find :");
	entS = newtEntry(26, 1, NULL, 20, NULL, NEWT_ENTRY_SCROLL);
	btnConf = newtCompactButton(9, 4, "Confirm");
	btnCan = newtCompactButton(27, 4, "Cancel");
	newtFormAddComponents(form, lblS, entS, btnConf, btnCan, NULL);

	while (1)
	{
		newtFormRun(form, &es);
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkString(newtEntryGetValue(entS), GREPALLOWED, 1))
			{
				strcpy(tmp, "Only alphanumeric characters and " GREPALLOWED " are allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}
			if (!checkNotString(newtEntryGetValue(entS), COMMANOTALLOWED))
			{
				strcpy(tmp, "Character \"" COMMANOTALLOWED "\" is not allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			// Logs can be in different locations
			if ((pt = strstr(file, "_LOGDIR_")))
			{
				// Replace '_LOGDIR_' with log directory
#if	defined(RAMDIR)
				if (isLogInRamDir())
					// directory is hard coded in backuptraces.sh
					strcpy(newfn, LOGDIR);
				else
					strcpy(newfn, "$ROOTACT/var/log/lrr");
#else
				strcpy(newfn, "$ROOTACT/var/log/lrr");
#endif

				// concat what is after '_LOGDIR_'
				strcat(newfn, pt+8);
			}
			else
				strcpy(newfn, file);
			snprintf(command, sizeof(command)-1, "grep -i \"%s\" %s >%s 2>&1", newtEntryGetValue(entS), newfn, WorkFile);
//          snprintf(command, sizeof(command)-1, "env >%s 2>&1", WorkFile);
			command[sizeof(command)-1] = '\0';
			dispResult("Result", command, 0, 1, NONROOT);
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

/*
 *  \brief Get transfer informations
 *
 *  \param title window title
 */
void getTransferInfos(char *title)
{
	char tmp[80];
	struct newtExitStruct es;
	newtComponent   form, btnConf, btnCan, entS, lblS, entU, lblU, entPa, lblPa, entPo, lblPo;

	newtCenteredWindow(50, 10, title);
	form = newtForm(NULL, NULL, 0);
	lblS = newtLabel(1, 1, "Server name or address :");
	entS = newtEntry(26, 1, NULL, 20, NULL, NEWT_ENTRY_SCROLL);
	lblPo = newtLabel(1, 3, "Port :");
	entPo = newtEntry(26, 3, "21", 20, NULL, NEWT_ENTRY_SCROLL);
	lblU = newtLabel(1, 5, "User name :");
	entU = newtEntry(26, 5, NULL, 20, NULL, NEWT_ENTRY_SCROLL);
	lblPa = newtLabel(1, 7, "Password :");
	entPa = newtEntry(26, 7, NULL, 20, NULL, NEWT_ENTRY_SCROLL | NEWT_FLAG_PASSWORD);
	btnConf = newtCompactButton(9, 9, "Confirm");
	btnCan = newtCompactButton(27, 9, "Cancel");
	newtFormAddComponents(form, lblS, entS, lblPo, entPo, lblU, entU, lblPa, entPa, btnConf, btnCan, NULL);

	while (1)
	{
		newtFormRun(form, &es);
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkNotString(newtEntryGetValue(entS), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entPo), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entU), COMMANOTALLOWED))
			{
				strcpy(tmp, "Character \"" COMMANOTALLOWED "\" is not allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			newtSetColor(NEWT_COLORSET_HELPLINE, "white", "blue");
			newtPushHelpLine("Transfer in progress, please wait ...");
			newtRefresh();

			sendLogFiles(newtEntryGetValue(entS), newtEntryGetValue(entPo), newtEntryGetValue(entU), newtEntryGetValue(entPa));

			break;
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}


/*
 *  \brief read network configuration of the gateway
 *
 *  \param dhcp dhcp activated 0|1
 *  \param ip ip address
 *  \param mask ip mask
 *  \param gw gw address
 *  \param dns dns
 *  \param ntp ntp server
 *  \param key key-installer
 *  \param ssize size of the char * parameters (ip, mask, gw, dns, ntp, key)
 */
void readNetworkConfig(int *dhcp, char *ip, char *mask, char *gw, char *dns, char *ntp, char *key, int ssize)
{
	FILE	*f;
	char	cmd[512], tmp[150];
	char	*pt;

	if (!dhcp || !ip || !mask || !gw || !dns || !ntp || !key)
		return;

	/* Become root for this command */
	userRoot(1);

	// log the command
	snprintf(cmd, sizeof(cmd), "echo %s/netconfig.sh -i '%s' -n '%s' -k '%s' > $ROOTACT/var/log/lrr/netconfig.txt 2>&1", CMD_SHELLS_DEVICE, NetConfInterFile, NetConfNtpFile, NetConfVpnFile);
	system(cmd);

	// execute the command
	snprintf(cmd, sizeof(cmd), "%s/netconfig.sh -i '%s' -n '%s' -k '%s' 2>> $ROOTACT/var/log/lrr/netconfig.txt > " NETCONFIGTMPFILE, CMD_SHELLS_DEVICE, NetConfInterFile, NetConfNtpFile, NetConfVpnFile);
	system(cmd);

	/* Back to suplog user */
	userRoot(0);

	*dhcp=0;
	ip[0] = '\0';
	mask[0] = '\0';
	gw[0] = '\0';
	dns[0] = '\0';
	ntp[0] = '\0';
	key[0] = '\0';

	if ((f = fopen(NETCONFIGTMPFILE, "r")))
	{
		while (fgets(tmp, sizeof(tmp), f))
		{
			// clean '\r' '\n'
			if ((pt = strchr(tmp, '\r')))
				*pt = '\0';
			if ((pt = strchr(tmp, '\n')))
				*pt = '\0';

			// search config lines
			if ((pt = strstr(tmp, "DHCP=")))
				*dhcp = atoi(pt+5);
			else if ((pt = strstr(tmp, "IP=")))
				strncpy(ip, pt+3, ssize);
			else if ((pt = strstr(tmp, "MASK=")))
				strncpy(mask, pt+5, ssize);
			else if ((pt = strstr(tmp, "GW=")))
				strncpy(gw, pt+3, ssize);
			else if ((pt = strstr(tmp, "DNS=")))
				strncpy(dns, pt+4, ssize);
			else if ((pt = strstr(tmp, "NTP=")))
				strncpy(ntp, pt+4, ssize);
			else if ((pt = strstr(tmp, "KEYINST=")))
				strncpy(key, pt+8, ssize);
		}
		fclose(f);
		ip[ssize-1] = '\0';
		mask[ssize-1] = '\0';
		gw[ssize-1] = '\0';
		dns[ssize-1] = '\0';
		ntp[ssize-1] = '\0';
		key[ssize-1] = '\0';
	}
}

/*
 *  \brief read network configuration of the gateway
 *
 *  \param dhcp dhcp activated 0|1
 *  \param ip ip address
 *  \param mask ip mask
 *  \param gw gw address
 *  \param dns dns
 *  \param ntp ntp server
 *  \param key key-installer
 */
int writeNetworkConfig(int dhcp, char *ip, char *mask, char *gw, char *dns, char *ntp, char *key)
{
	char	cmd[512];
	int	ret;

	if (!ip || !mask || !gw || !dns || !ntp || !key)
		return -1;

	/* Become root for this command */
	userRoot(1);

	// log command executed
	snprintf(cmd, sizeof(cmd), "echo %s/netconfig.sh -i '%s' -n '%s' -k '%s' --dhcp '%d' --address '%s' --gw '%s' --dns '%s' --ntp '%s' --mask '%s' --keyinst '%s' >>$ROOTACT/var/log/lrr/netconfig.txt 2>&1" ,
		CMD_SHELLS_DEVICE, NetConfInterFile, NetConfNtpFile, NetConfVpnFile,
		dhcp, ip, gw, dns, ntp, mask, key);
	system(cmd);
	
	// execute thecommand
	snprintf(cmd, sizeof(cmd), "%s/netconfig.sh -i '%s' -n '%s' -k '%s' --dhcp '%d' --address '%s' --gw '%s' --dns '%s' --ntp '%s' --mask '%s' --keyinst '%s' >>$ROOTACT/var/log/lrr/netconfig.txt 2>&1" ,
		CMD_SHELLS_DEVICE, NetConfInterFile, NetConfNtpFile, NetConfVpnFile,
		dhcp, ip, gw, dns, ntp, mask, key);
	ret = system(cmd);

	/* Back to suplog user */
	userRoot(0);
	
	if (ret != 0)
		return -1;
	else
		return 0;
}

void cbCheckBox(newtComponent co, void * data)
{
	struct cbInfoCompo	*listCompo;
	int		i;

	listCompo = data;
	if (listCompo[0].state[0] == ' ')
	{
		for (i=1; i<5; i++)
			newtEntrySetFlags(listCompo[i].en, NEWT_FLAG_DISABLED, NEWT_FLAGS_RESET);
	}
	else
	{
		for (i=1; i<5; i++)
			newtEntrySetFlags(listCompo[i].en, NEWT_FLAG_DISABLED, NEWT_FLAGS_SET);
	}

	newtRefresh();
}

/*
 *  \brief Get network configuration for TPE
 *
 *  \param title window title
 */
void getNetworkConfigTPE(char *title)
{
	char	tmp[80], rescb;
	char	ip[128], mask[128], gw[128], dns[128], ntp[128], key[128];
	int	dhcp, ret;
	struct newtExitStruct es;
	newtComponent   form, btnConf, btnCan, cbDhcp, lblDhcp, entIp, lblIp, entMask, lblMask;
	newtComponent	entGw, lblGw, entDns, lblDns, entNtp, lblNtp, entKey, lblKey;
	struct cbInfoCompo	listCompo[5];

	readNetworkConfig(&dhcp, ip, mask, gw, dns, ntp, key, sizeof(ip));

	newtCenteredWindow(60, 16, title);
	form = newtForm(NULL, NULL, 0);
	lblDhcp = newtLabel(1, 1, "DHCP activated :");
	cbDhcp = newtCheckbox(26, 1, "", (dhcp?'X':' '), " X", &rescb);
	lblIp = newtLabel(1, 3, "IP address :");
	entIp = newtEntry(26, 3, ip, 20, NULL, NEWT_ENTRY_SCROLL);
	lblMask = newtLabel(1, 5, "Netmask :");
	entMask = newtEntry(26, 5, mask, 20, NULL, NEWT_ENTRY_SCROLL);
	lblGw = newtLabel(1, 7, "Gateway :");
	entGw = newtEntry(26, 7, gw, 20, NULL, NEWT_ENTRY_SCROLL);
	lblDns = newtLabel(1, 9, "DNS list :");
	entDns = newtEntry(26, 9, dns, 30, NULL, NEWT_ENTRY_SCROLL);
	lblNtp = newtLabel(1, 11, "NTP server :");
	entNtp = newtEntry(26, 11, ntp, 20, NULL, NEWT_ENTRY_SCROLL);
	lblKey = newtLabel(1, 13, "Key-Installer :");
	entKey = newtEntry(26, 13, key, 20, NULL, NEWT_ENTRY_SCROLL);
	btnConf = newtCompactButton(9, 15, "Confirm");
	btnCan = newtCompactButton(27, 15, "Cancel");
	newtFormAddComponents(form, lblDhcp, cbDhcp, lblIp, entIp, lblMask, entMask, lblGw, entGw,
		lblDns, entDns, lblNtp, entNtp, lblKey, entKey, btnConf, btnCan, NULL);

	// list of component to change depending on checkbox value
	listCompo[0].state = &rescb;
	listCompo[0].en = cbDhcp;
	listCompo[1].en = entIp;
	listCompo[2].en = entMask;
	listCompo[3].en = entGw;
	listCompo[4].en = entDns;
	// callback to change field writability depending on dhcp checkbox value
	newtComponentAddCallback(cbDhcp, cbCheckBox, listCompo);

	if (dhcp)
	{
		int	i;
		for (i=1; i<5; i++)
			newtEntrySetFlags(listCompo[i].en, NEWT_FLAG_DISABLED, NEWT_FLAGS_SET);
	}

	while (1)
	{
		newtFormRun(form, &es);
		printf("Apres newtFormRun\n");
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkNotString(newtEntryGetValue(entIp), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entMask), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entGw), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entDns), COMMANOTALLOWED2) ||
				!checkNotString(newtEntryGetValue(entNtp), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entKey), COMMANOTALLOWED))
			{
				strcpy(tmp, "Character \"" COMMANOTALLOWED "\" is not allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			newtSetColor(NEWT_COLORSET_HELPLINE, "white", "blue");
			newtPushHelpLine("Saving configuration ...");
			newtRefresh();

			ret = writeNetworkConfig((rescb == 'X'), newtEntryGetValue(entIp),
				newtEntryGetValue(entMask), newtEntryGetValue(entGw),
				newtEntryGetValue(entDns), newtEntryGetValue(entNtp),
				newtEntryGetValue(entKey));

			if (ret < 0)
			{
				strcpy(tmp, "Failed to save network configuration ! Check if every parameters are set.");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}
			else
			{
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "blue");
				newtPushHelpLine("Configuration saved, reboot is needed");
				newtRefresh();
			}

			break;
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

/**
 * @brief      Set the isTail and runOnce for dispResult
 *
 * @param      choice  tMenuItem variable with title, command and commandType
 */
void processCommandType(tMenuItem *choice)
{
	char	tmp[MaxCmd];
	char	*pt;

	// Logs can be in different locations
	if ((pt = strstr(choice->command, "_LOGDIR_")))
	{
		// Replace '_LOGDIR_' with log directory
		strcpy(tmp, "(" TAILCMD);
#if	defined(RAMDIR)
		if (isLogInRamDir())
			// directory is hard coded in backuptraces.sh
			strcat(tmp, LOGDIR);
		else
			strcat(tmp, "$ROOTACT/var/log/lrr");
#else
		strcat(tmp, "$ROOTACT/var/log/lrr");
#endif

		// concat what is after '_LOGDIR_'
		strcat(tmp, pt+8);
		strcat(tmp, ") > ");
		strcat(tmp, WorkFile);
		strcat(tmp, " 2>&1");
	}
	else
	{
		snprintf(tmp, sizeof(tmp), "(%s) > %s 2>&1", choice->command, WorkFile);
		tmp[sizeof(tmp)-1] = '\0';
	}

	if (choice->commandType == CMDTYPE_DYNAMIC)
		dispResult(choice->text, tmp, 0, 0, choice->rootPermission);
	else if (choice->commandType == CMDTYPE_TAIL)
		dispResult(choice->text, tmp, 1, 0, choice->rootPermission);
	else
		dispResult(choice->text, tmp, 0, 1, choice->rootPermission);
}

/**
 * @brief      Creates a confirmation window with a message and two buttons cancel and confirm
 *
 * @param      title    The window title
 * @param      message  The message or warning to display
 * @param      cmd      The command to execute
 * @param[in]  commandType  The command type
 */
void createConfirmation(char *title, char *message, char *cmd, int commandType, int rootPermission)
{
	/* Variables declaration */
	struct newtExitStruct es;
	newtComponent   form, lblConf, lblMsg, entAns, btnCan, btnConf;

	/* newt variables and structures */
	newtCenteredWindow(50, 8, title);
	form = newtForm(NULL, NULL, 0);

	lblConf = newtLabel(1, 1, message);
	lblMsg = newtLabel(1, 3, "Enter 'yes' to confirm");
	entAns = newtEntry(1, 5, NULL, 4, NULL, NEWT_ENTRY_SCROLL);

	btnConf = newtCompactButton(9, 7, "Confirm");
	btnCan = newtCompactButton(27, 7, "Cancel");
	newtFormAddComponents(form, lblConf, lblMsg, entAns, btnConf, btnCan, NULL);

	newtFormRun(form, &es);
	if (es.u.co == btnConf)
	{
		if (!strcmp(newtEntryGetValue(entAns), "yes"))
		{
			/* New tMenuItem variable */
			tMenuItem choice = { title, cmd, commandType, rootPermission };
			processCommandType(&choice);
		}
	}

	newtFormDestroy(form);
	newtPopWindow();
}

/**
 * @brief      Ping IP
 *
 * @param      title  The title of the windows
 */
void ping(char *title)
{
	/* Variables declaration */
	struct newtExitStruct es;
	newtComponent   form, lblS, entS, btnConf, btnCan;
	char command[MaxCmd], tmp[80];

	/* newt variables and structures */
	newtCenteredWindow(50, 4, title);
	form = newtForm(NULL, NULL, 0);
	lblS = newtLabel(1, 1, "IP address:");
	entS = newtEntry(26, 1, NULL, 20, NULL, NEWT_ENTRY_SCROLL);
	btnConf = newtCompactButton(9, 3, "Confirm");
	btnCan = newtCompactButton(27, 3, "Cancel");

	newtFormAddComponents(form, lblS, entS, btnConf, btnCan, NULL);

	while (1)
	{
		newtFormRun(form, &es);
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkString(newtEntryGetValue(entS), IPALLOWED, 1))
			{
				strcpy(tmp, "Only alphanumeric characters and " IPALLOWED " are allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			int cmd_type = CMDTYPE_DYNAMIC;
			if (strlen(newtEntryGetValue(entS)) == 0)
			{
				sprintf(command, "echo 'Please enter an address to ping'");
				cmd_type = CMDTYPE_STATIC;
			}
			else
				sprintf(command, "ping -c 10 -w 5 %s", newtEntryGetValue(entS));

			newtSetColor(NEWT_COLORSET_HELPLINE, "white", "blue");
			newtPushHelpLine("Ping in progress, please wait ...");
			newtRefresh();

			/* New tMenuItem variable */
			tMenuItem choice = { title, command, cmd_type, ROOT };
			processCommandType(&choice);

			break;
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

/**
 * @brief      Open a reverse ssh connection
 *
 * @param      title  The window title
 */
void reverseSSH(char *title)
{
	struct newtExitStruct es;
	newtComponent   form, lblReqPort, entReqPort, lblHost, entHost, lblPort, entPort, lblUser, entUser, lblPass, entPass, btnConf, btnCan;
	char command[MaxCmd], tmp[80];

	newtCenteredWindow(50, 12, title);
	form = newtForm(NULL, NULL, 0);

	loadIni();

	char *pt;
	int index = 0;
	char *addr = "";
	char *user = "";
	char *pass = "";
	char *port = "";

	pt = (char *)CfgStr(HtVarLrr,"support",index,"addr","");
	if (pt && *pt)
		addr = strdup(pt);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"user","");
	if (pt && *pt)
		user = strdup(pt);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"pass","");
	if (pt && *pt)
		pass = strdup(pt);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"port","");
	if (pt && *pt)
		port = strdup(pt);

	lblReqPort = newtLabel(1, 1, "Required port:");
	entReqPort = newtEntry(26, 1, "2009", 20, NULL, NEWT_ENTRY_SCROLL);
	lblHost = newtLabel(1, 3, "ssh support host:");
	entHost = newtEntry(26, 3, addr, 20, NULL, NEWT_ENTRY_SCROLL);
	lblPort = newtLabel(1, 5, "ssh support port:");
	entPort = newtEntry(26, 5, port, 20, NULL, NEWT_ENTRY_SCROLL);
	lblUser = newtLabel(1, 7, "ssh support user:");
	entUser = newtEntry(26, 7, user, 20, NULL, NEWT_ENTRY_SCROLL);
	lblPass = newtLabel(1, 9, "Password:");
	entPass = newtEntry(26, 9, pass, 20, NULL, NEWT_ENTRY_SCROLL | NEWT_FLAG_PASSWORD);
	btnConf = newtCompactButton(9, 11, "Confirm");
	btnCan = newtCompactButton(27, 11, "Cancel");
	newtFormAddComponents(form, lblReqPort, entReqPort, lblHost, entHost, lblPort, entPort, lblUser, entUser, lblPass, entPass, btnConf, btnCan, NULL);

	while (1)
	{
		newtFormRun(form, &es);
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkNotString(newtEntryGetValue(entReqPort), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entHost), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entPort), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entUser), COMMANOTALLOWED))
			{
				strcpy(tmp, "Character \"" COMMANOTALLOWED "\" is not allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			sprintf(command, CMD_SHELLS "openssh.sh -P %s -A %s -D %s -U %s -W \"%s\" >/dev/null 2>&1 &", newtEntryGetValue(entReqPort), newtEntryGetValue(entHost), newtEntryGetValue(entPort), newtEntryGetValue(entUser), newtEntryGetValue(entPass));
			system(command);

			sprintf(command, "sleep 2 && pids='pidof sshpass.x' && if eval $pids >/dev/null; then\necho -n 'Reverse SSH PIDs: ' && eval $pids\nelse\necho Cannot open reverse SSH\nfi");
			/* New tMenuItem variable */
			tMenuItem choice = { title, command, CMDTYPE_STATIC, ROOT };
			processCommandType(&choice);

			break;
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

/**
 * @brief      Backup and restore LRR on Kerlink systems
 *
 * @param      title    The window title
 * @param[in]  restore  0: backup LRR, 1: restore LRR
 */
void backup_restore(char *title, const int restore)
{
	/* Variables declaration */
	struct newtExitStruct es;
	newtComponent   form, lblS, entS, btnConf, btnCan;
	char command[MaxCmd], tmp[80];

	/* newt variables and structures */
	newtCenteredWindow(50, 4, title);
	form = newtForm(NULL, NULL, 0);
	lblS = newtLabel(1, 1, "LRR id:");
	entS = newtEntry(26, 1, NULL, 20, NULL, NEWT_ENTRY_SCROLL);
	btnConf = newtCompactButton(9, 3, "Confirm");
	btnCan = newtCompactButton(27, 3, "Cancel");

	newtFormAddComponents(form, lblS, entS, btnConf, btnCan, NULL);

	while (1)
	{
		newtFormRun(form, &es);
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkString(newtEntryGetValue(entS), NUMBERALLOWED, 0))
			{
				strcpy(tmp, "Only " NUMBERALLOWED " are allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			/* Backup */
			if (!restore)
				strcpy(command, CMD_SHELLS_DEVICE "saverff.sh -L ");
			/* Restore */
			else
				strcpy(command, CMD_SHELLS_DEVICE "execrff.sh -L ");

			strcat(command, newtEntryGetValue(entS));

			/* New tMenuItem variable */
			tMenuItem choice = { title, command, CMDTYPE_DYNAMIC, ROOT };
			processCommandType(&choice);

			break;
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

/**
 * @brief      Set the antenna gain and cable attenuation
 *
 * @param      title  The title
 */
void powerTransmissionAdjustement(char *title)
{
	struct newtExitStruct es;
	newtComponent   form, lblNb, entNb, lblGain, entGain, lblCable, entCable, btnConf, btnCan;
	char command[MaxCmd], tmp[80];

	newtCenteredWindow(50, 8, title);
	form = newtForm(NULL, NULL, 0);
	lblNb = newtLabel(1, 1, "Antenna number:");
	entNb = newtEntry(26, 1, "0", 20, NULL, NEWT_ENTRY_SCROLL);
	lblGain = newtLabel(1, 3, "Antenna gain:");
	entGain = newtEntry(26, 3, "6", 20, NULL, NEWT_ENTRY_SCROLL);
	lblCable = newtLabel(1, 5, "Cable attenuation:");
	entCable = newtEntry(26, 5, "3", 20, NULL, NEWT_ENTRY_SCROLL);
	btnConf = newtCompactButton(9, 7, "Confirm");
	btnCan = newtCompactButton(27, 7, "Cancel");
	newtFormAddComponents(form, lblNb, entNb, lblGain, entGain, lblCable, entCable, btnConf, btnCan, NULL);

	while (1)
	{
		newtFormRun(form, &es);
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkString(newtEntryGetValue(entNb), NUMBERALLOWED, 0) || !checkString(newtEntryGetValue(entGain), NUMBERALLOWED, 0) || !checkString(newtEntryGetValue(entCable), NUMBERALLOWED, 0))
			{
				strcpy(tmp, "Only " NUMBERALLOWED " are allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			sprintf(command, CMD_SHELLS "ants.sh -chain%s %s-%s", newtEntryGetValue(entNb), newtEntryGetValue(entGain), newtEntryGetValue(entCable));

			/* New tMenuItem variable */
			tMenuItem choice = { title, command, CMDTYPE_STATIC, ROOT };
			processCommandType(&choice);
			break;
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

void transmitPower(char *title)
{
	struct newtExitStruct es;
	newtComponent   form, lblPw, entPw, btnConf, btnCan;
	char command[MaxCmd], tmp[80];

	newtCenteredWindow(50, 4, title);
	form = newtForm(NULL, NULL, 0);
	lblPw = newtLabel(1, 1, "Transmit power:");
	entPw = newtEntry(26, 1, NULL, 20, NULL, NEWT_ENTRY_SCROLL);
	btnConf = newtCompactButton(9, 3, "Confirm");
	btnCan = newtCompactButton(27, 3, "Cancel");
	newtFormAddComponents(form, lblPw, entPw, btnConf, btnCan, NULL);

	while (1)
	{
		newtFormRun(form, &es);
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkString(newtEntryGetValue(entPw), NUMBERALLOWED, 0))
			{
				strcpy(tmp, "Only " NUMBERALLOWED " are allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			sprintf(command, CMD_SHELLS "setiniparam.sh lgw gen power %s && echo [gen].power=%s added to lgw.ini && echo 'LRR radio will be restarted'", newtEntryGetValue(entPw), newtEntryGetValue(entPw));

			/* New tMenuItem variable */
			tMenuItem choice = { title, command, CMDTYPE_STATIC, ROOT };
			processCommandType(&choice);

			/* Restart radio thread */
			sprintf(command, "touch /tmp/lrrradiorestart");
			system(command);

			break;
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

/**
 * @brief      Change the LRR timezone
 *
 * @param      title  The window title
 */
void changeTimezone(char *title)
{
	struct newtExitStruct es;
	newtComponent   form, lblTimezone, listboxTimezone, btnConf, btnCan, btnUseOwn;
	char command[MaxCmd];

	int tzDisplayHeight = MaxY-10;
	newtCenteredWindow(MaxX-10, MaxY-5, title);

	form = newtForm(NULL, NULL, 0);
	lblTimezone = newtLabel(1, 1, "List of timezones:");
	listboxTimezone = newtListbox(26, 1, tzDisplayHeight, NEWT_FLAG_SCROLL); // NEWT_FLAG_RETURNEXIT not used

	loadIni();

	FILE *fp;
	char *currentLine = NULL;
	size_t lineLength = 0;
	int ptr = 0;
	int lines = 0;

	/* Get timezone list from support ftp */
	char *pt;
	int index = 0;

	pt = (char *)CfgStr(HtVarLrr,"support",index,"ftpaddr","");
	if (pt && *pt)
		setenv  ("FTPHOSTSUPPORT",strdup(pt),1);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"ftpuser","");
	if (pt && *pt)
		setenv  ("FTPUSERSUPPORT",strdup(pt),1);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"ftppass","");
	if (pt && *pt)
		setenv  ("FTPPASSSUPPORT",strdup(pt),1);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"ftpport","");
	if (pt && *pt)
		setenv  ("FTPPORTSUPPORT",strdup(pt),1);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"use_sftp","");
	if (pt && *pt)
		setenv  ("USE_SFTP_SUPPORT",strdup(pt),1);

	sprintf(command, CMD_SHELLS "dntimezone.sh --get-list >/dev/null 2>&1");
	system(command);

	char *timezone_list = "/tmp/TIMEZONE_LIST";

	/* Read the number of line/file in TIMEZONE_LIST */
	fp = fopen( timezone_list, "r");
	if (!fp)
	{
		/* New tMenuItem variable */
		tMenuItem choice = { title, "echo 'TIMEZONE_LIST not found'", CMDTYPE_STATIC, NONROOT };
		processCommandType(&choice);
		newtFormDestroy(form);
		newtPopWindow();
		setOwnTimezone(title);
		return;
	}
	while (getline(&currentLine, &lineLength, fp) != -1)
	{
		lines++;
	}
	fclose(fp);
  
	/* Create a double array with the name to display and the key. Needed for newtListboxAppendEntry */
	char **timezone = malloc (lines * sizeof(char*));

	/* Read TIMEZONE_LIST and fill the newt listbox */
	fp = fopen( timezone_list, "r");
	if (!fp)
	{
		/* New tMenuItem variable */
		tMenuItem choice = { title, "echo 'TIMEZONE_LIST not found'", CMDTYPE_STATIC, NONROOT };
		processCommandType(&choice);
		newtFormDestroy(form);
		newtPopWindow();
		return;
	}

	while (getline(&currentLine, &lineLength, fp) != -1)
	{
		/* Dynamical allocation for timezone */
		timezone[ptr] = malloc(lineLength * sizeof(char*));
		/* Copy the current line into timezone */
		strncpy(timezone[ptr], currentLine, lineLength);
		/* Fill the listbox */
		newtListboxAppendEntry(listboxTimezone, timezone[ptr], timezone[ptr]);
		/* Incremente pointer */
		ptr++;
	}
	fclose(fp);
  
	btnConf = newtCompactButton(MaxX/4 - 8, MaxY-6, "Confirm");
	btnCan = newtCompactButton(MaxX/2 - 10, MaxY-6, "Cancel");
	btnUseOwn = newtCompactButton(3*MaxX/4 - 16, MaxY-6, "Use own timezone");

	newtFormAddComponents(form, lblTimezone, listboxTimezone, btnConf, btnCan, btnUseOwn, NULL);

	newtFormRun(form, &es);
	if (es.u.co == btnConf)
	{
		/* Get on file directory and file basename.*/
		char fileDir[MaxCmd];
		sprintf(fileDir, "%s", (char*) newtListboxGetCurrent(listboxTimezone));
		char *fileBName = strrchr(fileDir, '/');
		if (fileBName != NULL) {
			*fileBName = '\0';
			fileBName++;
			sprintf(command, CMD_SHELLS "dntimezone.sh -T '%s' -D 'TIMEZONE_BASE_LRR/zoneinfo/%s' -L %s -Z %s", fileBName, fileDir, LOCALTIME, TIMEZONE_DEST);
		}
		else
		{
			fileBName = fileDir;
			sprintf(command, CMD_SHELLS "dntimezone.sh -T '%s' -L %s -Z %s", fileBName, LOCALTIME, TIMEZONE_DEST);
		}

		/* New tMenuItem variable */
		tMenuItem choice = { title, command, CMDTYPE_STATIC, ROOT };
		processCommandType(&choice);
	}

	/* free malloc */
	ptr = 0;
	for (ptr = 0; ptr < lines; ptr++)
	{
		free(timezone[ptr]);
	}
	free(timezone);
	newtFormDestroy(form);
	newtPopWindow();

	unsetenv("FTPHOSTSUPPORT");
	unsetenv("FTPUSERSUPPORT");
	unsetenv("FTPPORTSUPPORT");
	unsetenv("FTPPASSSUPPORT");
	unsetenv("USE_SFTP_SUPPORT");

	/* The exit button is the own for using its own ftp */
	if (es.u.co == btnUseOwn)
	{
		setOwnTimezone(title);
	}

}

/**
 * @brief      Sets the timezone with own ftp file
 *
 * @param      title  The window title
 */
void setOwnTimezone(char *title)
{
	struct newtExitStruct es;
	newtComponent   form, lblFtpServer, entFtpServer, lblPort, entPort, lblUser, entUser, lblPassword, entPassword, lblDirectory, entDirectory, lblFile, entFile, lblSftp, entSftp, btnConf, btnCan;
	char command[MaxCmd], tmp[80];

	newtCenteredWindow(50, 16, title);
	form = newtForm(NULL, NULL, 0);

	loadIni();

	char *pt;
	int index = 0;
	char *ftpaddr = "";
	char *ftpuser = "";
	char *ftppass = "";
	char *ftpport = "";
	char *use_sftp = "";

	pt = (char *)CfgStr(HtVarLrr,"support",index,"ftpaddr","");
	if (pt && *pt)
		ftpaddr = strdup(pt);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"ftpuser","");
	if (pt && *pt)
		ftpuser = strdup(pt);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"ftppass","");
	if (pt && *pt)
		ftppass = strdup(pt);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"ftpport","");
	if (pt && *pt)
		ftpport = strdup(pt);

	pt = (char *)CfgStr(HtVarLrr,"support",index,"use_sftp","");
	if (pt && *pt)
		use_sftp = strdup(pt);

	lblFtpServer = newtLabel(1, 1, "ftp address:");
	entFtpServer = newtEntry(26, 1, ftpaddr, 20, NULL, NEWT_ENTRY_SCROLL);
	lblPort = newtLabel(1, 3, "Port:");
	entPort = newtEntry(26, 3, ftpport, 20, NULL, NEWT_ENTRY_SCROLL);
	lblUser = newtLabel(1, 5, "User:");
	entUser = newtEntry(26, 5, ftpuser, 20, NULL, NEWT_ENTRY_SCROLL);
	lblPassword = newtLabel(1, 7, "Password:");
	entPassword = newtEntry(26, 7, ftppass, 20, NULL, NEWT_ENTRY_SCROLL | NEWT_FLAG_PASSWORD);
	lblDirectory = newtLabel(1, 9, "Directory of the file:");
	entDirectory = newtEntry(26, 9, "TIMEZONE_BASE_LRR/zoneinfo/Europe", 20, NULL, NEWT_ENTRY_SCROLL);
	lblFile = newtLabel(1, 11, "Timezone file:");
	entFile = newtEntry(26, 11, "Paris", 20, NULL, NEWT_ENTRY_SCROLL);
	lblSftp = newtLabel(1, 13, "Use sftp:");
	entSftp = newtEntry(26, 13, use_sftp, 20, NULL, NEWT_ENTRY_SCROLL);

	btnConf = newtCompactButton(9, 15, "Confirm");
	btnCan = newtCompactButton(27, 15, "Cancel");
	newtFormAddComponents(form, lblFtpServer, entFtpServer, lblPort, entPort, lblUser, entUser, lblPassword, entPassword, lblDirectory, entDirectory, lblFile, entFile, lblSftp, entSftp, btnConf, btnCan, NULL);

	while (1)
	{
		newtFormRun(form, &es);
		if (es.u.co == btnConf)
		{
			/* check if string is autorized */
			if (!checkString(newtEntryGetValue(entFile), ALPHAALLOWED, 1))
			{
				strcpy(tmp, "Only alphanumeric characters are allowed for Timezone file !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}

			/* check if string is autorized */
			if (!checkNotString(newtEntryGetValue(entDirectory), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entFtpServer), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entPort), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entUser), COMMANOTALLOWED) ||
				!checkNotString(newtEntryGetValue(entSftp), COMMANOTALLOWED))
			{
				strcpy(tmp, "Character \"" COMMANOTALLOWED "\" is not allowed !");
				newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
				newtPushHelpLine(tmp);
				newtRefresh();
				continue;
			}


			sprintf(command, CMD_SHELLS "dntimezone.sh -T '%s' -D '%s' -A '%s' -P '%s' -U '%s' -W '%s' -S '%s'", newtEntryGetValue(entFile), newtEntryGetValue(entDirectory), newtEntryGetValue(entFtpServer), newtEntryGetValue(entPort), newtEntryGetValue(entUser), newtEntryGetValue(entPassword), newtEntryGetValue(entSftp));

			/* New tMenuItem variable */
			tMenuItem choice = { title, command, CMDTYPE_STATIC, ROOT };
			processCommandType(&choice);

			break;
		}
		else
			break;
	}

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

/**
 * @brief      Edit a text file with root permission
 *
 * @param      title  The window title
 * @param      file   The file to modifiy
 */
void textEditor(char *title, char *file)
{
	struct newtExitStruct es;
	newtComponent   form, lblCareful, lblMsg, entAns, btnConf, btnCan;
	char command[MaxCmd], msg[MaxCmd];

	sprintf(msg,"Enter 'yes' to edit %s", file);

	newtCenteredWindow(50, 8, title);
	form = newtForm(NULL, NULL, 0);

	lblCareful = newtLabel(1, 1, "Modifiying this file can break the configuration!");
	lblMsg = newtLabel(1, 3, msg);
	entAns = newtEntry(1, 5, NULL, 4, NULL, NEWT_ENTRY_SCROLL);

	btnConf = newtCompactButton(9, 7, "Confirm");
	btnCan = newtCompactButton(27, 7, "Cancel");
	newtFormAddComponents(form, lblCareful, lblMsg, entAns, btnConf, btnCan, NULL);

	newtFormRun(form, &es);
	if (es.u.co == btnConf)
	{
		if (!strcmp(newtEntryGetValue(entAns), "yes"))
		{
			userRoot(1);
			sprintf(command, TERMINFO " && $ROOTACT/lrr/nano/nano %s", file);
			system(command);
			userRoot(0);
		}
	}

	newtFormDestroy(form);
	newtPopWindow();

	/* Set newtm colors */
	newtSetColor(NEWT_COLORSET_ROOT, "white", "green");
	newtSetColor(NEWT_COLORSET_ROOTTEXT, "white", "green");
	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtRefresh();
}

/**
 * @brief      Ping LRCs
 *
 * @param      title  The window title
 */
void pingLRCS(char *title)
{
	loadIni();

	u_int NbLrc = 0;
	int ptr = 0;
	NbLrc = CfgInt(HtVarLrr,"lrr",-1,"nblrc",1);
	if (NbLrc > NB_LRC_PER_LRR)
		NbLrc = NB_LRC_PER_LRR;

	struct newtExitStruct es;
	char command[MaxCmd], msg[80];
	newtComponent   form, lbLRC, listboxLRC, btnConf, btnCan;
	newtCenteredWindow(50, 10, title);
	form = newtForm(NULL, NULL, 0);
	lbLRC = newtLabel(1, 1, "List of LRCs:");
	listboxLRC = newtListbox(26, 1, 5, NEWT_FLAG_SCROLL | NEWT_FLAG_RETURNEXIT);
	btnConf = newtCompactButton(9, 9, "Confirm");
	btnCan = newtCompactButton(27, 9, "Cancel");
	newtFormAddComponents(form, lbLRC, listboxLRC, btnConf, btnCan, NULL);

	/* Create a double array with the name to display and the key. Needed for newtListboxAppendEntry */
	char **lrcs = malloc (NbLrc * sizeof(char*));

	for (ptr=0; ptr<NbLrc; ptr++)
	{
		int lrcsize = strlen(CfgStr(HtVarLrr,"laplrc",ptr,"addr","0.0.0.0"));
		lrcs[ptr] = malloc(lrcsize * sizeof(char*));
		sprintf(lrcs[ptr],CfgStr(HtVarLrr,"laplrc",ptr,"addr","0.0.0.0"));
		newtListboxAppendEntry(listboxLRC, lrcs[ptr], lrcs[ptr]);
	}

	newtFormRun(form, &es);
	if (es.u.co != btnCan)
	{
		sprintf(command, "ping -c 10 -w 5 %s", (char*) newtListboxGetCurrent(listboxLRC));
		sprintf(msg, "Ping LRC %s in progress, please wait ...", (char*) newtListboxGetCurrent(listboxLRC));

		newtSetColor(NEWT_COLORSET_HELPLINE, "white", "blue");
		newtPushHelpLine(msg);
		newtRefresh();

		/* New tMenuItem variable */
		tMenuItem choice = { title, command, CMDTYPE_STATIC, ROOT };
		processCommandType(&choice);

	}

	/* free malloc */
	ptr = 0;
	for (ptr = 0; ptr < NbLrc; ptr++)
	{
		free(lrcs[ptr]);
	}
	free(lrcs);

	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");
	newtPushHelpLine(NULL);
	newtRefresh();
	newtFormDestroy(form);
	newtPopWindow();
}

/**
 * @brief      Debug whoami
 *
 * @param      title  The window title
 */
void debugWhoami(char *title)
{
  char command[MaxCmd];
  sprintf(command, " echo -n ruid && echo ' %i' && echo -n euid && echo ' %i' && echo -n suid && echo ' %i'", ruid, euid, suid);

  /* New tMenuItem variable */
  tMenuItem choice = { title, command, CMDTYPE_STATIC, NONROOT };
  processCommandType(&choice);
}

/**
 * if no access to "Network Files editable" is granted we check the name of the
 * menu
 */
int isANetworkConfigMenu(tMenuItem *menu)
{
	if (NetConfEditable == 1)
		return 0;

	if (strcmp("editEth",menu->command) == 0) return 1;
	if (strcmp("editAPN",menu->command) == 0) return 1;
	if (strcmp("editETCHOSTS",menu->command) == 0) return 1;
	if (strcmp("editDNS",menu->command) == 0) return 1;
	if (strcmp("editNTP",menu->command) == 0) return 1;

	return 0;
}

/**
 * Check if network config for TP Enterprise allowed
 * menu
 */
int isANetworkConfigTPEMenu(tMenuItem *menu)
{
	if (strcmp("editEthTPE",menu->command) == 0 && !NetConfTPE) return 1;

	return 0;
}

 /**
  * @brief      activate menu
  *
  * @param      title   The title
  * @param      menu    The menu
  * @param[in]  szMenu  size of the menu
  */
void runMenu(char *title, tMenuItem *menu, int szMenu)
{
	struct newtExitStruct es;
	newtComponent   form, bExit, lb;
	int     i, nbEnt, nbDisp = 0;

	// calculate number of entries in menu
	nbEnt = szMenu / sizeof(tMenuItem);
	for (i=0; i<nbEnt; i++)
	{
		if (isANetworkConfigMenu(&menu[i])) continue;
		if (isANetworkConfigTPEMenu(&menu[i])) continue;
		nbDisp++;
	}

	// create window
	newtCenteredWindow(50, nbDisp+6, title);

	// create form
	form = newtForm(NULL, NULL, 0);

	// create buttons
	if (!strcmp(title, "Main menu"))
		bExit = newtButton(20, nbDisp+2, "Exit");
	else
		bExit = newtButton(20, nbDisp+2, "Back");

	// create listbox
	lb = newtListbox(1, 1, nbDisp, NEWT_FLAG_RETURNEXIT);

	// add entries in listbox
	for (i=0; i<nbEnt; i++)
	{
		if (isANetworkConfigMenu(&menu[i])) continue;
		if (isANetworkConfigTPEMenu(&menu[i])) continue;
		newtListboxAppendEntry(lb, menu[i].text, &(menu[i]));
	}

	// add in form
	newtFormAddComponents(form, lb, bExit, NULL);

	// run form
	newtFormRun(form, &es);
	while (es.u.co != bExit)
	{
		tMenuItem *choice = newtListboxGetCurrent(lb);
		if (choice->command && *(choice->command))
		{
			/* if FUNCTION */
			if (choice->commandType == CMDTYPE_FUNCTION)
			{
				if (!strcmp(choice->command, "functionExportLog"))
					getTransferInfos(choice->text);
				else if (!strcmp(choice->command, "functionHalt"))
					createConfirmation(choice->text, "Are you sure you want to turn off Linux?", "halt", CMDTYPE_STATIC, ROOT);
				else if (!strcmp(choice->command, "functionReboot"))
					createConfirmation(choice->text, "Are you sure you want to reboot the lrr?", "reboot", CMDTYPE_STATIC, ROOT);
				else if (!strcmp(choice->command, "functionRestartLRR"))
					createConfirmation(choice->text, "Are you sure you want to restart the lrr process?", "killall lrr.x && sleep 1 && (killall -9 lrr.x 2> /dev/null)", CMDTYPE_STATIC, ROOT);
				else if (!strcmp(choice->command, "functionReverseSSH"))
					reverseSSH(choice->text);
				else if (!strcmp(choice->command, "functionPing"))
					ping(choice->text);
				else if (!strcmp(choice->command, "functionBackup"))
					backup_restore(choice->text, 0);
				else if (!strcmp(choice->command, "functionRestore"))
					backup_restore(choice->text, 1);
				else if (!strcmp(choice->command, "functionPowerTrAd"))
					powerTransmissionAdjustement(choice->text);
				else if (!strcmp(choice->command, "functionTrPower"))
					transmitPower(choice->text);
				else if (!strcmp(choice->command, "functionTimezone"))
					changeTimezone(choice->text);
				else if (!strcmp(choice->command, "editEth"))
					textEditor(choice->text, NETWORK);
				else if (!strcmp(choice->command, "editEthTPE"))
					getNetworkConfigTPE(choice->text);
				else if (!strcmp(choice->command, "editAPN"))
					textEditor(choice->text, APN);
				else if (!strcmp(choice->command, "editETCHOSTS"))
					textEditor(choice->text, "/etc/hosts");
				else if (!strcmp(choice->command, "editDNS"))
					textEditor(choice->text, "/etc/resolv.conf");
				else if (!strcmp(choice->command, "editNTP"))
					textEditor(choice->text, "/etc/ntp.conf");
				else if (!strcmp(choice->command, "functionPingLRCs"))
					pingLRCS(choice->text);
				else if (!strcmp(choice->command, "functionWhoami"))
					debugWhoami(choice->text);
			}

			/* If GREP */
			else if (choice->commandType == CMDTYPE_GREP)
				getGrepInfos(choice->command);

			/* if MENU */
			else if (choice->commandType == CMDTYPE_MENU)
			{
				// awful, but need to use sizeof
				if (!strcmp(choice->command, "MenuLog"))
					runMenu(choice->text, MenuLog, sizeof(MenuLog));
				else if (!strcmp(choice->command, "MenuVizLog"))
					runMenu(choice->text, MenuVizLog, sizeof(MenuVizLog));
				else if (!strcmp(choice->command, "MenuTailLog"))
					runMenu(choice->text, MenuTailLog, sizeof(MenuTailLog));
				else if (!strcmp(choice->command, "MenuGrepLog"))
					runMenu(choice->text, MenuGrepLog, sizeof(MenuGrepLog));
				else if (!strcmp(choice->command, "MenuLRRConfig"))
					runMenu(choice->text, MenuLRRConfig, sizeof(MenuLRRConfig));
				else if (!strcmp(choice->command, "MenuNetwork"))
					runMenu(choice->text, MenuNetwork, sizeof(MenuNetwork));
				else if (!strcmp(choice->command, "MenuTrouble"))
					runMenu(choice->text, MenuTrouble, sizeof(MenuTrouble));
				else if (!strcmp(choice->command, "MenuServices"))
					runMenu(choice->text, MenuServices, sizeof(MenuServices));
				else if (!strcmp(choice->command, "MenuVPN"))
					runMenu(choice->text, MenuVPN, sizeof(MenuVPN));
				else if (!strcmp(choice->command, "MenuDebug"))
					runMenu(choice->text, MenuDebug, sizeof(MenuDebug));
			}

			/* if STATIC, DYNAMIC or TAIL */
			else
				processCommandType(choice);

		}
		newtFormRun(form, &es);
	}

	// destroy all things created
	newtFormDestroy(form);
	newtPopWindow();
}

/*
 *  \brief main
 */
int main(int argc, char *argv[])
{
	char tmp[1024], *pt;

	/* Get real UID, effective UID and save set-user ID */
	getresuid(&ruid, &euid, &suid);


#if defined(WIRMAV2) || defined(CISCOMS) || defined(FCLOC) || defined(FCMLB) || defined(FCPICO) || defined(FCLAMP) || defined(TEKTELIC) || defined(GEMTEK)
	char dir[256];
	int i;
	// set ROOTACT
	pt = getenv("ROOTACT");
	if (!pt)
	{
		// dirname of argv[0] is ".", current directory is $HOME
		// use "SHELL" variable
		pt = getenv("SHELL");
		if (pt == NULL)
			return -1;
		SAFESTRCPY(dir, pt);
		// dir is "/mnt/fsuser-1/actility/lrr/suplog/suplog.x",
		// must get "/mnt/fsuser-1/actility/"
		for (i=0; i<3; i++)
		{
			pt = dirname(dir);
			if (!pt)
				return -1;
			strcpy(dir, pt);
		}
		setenv("ROOTACT", dir, 0);
	}
	// set PATH, to find ifconfig and route
	pt = getenv("PATH");
	if (pt)
	{
		sprintf(tmp, "%s:/sbin", pt);
		setenv("PATH", tmp, 1);
	}
#endif

#if defined(WIRMAAR) || defined(WIRMANA)
	// set ROOTACT
	pt = getenv("ROOTACT");
	if (!pt)
	{
		setenv("ROOTACT", "/user/actility", 0);
	}
	// set PATH, to find ifconfig and route
	pt = getenv("PATH");
	if (pt)
	{
		sprintf(tmp, "%s:/sbin", pt);
		setenv("PATH", tmp, 1);
	}
#endif

	/* Initialization */
	initSystem();
	initDisplay();

	/* The workfile file needs to be created as suplog user first */
	sprintf(WorkFile, "/tmp/_suplog_%d", getpid());
	sprintf(tmp, "( echo '/tmp/_suplog_%d creation' ) > %s 2>&1", getpid(), WorkFile);
	system(tmp);

	/* Set newtm colors */
	newtSetColor(NEWT_COLORSET_ROOT, "white", "green");
	newtSetColor(NEWT_COLORSET_ROOTTEXT, "white", "green");
	newtSetColor(NEWT_COLORSET_HELPLINE, "white", "green");

	/* display version on first line */
	sprintf(tmp, "Actility support tool %s", SUPLOGVER);
	newtDrawRootText(0, 0, tmp);
	newtPushHelpLine(NULL);
	newtRefresh();

	/* Load all ini in hash tables */
	if (loadIni())
	{
		newtSetColor(NEWT_COLORSET_HELPLINE, "white", "red");
		newtPushHelpLine("Error loading .ini");
		newtRefresh();
	}

	// read values in [system/suplog] for default lrr.ini, but also in [suplog] for custom lrr.ini
	sprintf(tmp, "%s/suplog", System);
	NetConfEditable		= CfgInt(HtVarLrr,tmp,-1,"networkconfigeditable",0);
	NetConfEditable		= CfgInt(HtVarLrr,"suplog",-1,"networkconfigeditable",NetConfEditable);
	NetConfTPE		= CfgInt(HtVarLrr,tmp,-1,"networkconfigtpe",0);
	NetConfTPE		= CfgInt(HtVarLrr,"suplog",-1,"networkconfigtpe",NetConfTPE);
	NetConfInterFile	= CfgStr(HtVarLrr,tmp,-1,"networkconfiginterfile",NetConfInterFile);
	NetConfInterFile	= CfgStr(HtVarLrr,"suplog",-1,"networkconfiginterfile",NetConfInterFile);
	NetConfNtpFile		= CfgStr(HtVarLrr,tmp,-1,"networkconfigntpfile",NetConfNtpFile);
	NetConfNtpFile		= CfgStr(HtVarLrr,"suplog",-1,"networkconfigntpfile",NetConfNtpFile);
	NetConfVpnFile		= CfgStr(HtVarLrr,tmp,-1,"networkconfigvpnfile",NetConfVpnFile);
	NetConfVpnFile		= CfgStr(HtVarLrr,"suplog",-1,"networkconfigvpnfile",NetConfVpnFile);

	/* Launch main menu */
	runMenu("Main menu", MenuMain, sizeof(MenuMain));

	/* clear all things */
	endDisplay();
	unlink(WorkFile);

	return 0;
}

