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
 * \file    htbl_suplog.c
 * \brief   Needed for reading all the .ini
 * \author  Benjamin Chomarat, Actility
 */

#include "htbl_suplog.h"

/* Definition of configuration files */
char	*ConfigFileParams	= "_parameters.sh";
char	*ConfigFileSystem	= "_system.sh";
char	*ConfigFileCustom	= "custom.ini";
char	*ConfigFileGps		= "gpsman.ini";
char	*ConfigFileDefine	= "defines.ini";
char	*ConfigFileLrr		= "lrr.ini";
char	*ConfigFileState	= "_state.ini";


char    *CfgStr(void *ht,char *sec,int index,char *var,char *def)
{
        return  rtl_iniStr(ht,sec,index,var,def);
}

int     CfgInt(void *ht,char *sec,int index,char *var,int def)
{
        return  rtl_iniInt(ht,sec,index,var,def);
}

int CfgCBIniLoad(void *user,const char *section,const char *name,const char *value)
{
        return rtl_iniLoadCB(user,section,name,value);
}


char	*DoConfigFileCustom(char *file)
{
	static	char	path[1024];

	sprintf	(path,"%s/%s",ConfigCustom,file);
	if	(access(path,R_OK) == 0)
		return	path;
	return	NULL;
}

char	*DoConfigFileDefault(char *file,char *suff)
{
	static	char	path[1024];

	if	(suff && *suff)
	{
		char	tmp[128];
		char	*pt;

		strcpy	(tmp,file);
		pt	= strstr(tmp,".ini");
		if	(pt)
		{
			*pt	= '\0';
			sprintf	(path,"%s/%s_%s.ini",ConfigDefault,tmp,suff);
		}
		else
			sprintf	(path,"%s/%s_%s",ConfigDefault,tmp,suff);
	}
	else
		sprintf	(path,"%s/%s",ConfigDefault,file);
	if	(access(path,R_OK) == 0)
		return	path;

	return	NULL;	// ism.band vs ism.bandlocal
}

/**
 * @brief      Decrypte the hashtable
 *
 * @param      htbl  The htbl
 */
void	DecryptHtbl(void *htbl)
{
	inline	int	CBHtWalk(void *htbl,char *pvar,void *pvalue,void *p)
	{
		char	*value	= (char *)pvalue;
		char	var[512];
		char	varext[512];
		char	plaintext[1024];
		char	*pt;
		int	keynum;
		int	ret;
		int	lg;
		u_char	*keyvalhex;

		if	(!value)
			return	0;
		strcpy	(var,pvar);
		sprintf	(varext,"%s_crypted_k",var);
		pt	= rtl_htblGet(htbl,varext);
		if	(!pt || !*pt)
			return	0;
		// this variable is declared as crypted 
		lg	= strlen(value);
		if	(lg == 0 || *value != '[' || value[lg-1] != ']')
		{
			// but the crypted value is "" or not between [] 
			// => considered as not crypted
//RTL_TRDBG(1,"'%s' declared as _crypted_k=%s but empty or not [...]\n", var,pt);
			rtl_htblRemove(htbl,varext);
			return	0;
		}
		value[lg-1]	= '\0';
		value++;
		keynum		= atoi(pt);
#ifdef NOSSL
		keyvalhex	= NULL;
#else
		keyvalhex	= BuildHex(keynum);
#endif
		if	(!keyvalhex)
			return	0;

//RTL_TRDBG(1,"decrypt '%s' key=%d '%s'\n",var,keynum,value);
		plaintext[0]	= '\0';
#ifdef NOSSL
		ret = -1;
#else
		ret	= lrr_keyDec((u_char *)value,-1,
				keyvalhex,NULL,(u_char *)plaintext,1000,0);
#endif
		if	(ret >= 0)	// "" is a possible decrypted value
		{
			plaintext[ret]	= '\0';
#if	0
RTL_TRDBG(1,"decrypt'%s' ret=%d '%s'\n",var,ret,plaintext);
#endif
			rtl_htblRemove(htbl,varext);
			rtl_htblRemove(htbl,var);
			rtl_htblInsert(htbl,var,(void *)strdup(plaintext));
		}
		else
		{
//RTL_TRDBG(1,"decrypt '%s' ret=%d ERROR\n",var,ret);
		}

		free(keyvalhex);
		return	0;
	}

	/* Read the entire hash table and apply the CBHtWalk function */
	rtl_htblWalk(htbl,CBHtWalk,NULL);
}

/**
 * @brief      Loads a configuration define.
 */
static	void	LoadConfigDefine()
{
	char	*file;

	file	= DoConfigFileCustom(ConfigFileSystem);
	if	(file)
		rtl_iniParse(file,CfgCBIniLoad,HtVarSys);


	file	= DoConfigFileCustom(ConfigFileParams);
	if	(file)
		rtl_iniParse(file,CfgCBIniLoad,HtVarSys);


	file	= DoConfigFileDefault(ConfigFileDefine,NULL);
	if	(file )
		rtl_iniParse(file,CfgCBIniLoad,HtVarLrr);


	file	= DoConfigFileDefault(ConfigFileDefine,NULL);
	if	(file)
		rtl_iniParse(file,CfgCBIniLoad,HtVarLgw);

	// TODO ??
	// LgwForceDefine(HtVarLrr);
	// LgwForceDefine(HtVarLgw);
}

/**
 * @brief      Loads a configuration lrr.
 */
static	void	LoadConfigLrr()
{
	char	*file;

	file	= DoConfigFileDefault(ConfigFileLrr,NULL);
	if	(file)
		rtl_iniParse(file,CfgCBIniLoad,HtVarLrr);

	file	= DoConfigFileCustom(ConfigFileLrr);
	if	(file)
		rtl_iniParse(file,CfgCBIniLoad,HtVarLrr);

	file	= DoConfigFileCustom(ConfigFileGps);
	if	(file)
		rtl_iniParse(file,CfgCBIniLoad,HtVarLrr);

	file	= DoConfigFileCustom(ConfigFileCustom);
	if	(file)
		rtl_iniParse(file,CfgCBIniLoad,HtVarLrr);

	file	= DoConfigFileCustom(ConfigFileState);
	if	(file)	
		rtl_iniParse(file,CfgCBIniLoad,HtVarLrr);

	DecryptHtbl(HtVarLrr);
}

/**
 * @brief      Read all the .ini files
 *
 * @return     Return 0 if ok, -1 if hash tables can't be initialized
 */
int loadIni()
{
	char	*RootAct	= NULL;
	RootAct	= getenv("ROOTACT");

	sprintf	(ConfigDefault,"%s/%s",RootAct,"lrr/config");
	sprintf	(ConfigCustom,"%s/%s",RootAct,"usr/etc/lrr");
	          	
	if (HtVarLrr)
	{
		rtl_htblDestroy(HtVarLrr);
		rtl_htblDestroy(HtVarLgw);
		rtl_htblDestroy(HtVarSys);
	}
			

	HtVarLrr	= rtl_htblCreateSpec(25,NULL,
						HTBL_KEY_STRING|HTBL_FREE_DATA);

	HtVarLgw	= rtl_htblCreateSpec(25,NULL,
						HTBL_KEY_STRING|HTBL_FREE_DATA);

	HtVarSys	= rtl_htblCreateSpec(25,NULL,
						HTBL_KEY_STRING|HTBL_FREE_DATA);

	if	(!HtVarLrr || !HtVarLgw || !HtVarSys)
	{
		return -1;
	}

	LoadConfigDefine();
	LoadConfigLrr();

	return 0;
}
