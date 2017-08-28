#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>

#include "libgen/libgen.h"

//#define DEBUG 
#define MAX_INPUT 64

void printUsage()
{
    char prog[] = "keygen";
    printf("\nusage: %s create a hash for LRR configuration", prog);
    printf("\n-p                              set if running in production environment");
    printf("\n-t                              set if prod and runtime");
    printf("\n-r                              generate a root password");
    printf("\n-s                              generate a support password");
    printf("\n-k                              generate a usb installation key");
    printf("\n-c <string>                     key if -p is not set");
    printf("\n-h                              print this page");
    printf("\n");
}

struct options {
    int prod;
    int runtime;
    int r_passgen;
    int s_passgen;
    int u_keygen;
    char customer[MAX_INPUT];
};

int main (int argc, char **argv) 
{
    int c;
    char pass[14];
    struct options opt;
    memset(&opt, 0, sizeof(opt));

    char *prefix = NULL;
    extern char *optarg;
    extern int optind, optopt;
    while ((c = getopt(argc, argv, "prskhtc:")) != -1)
    {
      switch (c)
      {
        case 'p' :
          opt.prod = 1;
          break;
        case 't' :
          opt.runtime= 1;
          break;
        case 'r' :
          opt.r_passgen = 1;
          prefix = "root";
          break;
        case 's' :
          opt.s_passgen = 1;
          prefix = "support";
          break;
        case 'k' :
          opt.u_keygen = 1;
          prefix = "usbkey";
          break;
        case 'c' :
          if (strlen(optarg) <= MAX_INPUT) {
              memcpy(opt.customer, optarg, strlen(optarg));
              opt.prod = 0;
          }
          else {
              printf("Invalid Key\n");
              printUsage();
              exit(1);
          }
          break;
        case 'h' :
          printUsage();
          exit(0);
          break;
        default:
          printUsage();
          exit(1);
          break;
      }
    }

    if (! opt.prod &&
        opt.customer == NULL) {
        printf("The key is mandatory in non production environment\n");
        printUsage();
        exit (1);
    }

    if (!opt.r_passgen && !opt.s_passgen && !opt.u_keygen) {
        printf("Please specify a type of key\n");
        printUsage();
        exit (1);
    }

    if (opt.prod) {
        char cmd[64];
        if (opt.runtime) {
            snprintf(cmd, 45 , "cat /etc/HOSTNAME | awk -F_ \'{ printf $2 }\'");
        }
        else {
            snprintf(cmd, 44 , "cat etc/HOSTNAME | awk -F_ \'{ printf $2 }\'");
        }

        FILE *in;
        if(!(in = popen(cmd, "r"))){
            printf("Error retrieving key information\n");
            exit (1);
        }
        else {
            fgets(opt.customer, MAX_INPUT, in);
        }
        pclose(in);
    }

    keygen_t keygen;
    char secret[32];
    unsigned int tot_len = strlen (opt.customer) + strlen(prefix) + 1;

    snprintf(secret, tot_len+1, "%s+%s", prefix, opt.customer);

#ifdef DEBUG
    printf("secret %s length %d\n", secret, tot_len);
#endif

    init(&keygen);
    generate(&keygen, secret, tot_len+1);
    retrieve(&keygen, pass);

    printf("%s_key:%s\n", prefix, pass);
    return 0;
}

