#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "rtlbase.h"
#include "rtlhtbl.h"

extern char *optarg;

// Variable element
typedef struct s_varElem {
	char			*name;
	char			*value;
	struct s_varElem	*next;
} t_varElem;

// Section element
typedef struct s_secElem {
	char			*name;
	struct s_varElem	*vars;
	struct s_secElem	*next;
} t_secElem;

t_secElem	*IniCont = NULL;	// Ini content
FILE		*OutFd;			// Output file
int		Verbose=0;		// Verbose option

// Callback called for each variable found in an ini file
int iniLoadCB(void *user,const char *section,const char *name,const char *value)
{
	t_secElem	*pts, *pts_prev, *newsec;
	t_varElem	*ptv, *ptv_prev, *newvar;
	int		cmpSec=1, cmpVar=1;

	if (Verbose) printf("iniLoadCB: sec='%s', name='%s', value='%s'\n", section, name, value);
	// search section
	pts = IniCont;
	pts_prev = NULL;
	while (pts)
	{
		cmpSec = strcmp(pts->name, section);
		if (Verbose) printf("sec strcmp('%s', '%s') = %d\n", pts->name, section, cmpSec);

		// section found or after in the alphabetic order
		if (cmpSec >= 0)
			break;

		pts_prev = pts;
		pts = pts->next;
	}

	// section not found
	if (!pts || cmpSec != 0)
	{
		if (Verbose) printf("create section '%s'\n", section);
		// create section
		newsec = (t_secElem *) malloc(sizeof(t_secElem));
		if (!newsec)
			return -1;
		newsec->name = strdup(section);
		newsec->next = NULL;
		newsec->vars = NULL;

		// link with previous section
		if (pts_prev)
			pts_prev->next = newsec;
		else
			IniCont = newsec;	// Section must be inserted before the first one

		// link with next section
		if (pts)
			newsec->next = pts;

		// IniCont empty
		if (!IniCont)
			IniCont = newsec;

		pts = newsec;
	}

	// at this point the section exists and 'pts' is set on it
	// search variable
	ptv = pts->vars;
	ptv_prev = NULL;
	while (ptv)
	{
		cmpVar = strcmp(ptv->name, name);
		if (Verbose) printf("var strcmp('%s', '%s') = %d\n", ptv->name, name, cmpVar);

		// variable found or after in the alphabetic order
		if (cmpVar >= 0)
			break;

		ptv_prev = ptv;
		ptv = ptv->next;
	}

	// if variable exists just change the value
	if (cmpVar == 0)
	{
		if (Verbose) printf("change var '%s'='%s'\n", name, value);
		ptv->value = strdup(value);
		return 0;
	}

	// variable must be created
	newvar = (t_varElem *) malloc(sizeof(t_varElem));
	if (!newvar)
		return -1;
	newvar->name = strdup(name);
	newvar->value = strdup(value);
	newvar->next = NULL;
	if (Verbose) printf("create var '%s'='%s'\n", name, value);

	// link with next variable
	if (ptv)
		newvar->next = ptv;

	// link with previous variable
	if (ptv_prev)
		ptv_prev->next = newvar;
	else
		pts->vars = newvar;	// first var of the section

	return	1;
}

// Help
void use()
{
	printf("moreini.x -t <targetfile> -a <addfile> [-y] [-o <outfile>] [-h]\n");
	printf("Merge <targetfile> and <addfile> in <targetfile>\n");
	printf("  -o <outfile>: do not erase <targetfile>, result is written in <outfile>\n");
	printf("  -y: do not ask for confirmation before erasing <targetfile>\n");
	printf("  -h: help\n");
}

// Write output file
// Also used for displaying trace in verbose mode
void writeNewFile(char *fn, int dump)
{
	struct tm	stm;
	time_t		now;
	t_secElem	*pts;
	t_varElem	*ptv;
	char		tab[2];

	if (!IniCont)
	{
		printf("Nothing to do !\n");
		exit(0);
	}

	if (!dump)
	{
		OutFd = fopen(fn, "w");
		if (!OutFd)
		{
			printf("Can not open file '%s' to write result !\n", fn);
			exit(0);
		}
		now = time(NULL);
		localtime_r(&now, &stm);
		fprintf(OutFd, ";\n; File generated by moreini.x %04d/%02d/%02d %02d:%02d:%02d\n;\n\n",
			stm.tm_year+1900, stm.tm_mon+1, stm.tm_mday, stm.tm_hour, stm.tm_min, stm.tm_sec);
	}
	else
		OutFd = stdout;

	tab[0] = '\0';
	tab[1] = '\0';
	pts = IniCont;
	while (pts)
	{
		if (pts->name && *(pts->name))
		{
			fprintf(OutFd, "\n[%s]\n", pts->name);
			tab[0] = '\t';
		}
		ptv = pts->vars;
		while (ptv)
		{
			fprintf(OutFd, "%s%s=%s\n", tab, ptv->name, ptv->value);
			ptv = ptv->next;
		}
		pts = pts->next;
	}

	if (!dump)
		fclose(OutFd);
}

// Dump ini content
void dumpIniCont()
{
	if (Verbose) printf("IniCont:\n");

	writeNewFile(NULL, 1);
}

// Main
int main(int argc, char *argv[])
{
	struct stat	st;
	int	err, confirm=1, opt, ret;
	char	*targetfn = NULL;
	char	*addfn = NULL;
	char	*outfn = NULL;
	char	buf[4096];

	while ((opt=getopt(argc, argv, "t:a:o:hyv")) != -1)
	{
		switch (opt)
		{
			case 'h':
				use();
				break;
			case 'v':
				Verbose=1;
				break;
			case 'y':
				confirm=0;
				break;
			case 't':
				targetfn = strdup(optarg);
				break;
			case 'a':
				addfn = strdup(optarg);
				break;
			case 'o':
				outfn = strdup(optarg);
				break;
		}
	}

	// missing "target" or "add" filename
	if (!targetfn || !addfn)
	{
		fprintf(stderr, "Error: missing target file or add file !\n");
		use();
		exit(1);
	}

	// "add" file does not exist
	if (stat(addfn, &st) < 0 )
	{
		fprintf(stderr, "Error: file '%s' doesn't exist !\n", addfn);
		use();
		exit(1);
	}

	// "target" file does not exist, copy "add" file
	if (stat(targetfn, &st) < 0)
	{
		if (outfn)
			ret = snprintf(buf, sizeof(buf), "echo \"; File generated by moreini.x $(date '+%%Y/%%m/%%d %%H:%%M:%%S')\" > %s; cat %s >> %s", outfn, addfn, outfn);
		else
			ret = snprintf(buf, sizeof(buf), "echo \"; File generated by moreini.x $(date '+%%Y/%%m/%%d %%H:%%M:%%S')\" > %s; cat %s >> %s", targetfn, addfn, targetfn);
		if (ret >= sizeof(buf))
			fprintf(stderr, "Error: file name too long ! (%s)\n", buf);
		else
		{
//			printf(buf);
			system(buf);
		}
		return 0;
	}

	if (Verbose) printf("Load %s\n", targetfn);
	if ((err=rtl_iniParse(targetfn, iniLoadCB, IniCont)) < 0)
	{
		fprintf(stderr, "parse error=%d\n", err);
		exit(1);
	}
	if (Verbose) printf("Load %s\n", addfn);
	if ((err=rtl_iniParse(addfn, iniLoadCB, IniCont)) < 0)
	{
		fprintf(stderr, "parse error=%d\n", err);
		exit(1);
	}

	if (Verbose) dumpIniCont();

	if (outfn)
		writeNewFile(outfn, 0);
	else
	{
		if (confirm)
		{
			printf("Overwrite file '%s' ? (y|N) > ", targetfn);
			scanf("%s", buf);
			if (buf[0] == 'y' || buf[0] == 'Y')
				writeNewFile(targetfn, 0);
			else
				printf("Aborted, file not overwritten\n");
		}
		else
			writeNewFile(targetfn, 0);
	}

	return 0;
}
