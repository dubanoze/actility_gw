#ifndef NOSSL
#include <stdio.h>
#include <stdlib.h>

#define	NB_KEY	3

// warning: ‘mk’ defined but not used [-Wunused-variable]	static	char	*mk = "----------------------------------------";
static	unsigned int	_Build(int version,int pos,int *bidon)
{
	int	ret	= 0;

	*bidon	= 0;
	switch	(version)
	{
	case	2:	// A LRR FEEDS AES 128B (with phone keypad L=5 R=S=7)
	switch	(pos)
	{
	case	0: *bidon = pos+version; ret =	0xA + (*bidon<<16);	break; 
	case	1: *bidon = pos+version; ret =	0x5 + (*bidon<<16);	break;
	case	2: *bidon = pos*version; ret =	0x7 + (*bidon<<16);	break;
	case	3: *bidon = pos+version; ret =	0x7 + (*bidon<<16);	break;
	case	4: *bidon = pos+version; ret =	0xF + (*bidon<<16);	break;
	case	5: *bidon = pos*version; ret =	0xE + (*bidon<<16);	break;
	case	6: *bidon = pos+version; ret =	0xE + (*bidon<<16);	break;
	case	7: *bidon = pos+version; ret =	0xD + (*bidon<<16);	break;
	case	8: *bidon = pos*version; ret =	0x7 + (*bidon<<16);	break;
	case	9: *bidon = pos+version; ret =	0xA + (*bidon<<16);	break;
	case	10: *bidon = pos+version; ret =	0xE + (*bidon<<16);	break;
	case	11: *bidon = pos*version; ret =	0x7 + (*bidon<<16);	break;
	case	12: *bidon = pos*version; ret =	0x1 + (*bidon<<16);	break;
	case	13: *bidon = pos+version; ret =	0x2 + (*bidon<<16);	break;
	case	14: *bidon = pos+version; ret =	0x8 + (*bidon<<16);	break;
	case	15: *bidon = pos+version; ret =	0xB + (*bidon<<16);	break;
	default:
		ret	= 0;
	break;
	}
	break;
	case	1:
	switch	(pos)
	{
	case	0: *bidon = pos*version; ret =	0x9 + (*bidon<<16);	break; 
	case	1: *bidon = pos+version; ret =	0x9 + (*bidon<<16);	break;
	case	2: *bidon = pos*version; ret =	0x9 + (*bidon<<16);	break;
	case	3: *bidon = pos+version; ret =	0x9 + (*bidon<<16);	break;
	case	4: *bidon = pos+version; ret =	0x9 + (*bidon<<16);	break;
	case	5: *bidon = pos*version; ret =	0x9 + (*bidon<<16);	break;
	case	6: *bidon = pos*version; ret =	0x9 + (*bidon<<16);	break;
	case	7: *bidon = pos+version; ret =	0x9 + (*bidon<<16);	break;
	case	8: *bidon = pos*version; ret =	0x9 + (*bidon<<16);	break;
	case	9: *bidon = pos*version; ret =	0x9 + (*bidon<<16);	break;
	case	10: *bidon = pos+version; ret =	0x9 + (*bidon<<16);	break;
	case	11: *bidon = pos*version; ret =	0x9 + (*bidon<<16);	break;
	case	12: *bidon = pos+version; ret =	0x9 + (*bidon<<16);	break;
	case	13: *bidon = pos*version; ret =	0x9 + (*bidon<<16);	break;
	case	14: *bidon = pos+version; ret =	0x9 + (*bidon<<16);	break;
	case	15: *bidon = pos+version; ret =	0x9 + (*bidon<<16);	break;
	default:
		ret	= 0;
	break;
	}
	break;
	case	0:
	default:
	switch	(pos)
	{
	case	0: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break; 
	case	1: *bidon = pos*version; ret =	0xaa + (*bidon<<16);	break;
	case	2: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break;
	case	3: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break;
	case	4: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break;
	case	5: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break;
	case	6: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break;
	case	7: *bidon = pos*version; ret =	0xaa + (*bidon<<16);	break;
	case	8: *bidon = pos*version; ret =	0xaa + (*bidon<<16);	break;
	case	9: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break;
	case	10: *bidon = pos*version; ret =	0xaa + (*bidon<<16);	break;
	case	11: *bidon = pos*version; ret =	0xaa + (*bidon<<16);	break;
	case	12: *bidon = pos*version; ret =	0xaa + (*bidon<<16);	break;
	case	13: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break;
	case	14: *bidon = pos*version; ret =	0xaa + (*bidon<<16);	break;
	case	15: *bidon = pos+version; ret =	0xaa + (*bidon<<16);	break;
	default:
		ret	= 0;
	break;
	}
	break;
	}
	return	ret;
}


unsigned char	*BuildBin(int version)
{
	int	i;
	int	result;
	unsigned char *pt;

	result	= 0;
	pt	= malloc(16);
	if	(!pt)
		return	NULL;
	for	(i = 0 ; i < 16 ; i++)
	{
		*(pt+i)	= (unsigned char)_Build(version,i,&result);
	}
	return	pt;
}

unsigned char	*BuildHex(int version)
{
	int	i;
	int	j;
	int	result;
	unsigned char c;
	unsigned char *pt;

	result	= 0;
	pt	= malloc(33);
	if	(!pt)
		return	NULL;
	for	(i = 0 , j = 0 ; i < 16 ; i++ , j = j + 2)
	{
		c	= (unsigned char)_Build(version,i,&result);
		sprintf	((char *)pt+j,"%02x",c);
	}
	pt[32]	= '\0';
	return	pt;
}

#ifdef	MAIN
void	main()
{
	int	k;
	int	i;
	int	result;
	unsigned char *pt;

	for	(k = 0 ; k < NB_KEY ; k++)
	{
		pt	= BuildBin(k);
		for	(i = 0 ; i < 16 ; i++)
			printf	("%02x",*(pt+i));
		printf	("\n");
		free	(pt);
	}

	for	(k = 0 ; k < NB_KEY ; k++)
	{
		pt	= BuildHex(k);
		printf	("%s\n",pt);
		free	(pt);
	}
}
#endif
#endif
