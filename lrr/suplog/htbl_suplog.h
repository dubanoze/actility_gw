#ifndef HTBL_SUPLOG_H
#define HTBL_SUPLOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Needed for PATH_MAX */
#include <limits.h>

/* .h where we can find rtl_htblCreateSpec */
#include "rtlhtbl.h"

/* .h where we can find rtl_iniParse and rtl_iniLoadCB */
#include "rtlbase.h"

/* Needed for NB_LRC_PER_LRR */
// #include "infrastruct.h"
#define	NB_LRC_PER_LRR	5 // LRC declared by LRR

/* BuildHex is in keybuild.c in lrr/com */
unsigned char *BuildHex(int version);

/* BuildHex is in keybuild.c in lrr/com */
unsigned char *BuildHex(int version);

/* lrr_keyDec is in keycrypt.c in lrr/com */
int lrr_keyDec(unsigned char *ciphertext,int ciphertext_len,unsigned char *key, unsigned char *iv, unsigned char *plaintext,int maxlen,int aschex);

/* Functions */
int loadIni();
char *CfgStr(void *ht,char *sec,int index,char *var,char *def);
int  CfgInt(void *ht,char *sec,int index,char *var,int def);

/* Global variables */
char	ConfigDefault[PATH_MAX];
char	ConfigCustom[PATH_MAX];
void	*HtVarLrr;	// hash table for variables LRR lrr.ini
void	*HtVarLgw;	// hash table for variables LGW lgw.ini
void	*HtVarSys;	// hash table for variables "system"

#endif // HTBL_SUPLOG_H