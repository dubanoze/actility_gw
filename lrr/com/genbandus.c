#include	<unistd.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

int	optu1	= 0;	// up channels 125K
int	optu5	= 0;	// up channels 500k
int	optd5	= 0;	// down channels 500k
int	opth	= 0;	// hybride mode 8,16,32,64,72
int	optv	= 0;	// verify unicity
int	opte	= 0;	// full empty definition

unsigned int	u1	= 902300000;
unsigned int	u1step	= 200000;

unsigned int	u5	= 903000000;
unsigned int	u5step	= 1600000;

unsigned int	d5	= 923300000;
unsigned int	d5step	= 600000;

unsigned int	h;
unsigned int	nbh	= 8;	// hybride mode 8 channels only

unsigned int	tbchan[1024];
int		nbchan = 128;

int	used(unsigned int f,int *ret)
{
	int	i;

	*ret	= -1;
	if	(!optv)
		return	0;
	for	(i = 0 ; i < nbchan ; i++)
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
printf("genbandus:\n");
printf("--------\n");
printf("--help\n");
printf("	this help\n");
printf("-u freq[:step]\n");
printf("	first frequency for upstream band LC0..LC63/125Khz spaced of step hz\n");
printf("-U freq[:step]\n");
printf("	first frequency for upstream band LC64..LC72/500Khz spaced of step hz\n");
printf("-d freq[:step]\n");
printf("	first frequency for downstream band LCd0..LCd7/500Khz spaced of step hz\n");
printf("-h 0|8|16|32|64\n");
printf("	first logical channel to use in upstream bands in hybride mode.\n");
printf("	This option is required to generate physical channels mapping.\n");
printf("-v\n");
printf("	verify unicity of each frequency use (-u -U -d are mandatory)\n");
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
printf("- to generate logical channels file for standard US915 (channels_us915.ini):\n");
printf("	genbandus -v -u902300000 -U903000000 -d923300000\n");
printf("\n");
printf("- to generate physical channels file for hybride US915 (lgw_us915h8.ini) on a\n");
printf("single SX1301 board starting listening channels at LC0:\n");
printf("	genbandus -u902300000 -U903000000 -d923300000 -h0\n");
printf("\n");
printf("- to redefine only the first band (LC0..LC63/125Khz) and to use LC8..LC15 as listening channels: \n");
printf("	genbandus -u915100000\n");
printf("	genbandus -u915100000 -h8\n");
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

	while((opt=getopt(argc, argv, "evu:U:d:h:"))!=-1 && error==-1) 
	{
	switch(opt) 
	{
	case	'e' :
		nbopt++;
		opte	= 1;
		optu1	= optu5	= optd5	= 1;
	break;
	case	'v' :
		nbopt++;
		optv	= 1;
	break;
	case	'u' :
		nbopt++;
		optu1	= 1;
		sscanf(optarg,"%u:%u",&u1,&u1step);
	break;
	case	'U' :
		nbopt++;
		optu5	= 1;
		sscanf(optarg,"%u:%u",&u5,&u5step);
	break;
	case	'd' :
		nbopt++;
		optd5	= 1;
		sscanf(optarg,"%u:%u",&d5,&d5step);
	break;
	case	'h' :
		nbopt++;
		opth	= 1;
		h	= 10000000;
		if	(strlen(optarg))
			h	= atoi(optarg);
		switch	(h)
		{
		case 0 : case 8 : case 16 : case 32 : case 64 :
		break;
		default :
			printf("invalid first channel %d for hybride mode\n",h);
			exit(1);
		break;
		}
	break;
	}
	}

	if	(nbopt == 0)
	{
		printf("ERROR : missing options ...\n");
		usage();
		exit(1);
	}
	if	(opte == 0 && optv == 1 && (!optu1 || !optu5 || !optd5 ))
	{
		printf("ERROR : missing options ...\n");
		usage();
		exit(1);
	}

	// up u1
	frq	= u1;
	step	= u1step;
	for	(i = 0 ; i < 64 ; i++)
	{
		if	(opte && optu1)
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
		if	(!opth && optu1)
		{
			printf	("[channel:%d]\n",i);
			printf	("	name=LC%d\n",i);
			printf	("	subband=%d\n",20);
			printf	("	freqhz=%u\n",frq);
			printf	("	bandwidth=%s\n","${BW_125KHZ}");
		}

		tbchan[i]	= frq;
		frq	= frq + step;
	}

	// up u5
	frq	= u5;
	step	= u5step;
	for	(i = 64 ; i < 72 ; i++)
	{
		if	(opte && optu5)
		{
			printf	("[channel:%d]\n",i);
			printf	("	name=LC%d\n",i);
			printf	("	freqhz=\n");
			continue;
		}
		if	(optv && used(frq,&ret))
		{
			printf("ERROR u5/%d freq=%u already used channel=%d\n",
				i,frq,ret);
			exit(1);
		}
		if	(!opth && optu5)
		{
			printf	("[channel:%d]\n",i);
			printf	("	name=LC%d\n",i);
			printf	("	subband=%d\n",30);
			printf	("	freqhz=%u\n",frq);
			printf	("	bandwidth=%s\n","${BW_500KHZ}");
		}

		tbchan[i]	= frq;
		frq	= frq + step;
	}

	// down d5
	frq	= d5;
	step	= d5step;
	for	(i = 127 ; i < 135 ; i++)
	{
		if	(opte && optd5)
		{
			printf	("[channel:%d]\n",i);
			printf	("	name=LCd%d\n",i-127);
			printf	("	freqhz=\n");
			continue;
		}
		if	(optv && used(frq,&ret))
		{
			printf("ERROR d5/%d freq=%u already used channel=%d\n",
				i,frq,ret);
			exit(1);
		}
		if	(!opth && optd5)
		{
			printf	("[channel:%d]\n",i);
			printf	("	name=LCd%d\n",i-127);
			printf	("	subband=%d\n",40);
			printf	("	freqhz=%u\n",frq);
			printf	("	bandwidth=%s\n","${BW_500KHZ}");
			if	(i == 127)
			{
			printf	("	usedforrx2=%d\n",1);
			printf	("	dataraterx2=%s\n","${DR_LORA_SF12}");
			printf	("	power=+%u\n",0);
			}
		}

		tbchan[i]	= frq;
		frq		= frq + step;
	}

	if	(!opth)
		exit(0);

	// generate configuration for hybride mode

	int	hmin	= h;
	int	hmax	= h + nbh - 1;
	unsigned int fmin;
	unsigned int fmax;
	unsigned int fave;

	char	rfcom[2][128];
	unsigned int rf[2];
	unsigned int ic[8];
	int	r;

	fmin	= tbchan[hmin];
	fmax	= tbchan[hmax];
	fave	= (fmin + fmax) / 2;
	rf[0]	= (tbchan[hmin+0] + tbchan[hmin+3]) / 2;
	rf[1]	= (tbchan[hmin+4] + tbchan[hmin+7]) / 2;

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
		step	= tbchan[hmin+i]-rf[r];
		ic[i]	= rf[r]+step;
		sprintf	(iccom,"(LC%d-rfconf[%d].freqhz)=(%u-%u)\n",hmin+i,r,
					tbchan[hmin+i],rf[r]);
		printf	("[ifconf:%d]\n",i);
		printf	("	enable=%d\n",1);
		printf	("	rfchain=%d\n",r);
		printf	("	freqhz=%d	; %s\n",step,iccom);
	}

	// verifs

	for	(i = 0 ; i < 8 ; i++)
	{
		if	(ic[i] != tbchan[hmin+i])
		{
			printf	("ERROR ifconf[%d] != tbchan[%d]\n",i,hmin+i);
		}
	}

	exit(0);
}
