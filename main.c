/* Prodos Emulator main segment
   by Matthew Ghio
   version 0.1 of January 8, 1996
   
   Initializes the ProDOS MLI emulator, initializes a virtual Apple IIe,
   loads the apple.rom file into $C100, loads the system file at $2000,
   and runs the program using Randy Frank's 65C02 emulator and I/O code.
*/

#include "apple.h"
#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define SOUT 0x300
#define REF_DELAY 2000
#define ROMFILE "apple.rom"

extern char prodosroot[];
extern int port;
extern FILE * log;

char * rcsid="$Revision: 1.8 $";

void
usage (char *arg0)
{
  fprintf (stderr, "Usage: %s [args] sysfile\n", arg0);
  fprintf (stderr, "   [-r directory] Root directory of Prodos filesystem\n");
  fprintf (stderr, "   [-w directory] Set working directory\n");
  fprintf (stderr, "   [-p port] Set serial port\n");
  fprintf (stderr, "   [-t] Trace execution\n");
  fprintf (stderr, "   [-s] Silent mode\n");
  exit (1);
}

void
main (int argc, char **argv)
{
  short int byte;
  int i, addr;			/* Loop iterators */
  int refdelay = REF_DELAY;
  FILE *fp;
  int fd;
  char sysfile[256] = "";
  char buffer[1024];
  struct sockaddr_in a;
  short int b;

  BeepNoise = 1;

	if ((log=fopen("prodos.log","a"))==NULL) {
		fprintf(stderr,"Error log\n");
		exit(-1);
	}

	fprintf(log,"Start %s\n",rcsid);
	fflush(log);

/* Load the ROM file */
  if ((fd = open (ROMFILE, O_RDONLY)) > 0)
    {
      read (fd, &Rom[0x100], 0x3f00);
      close (fd);
    }
  else
    {
      printf ("Warning: could not load %s\n", ROMFILE);
      fd=open("/usr/local/lib/apple.rom",O_RDONLY);
      read(fd,&Rom[0x100],0x3f00); 
      close(fd);
    }

/* Interpret args */

  for (i = 1; i <= argc - 1; i++)
    {
      if (!strcmp (argv[i], "-t"))
	DebugSingle = 1;
      else if (!strcmp (argv[i], "-s"))
	BeepNoise = 0;
      else if (!strcmp (argv[i], "-r"))
	{
	  /* Set Prodos root dir */
	  if (argv[++i][0] == '/')
	    strcpy (prodosroot, argv[i]);
	  else
	    {
	      getcwd (prodosroot, 1024);
	      strcat (prodosroot, "/");
	      strcat (prodosroot, argv[i]);
	    }
	}
      else if (!strcmp (argv[i], "-w"))
	{
	  /* Set working dir */
	  if (chdir (argv[++i]))
	    {
	      fprintf (stderr, "Can't chdir to %s");
	    }
	}
      else if (!strcmp (argv[i], "-p"))
	{
	  /* Set serial port */
	  port = atoi (argv[++i]);
	}
      else if (!strcmp (argv[i], "-i"))
	{
	  /* Set ip */
	  ip=(char*)malloc(16);
	  strcpy(ip,argv[++i]);
	}
      else if (!sysfile[0] && argv[i][0] != '-')
	strcpy (sysfile, argv[i]);
      else
	usage (argv[0]);
    }
  if (!*sysfile)
    usage (argv[0]);

/* Load prodos sys file at $2000 */

  if ((fp = fopen (sysfile, "r")) == NULL)
    {
      printf ("Can't open %s\n", sysfile);
      exit (1);
    };
  addr = 0x2000;
  while ((byte = fgetc (fp)) != EOF)
    MegaPutMem (addr++, byte);
  (void) fclose (fp);

/* Initialize the emulation: */

  /* Set up some zero page locations */
  MegaPutMem (32, 0);
  MegaPutMem (33, 40);		/* 40 columns */
  MegaPutMem (34, 0);
  MegaPutMem (35, 24);		/* 24 rows */
  MegaPutMem (50, 255);		/*Normal text mode (this value is a bitwise AND mask) */
  MegaPutMem (54, 0xf0);
  MegaPutMem (55, 0xfd);	/* Character Out */
  MegaPutMem (56, 0x1b);
  MegaPutMem (57, 0xfd);	/* Character In */

  /* Set up the Prodos Global Page */
  MegaPutMem (0xBF98, 0xB3);	/* =IIe with 80 columns and 128K */
  MegaPutMem (0xBF9A, 0xFF);	/* Set the active prefix flag */

  (void) initscr ();
  InitProdos ();
  MegaStartUp ();
  CPUReset ();
  nonl ();
  paddle0 = 0.5, paddle1 = 0.5;
  PPC = 0x2000;			/* Jump to start of executable file */

  
/* Begin 65C02 emulation loop */
  while (!MegaQuitDetect)
    {
      CPUExecute ();
      if (!(--refdelay))
	{
	  refresh ();
	  refdelay = REF_DELAY;
	  byte = GetC000 ((ADDR) (0xC000));	/* allow the user to interrupt ALWAYS... */
	}
      if (MegaSerial)
	byte = GetC000 ((ADDR) (0xC0A8));
      if (MegaSerialOut)
	{
	  memset (&a, '\0', sizeof (a));
	  a.sin_family = AF_INET;
	  a.sin_port = htons (port+1);
	  a.sin_addr.s_addr = inet_addr ("10.59.65.118");
	  buffer[0] = MegaGetMem (SOUT);
	  if (buffer[0])
	    {
	      if (sendto (sockfd, buffer, 1, 0, (const struct sockaddr *)&a, sizeof (a)) < 0)
		fprintf (stderr, "error sending %x %s on %x\n\r", buffer[0],
			 strerror (errno), sockfd);
	    }
	  MegaPutMem (SOUT, 0);
	}
      if (DebugSingle || DebugTrace)
	Debugger ();
    }

/* Exit cleanly: */
  MegaShutDown ();
  fclose(log);

  exit (0);
}
