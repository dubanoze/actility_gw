#include	<unistd.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

int	optf	= 0;
int	optb	= 0;

int	optc	= -1;
int	optC	= -1;

int	minc	= 1000000;

int	opth	= 0;	// hybride mode 8,16,32,64,72
int	optv	= 0;	// verify unicity
int	opte	= 0;	// full empty definition

unsigned int	f	= 0;
unsigned int	rstep	= 200000;
unsigned int	bandw	= 125;

unsigned int	h;
unsigned int	nbh	= 8;	// hybride mode 8 channels only

unsigned int	tbchan[1024];
int		nbchan		= 0;
int		nbchanmax	= 128;

int	used(unsigned int f,int *ret)
{
	int	i;

	*ret	= -1;
	if	(!optv)
		return	0;
	for	(i = 0 ; i < nbchanmax ; i++)
	{
		if	(tbchan[i] == 0)
			continue;
		if	(tbchan[i] == f)
		{
			*ret	= i;
			return	1;
		}
	}
	return	0;
}

void	usage()
{
printf("genband:\n");
printf("--------\n");
printf("--help\n");
printf("	this help\n");
printf("-c firstnum\n");
printf("	first channel number to use\n");
printf("-C lastnum\n");
printf("	last channel number to use\n");
printf("-f freq[:step]\n");
printf("	start frequency spaced of step hz. Each time the -f is encountered,\n");
printf("	logicals channels from -cfirstnum to -Clastnum are generated.\n");
printf("	-cfirstnum and -Clastnum must be redefined before each use of -f\n");
printf("-h firstlc\n");
printf("	first logical channel to use in hybride mode.\n");
printf("	This option is required to generate physical channels mapping.\n");
printf("-e\n");
printf("	empty definition to discard channel inheritance from default configuration file\n");
printf("\n");
printf("Cautions:\n");
printf("--------\n");
printf("- usage of [:step] is discouraged, keep default values\n");
printf("- this tool does not produce full configuration files but only the parts\n");
printf("concerning frequencies and logical/physical channels.\n");
printf("\n");
printf("Examples:\n");
printf("--------\n");
printf("- to generate logical channels file for standard EU868_2015 (channels_eu868_2015.ini):\n");
printf("	genband -c1 -C4 -f868100000 -c5 -C8 -f867100000\n");
printf("\n");
printf("- to generate physical channels file for EU868_2015 (lgw_eu868_2015.ini) on a\n");
printf("single SX1301 board starting listening channels at LC1:\n");
printf("	genband -h1 -c1 -C4 -f868100000 -c5 -C8 -f867100000\n");
}

void	generatefreq(int c,int C,int frq,int step)
{
	int	i;
	int	ret;

	for	(i = c ; i <= C ; i++)
	{
		if	(opte)
		{
			printf	("[channel:%d]\n",i);
			printf	("	name=LC%d\n",i);
			printf	("	freqhz=\n");
			continue;
		}
		if	(optv && used(frq,&ret))
		{
			printf("ERROR u1/%d freq=%u already used channel=%d\n",
				i,frq,ret);
			exit(1);
		}
		if	(!opth)
		{
			printf	("[channel:%d]\n",i);
			printf	("	name=LC%d\n",i);
			printf	("	subband=%d\n",1);
			printf	("	freqhz=%u\n",frq);
			switch	(bandw)
			{
			case	500 :
			printf	("	bandwidth=%s\n","${BW_500KHZ}");
			break;
			case	125 :
			default :
			printf	("	bandwidth=%s\n","${BW_125KHZ}");
			break;
			}
		}

		tbchan[i]	= frq;
		frq	= frq + step;
		nbchan++;
	}
}

int	main(int argc,char *argv[])
{

	int	error	= -1;
	int	nbopt	= 0;
	int	opt;
	int	i;
	int	ret;

	unsigned int step;
	unsigned int frq;

	if	(argc >=2 && strcmp(argv[1],"--help") == 0)
	{
		usage();
		exit(0);
	}

	while((opt=getopt(argc, argv, "evc:C:f:h:"))!=-1 && error==-1) 
	{
	switch(opt) 
	{
	case	'e' :
		nbopt++;
		opte	= 1;
	break;
	case	'v' :
		nbopt++;
		optv	= 1;
	break;
	case	'c' :
		nbopt++;
		optc	= atoi(optarg);
		if	(minc > optc)
			minc	= optc;
	break;
	case	'b' :
		nbopt++;
		optb	= atoi(optarg);
	break;
	case	'C' :
		nbopt++;
		optC	= atoi(optarg);
	break;
	case	'f' :
		if	(optc < 0 || optC < 0)
		{
			printf("ERROR : missing options ...\n");
			usage();
			exit(1);
		}
		nbopt++;
		optf	= atoi(optarg);
		sscanf(optarg,"%u:%u",&f,&rstep);
		if	(f < 1000)
			f	= f * 1000000;
		if	(f < 10000)
			f	= f * 100000;
		generatefreq(optc,optC,f,rstep);
		optc	= -1;
		optC	= -1;
	break;
	case	'h' :
		nbopt++;
		opth	= 1;
		h	= 1;
		if	(strlen(optarg))
			h	= atoi(optarg);
	break;
	}
	}

	if	(nbopt == 0)
	{
		printf("ERROR : missing options ...\n");
		usage();
		exit(1);
	}

	if	(!opth)
		exit(0);

	// generate configuration for hybride mode

	int	hmin	= h;
	int	hmax	= h + nbh - 1;
	unsigned int fmin;
	unsigned int fmax;
	unsigned int fave;

	unsigned int tbchansort[128];;

	char	rfcom[2][128];
	unsigned int rf[2];
	unsigned int ic[8];
	int	r;
	int	j;

	for	(i = hmin , j = 0 ; i <= hmax ; i++ , j++)
	{
		tbchansort[j]	= tbchan[i];
	}

	{
		inline int intcmp(const void *c1, const void *c2)
		{
			int	f1	= *(int *)c1;
			int	f2	= *(int *)c2;

			return	f1 - f2;
		}
		qsort	(tbchansort,8,sizeof(int),intcmp);

	}

#if	0
	for	(i = 0 ; i < 8 ; i++)
	{
		printf	("; %u\n",tbchansort[i]);
	}
#endif

	fmin	= tbchansort[hmin];
	fmax	= tbchansort[hmax];
	fave	= (fmin + fmax) / 2;
	rf[0]	= (tbchansort[0] + tbchansort[3]) / 2;
	rf[1]	= (tbchansort[4] + tbchansort[7]) / 2;

	sprintf	(rfcom[0],"(LC%d+LC%d)/2",hmin+0,hmin+3);
	sprintf	(rfcom[1],"(LC%d+LC%d)/2",hmin+4,hmin+7);

#if	0
	printf	("chan[%d]=%u .... chan[%d]=%u average=%u\n",
			hmin,fmin,hmax,fmax,fave);
#endif

#if	0
	printf	("rf0=%u rf1=%u\n",rf[0],rf[1]);
#endif

	for	(r = 0 ; r < 2 ; r++)
	{
		printf	("[rfconf:%d]\n",r);
		printf	("	enable=%d\n",1);
		printf	("	freqhz=%u	; %s\n",rf[r],rfcom[r]);
	}

	r	= 0;
	for	(i = 0 ; i < 8 ; i++)
	{
		int	step;
		char	iccom[128];

		if	(i >= 4)
			r = 1;
		step	= tbchansort[i]-rf[r];
		ic[i]	= rf[r]+step;
		sprintf	(iccom,"(LC%d-rfconf[%d].freqhz)=(%u-%u)\n",hmin+i,r,
					tbchansort[i],rf[r]);
		printf	("[ifconf:%d]\n",i);
		printf	("	enable=%d\n",1);
		printf	("	rfchain=%d\n",r);
		printf	("	freqhz=%d	; %s\n",step,iccom);
	}

	// verifs

	for	(i = 0 ; i < 8 ; i++)
	{
		if	(ic[i] != tbchansort[i])
		{
			printf	("ERROR ifconf[%d] != tbchansort[%d]\n",i,i);
		}
	}

	exit(0);
}
