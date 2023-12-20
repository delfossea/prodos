/* ProDOS emulator v0.1 */
/* by Matthew Ghio */
/* January 8, 1996 */

/* This program emulates the functionality of ProDOS, an Apple II based
   operating system which was introduced in 1984.  Prodos uses a
   hierarchically ordered filesystem, in which files are organized into
   subdirectories.  Filenames are case-insensitive under Prodos (though
   usually considered to be uppercase).  Applications communicate with
   the operating system via the Machine Language Interface (MLI), which
   is called via a JSR $BF00.  The MLI pulls the program counter off
   the stack, and reads the next three bytes of data following the JSR
   instruction.  The first byte is the function code (open, read, write,
   close, delete, etc).  The second and third byte contain a (16-bit)
   pointer to a parameter data structure.  The meaning of the parameters
   varies depending on the function call.  After the MLI is finished, the
   program counter is increased by three, and control is returned to the
   application.  Error codes are returned in the accumulator (A) register,
   with zero indicating no error.  If the return code is non-zero, the
   carry bit is also set.
   The MLI call is trapped by the 6502 emulator and passed to the mli()
   routine, which performs the functions of the Prodos kernel by calling
   the equivilent unix functions.  The 6502 processor is LSB-first, so
   all values in Prodos's data structures are also.  I sometimes use the
   Apple convention of writing hex numbers with a $ preceding them, such
   as $FE, which is decimal 254. */

/* History:

   v0.1 - Jan 8, 1996
     Implemented all Prodos 1.0 MLI calls except ALLOC_INTERRUPT,
     DEALLOC_INTERRUPT, READ_BLOCK, and WRITE_BLOCK.
     No clock emulation.
   v0.0 - December 19, 1995
*/
/*
Things to do:
  - Clock emulation and dates/times on files;
      store the creation date in .prodosdir
  - Somehow emulate prodos file permissions by doing chmods
  - Update the 48K memory map at $BF58-$BF6F
  - Update other locations in ProDOS Global Page (which?)
  - Have better bounds checking on parameters passed to mli
  - Make GET_BUF return something meaningful
  - Fix all the bugs.

Things to maybe do:
  - Allow mounting of disk images (big project, unless someone has some
      code to handle the prodos filesystem that they'd like to contribute)
  - Emulate /RAM on S3D2?
  - Support interrupts?
*/

#include "apple.h"
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

/* Some global variables */
char prodosroot[1024] = "";	/* Unix directory that will become prodos's root dir */
char prodosvolname[18];		/* /UNIX */
char prodospfx[64];		/* The prefix we return with calls to GET_PREFIX */
char prodosfilename[64];	/* Buffer for a prodos pathname */
char unixfilename[1024];	/* and its unix translation */
struct stat statbuf;		/* For calls to stat() */
int prodosfd[8];		/* Mapping from Prodos's file descriptors to unix fds.
				   Prodos has a maximum of 8 open files. */
int prodosofstat[8];		/* Status of open files:
				   0=no open file
				   1=open file for r/w
				   2=open file, read only
				   3=open directory, read only  */
unsigned char prodosofnewlinemask[8];	/* Newline AND mask, zero=disabled */
unsigned char prodosofnewlinechar[8];	/* Newline character, set by $C9 call */
DIR *opendirp[8];		/* For calls to readdir on open dirs */
long int prodosofptr[8];	/* Current file position in a prodos open file
				   (used only for reading directories) */
long int prodosofeof[8];	/* EOF for open directories */
char prodosdirname[8][16];	/* Names of the subdirectories we have open */
char unixdirname[8][1024];	/* Full unix pathnames of the subdirs opened */
char dirblock[512];		/* Buffer to generate a prodos-format directory block in */
int dirblocknum;		/* and which directory block # is currently in the buffer */
unsigned short int dirfilecount[8];	/* Number of files files in a ProDOS dir */
FILE * log;

/* Translate a ProDOS filename to a Unix one */
unsigned char
translate_filename (short int location, char filename[])
{
  int x, y;
  /* Under prodos, all file names are in uppercase, and upper or lower case
     names interchangeably refer to the same file.  So we must convert all
     lower case filenames to upper case.  */
  /* High-Bit kludge: 
     Some prodos applications give pathnames with the high bit set.
     Apparently, prodos ignores the high bit.  To accomodate this behavior,
     we AND every character with 0x7F to strip the high bit.  */
  if ((MegaGetMem (location + 1) & 0x7f) == '/')
    {
      /* Absolute pathname */
      for (x = 0; prodosvolname[x] > 0 && x < MegaGetMem (location)
	   && (prodosvolname[x] ==
	       toupper (MegaGetMem (location + x + 1) & 0x7f)); x++)
	{
	}
      if (prodosvolname[x] > 0)
	{
	  return (0x45);	/* volume not found */
	}
      for (y = 0; prodosroot[y]; y++)
	{
	  filename[y] = prodosroot[y];
	}
      filename[y++] = '/';
      while (x < MegaGetMem (location))
	{
	  filename[y] = toupper (MegaGetMem (location + x + 1) & 0x7F);
	  x++;
	  y++;
	}
      filename[y] = 0;
    }
  else
    {
      /* Relative filename */
      getcwd (filename, 1024);
      x = MegaGetMem (location);
      y = strlen (filename);
      filename[y++] = '/';
      filename[x + y] = 0;
      while (x)
	{
	  filename[x + y - 1] = toupper (MegaGetMem (location + x) & 0x7f);
	  x--;
	}
    }
  return (0);
}

/* Create or update a .prodosdir file, which contains directory entries
   with prodos-specific filetype information in them */
/* First arg is the name of the directory (in the unix filesystem),
      or NULL for the current directory.
   Second arg is the file name.
   Third arg is the new file name (If the file is not to be renamed,
      the filenames should be the same.  If the file is being deleted,
      the new file name (and the next two feilds) should contain zeros.)
   Fourth arg is the prodos filetype (one byte)
   Fifth arg is the auxiliary filetype (2 bytes) */
/* Returns: 0 on success, -1 on error */
int
set_pinfo (char *dirname, char *filename, char *newfilename,
	   unsigned char filetype, unsigned short int auxtype)
{
  char dirfile[1024];
  int fd;
  unsigned char entry[128];
  int entrynumber = 0, match = 0;
  int firstempty = 0, emptyflag = 0;
  *dirfile = 0;
  if (dirname)
    {
      strcpy (dirfile, dirname);
      if (dirfile[0])
	strcat (dirfile, "/");
    }
  strcat (dirfile, ".prodosdir");
  fd = open (dirfile, O_RDWR | O_CREAT, 0666);	/* Open the file and create it if it doesn't exist */
  if (fd < 0)
    return (-1);
  flock (fd, LOCK_EX);		/* Lock the file while we update it */
  /* read the entries and look for a match */
  while (read (fd, entry, 128) > 0 && !match)
    {
      /* Keep note of where the first empty entry is,
         in case we are going to have to fill it. */
      if (!emptyflag)
	{
	  if (entry[0] == 0)
	    emptyflag = 1;
	  else
	    firstempty++;
	}
      if (strcmp (filename, entry) == 0)
	match = 1;
      else
	entrynumber++;
    }
  if (match)
    lseek (fd, entrynumber * 128, SEEK_SET);
  else
    lseek (fd, firstempty * 128, SEEK_SET);
  bzero (entry, 128);
  strncpy (entry, newfilename, 64);
  entry[65] = filetype;
  entry[66] = auxtype & 0x00FF;	/* Store the auxtype lsb-first */
  entry[67] = (auxtype & 0xFF00) >> 8;
  write (fd, entry, 128);	/* Write the new entry */
  flock (fd, LOCK_UN);		/* Unlock the file */
  close (fd);
  return (0);
}

/* Get an entry from the .prodosdir file */
/* Returns: 0 on success, -1 on error */
int
get_pinfo (char *dirname, char *filename, unsigned char *filetype,
	   unsigned short int *auxtype)
{
  char dirfile[1024];
  int fd;
  unsigned char entry[128];
  int match = 0;
  *dirfile = 0;
  if (dirname)
    {
      strcpy (dirfile, dirname);
      if (dirfile[0])
	strcat (dirfile, "/");
    }
  strcat (dirfile, ".prodosdir");
  fd = open (dirfile, O_RDONLY);	/* Open the file for reading */
  if (fd < 0)
    return (-1);
  /* read the entries and look for a match */
  while (!match)
    {
      if (read (fd, entry, 128) <= 0)
	{
	  close (fd);
	  return (-1);
	}
      if (strcmp (filename, entry) == 0)
	match = 1;
    }
  close (fd);
  *filetype = entry[65];
  *auxtype = (entry[66] | entry[67] << 8);
  return (0);
}

/* Split a complete pathname into its filename and directory components */
void
splitpath (char *pathname, char *dirname, char *filename)
{
  int a, b;
  a = strlen (pathname);
  while (a > 0 && pathname[a] != '/')
    a--;
  if (a > 0)
    {
      for (b = 0; pathname[a + b + 1] != 0; b++)
	filename[b] = pathname[a + b + 1];
      filename[b] = 0;
      for (b = 0; b < a; b++)
	dirname[b] = pathname[b];
      dirname[b] = 0;
    }
  else
    {
      dirname[0] = 0;
      strcpy (filename, pathname);
    }
}

/* Generate a prodos-format directory block */
int
generatedirblock (DIR * dirp, char dirname[], short int filecount)
{
  struct dirent *dirents;
  int entryloc;
  int a, b;
  char filename[1024];
  unsigned char filetype;
  short int auxtype;

  bzero (dirblock, 512);
  if (dirblocknum == 0)
    {
      if (strlen (dirname) <= (strlen (prodosroot) + 2))
	{
	  /* Root Directory */
	  strcpy (&dirblock[4], prodosvolname);
	  dirblock[4] = (strlen (prodosvolname) - 1) | 0xF0;
	  dirblock[0x22] = 0x23;	/* Access Bits */
	}
      else
	{
	  /* Subdirectory */
	  a = strlen (dirname);
	  a--;
	  while (dirname[a] == '/')
	    a--;
	  while (dirname[a] != '/')
	    a--;
	  a++;
	  b = 0;
	  while (dirname[a] != '/' && dirname[a] > 0)
	    {
	      dirblock[b + 5] = dirname[a];
	      b++;
	      a++;
	    }
	  dirblock[4] = b | 0xe0;
	  dirblock[0x14] = 0x75;	/* Subdirectory header magic number */
	}
      dirblock[0x23] = 0x27;	/* Entry Length */
      dirblock[0x24] = 0x0d;	/* Entries Per Block */
      dirblock[0x25] = filecount;
      /* dirblock[0x25] contains a count of the number of files in the
         directory.  Some programs actually check this, unfortunately. */
      entryloc = 0x2b;
    }
  else
    {
      entryloc = 0x04;
    }

  while (entryloc < 473)
    {
      if (dirents = readdir (dirp))
	{
	  if (strlen ((*dirents).d_name) <= 15)
	    {
	      for (a = 0;
		   (*dirents).d_name[a] >= 32 && (*dirents).d_name[a] < 'a';
		   a++)
		{
		}
	      if ((*dirents).d_name[a] == 0)
		{
		  if ((*dirents).d_name[0] != '.')
		    {
		      strcpy (&dirblock[entryloc + 1], (*dirents).d_name);
		      dirblock[entryloc] = strlen ((*dirents).d_name) | 0x10;
		      strcpy (filename, dirname);
		      strcat (filename, "/");
		      strcat (filename, (*dirents).d_name);
		      if (stat (filename, &statbuf) == 0)
			{
			  if (statbuf.st_mode & S_IFDIR)
			    {
			      dirblock[entryloc] =
				strlen ((*dirents).d_name) | 0xD0;
			      dirblock[entryloc + 0x10] = 0x0f;	/* Type DIR */
			    }
			  else
			    {
			      dirblock[entryloc] =
				strlen ((*dirents).d_name) | 0x10;
			      if (get_pinfo
				  (dirname, (*dirents).d_name, &filetype,
				   &auxtype))
				{
				  dirblock[entryloc + 0x10] = 0x04;	/* Default type is TXT */
				}
			      else
				{
				  dirblock[entryloc + 0x10] = filetype;
				  dirblock[entryloc + 0x1f] =
				    (auxtype & 0xFF);
				  dirblock[entryloc + 0x20] =
				    (auxtype & 0xFF00) >> 8;
				}
			    }
			  dirblock[entryloc + 0x13] = (statbuf.st_blocks & 0xFF);	/* Blocks used */
			  dirblock[entryloc + 0x14] =
			    (statbuf.st_blocks & 0xFF00) >> 8;
			  dirblock[entryloc + 0x15] = (statbuf.st_size & 0xFF);	/* Size in bytes */
			  dirblock[entryloc + 0x16] =
			    (statbuf.st_size & 0xFF00) >> 8;
			  dirblock[entryloc + 0x17] =
			    (statbuf.st_size & 0xFF0000) >> 16;
			}
		      entryloc += 39;
		    }
		}
	    }
	}
      else
	entryloc += 512;
    }
  if (entryloc >= 512)
    entryloc -= 512;
  if (entryloc > 4)
    {				/* If there is one or more entries, return the block */
      dirblocknum++;
      return (0);
    }
  else
    return (-1);		/* Otherwise return EOF */
}

/* Count the number of prodos files in a directory */
unsigned short int
countfiles (DIR * dirp)
{
  struct dirent *dirents;
  int a, b;
  unsigned char filecount = 0;

  while (dirents = readdir (dirp))
    {
      if (strlen ((*dirents).d_name) <= 15)
	{
	  for (a = 0;
	       (*dirents).d_name[a] >= 32 && (*dirents).d_name[a] < 'a'; a++)
	    {
	    }
	  if ((*dirents).d_name[a] == 0)
	    {
	      if ((*dirents).d_name[0] != '.')
		{
		  filecount++;
		}
	    }
	}
    }
  return (filecount);
}

/* The ProDOS MLI emulator */
int
mli ()
{
  register BYTE lo, hi;
  BYTE mlicode;
  BYTE x, y;
  unsigned short parmaddr;
  unsigned short location;
  unsigned short reqlen;
  unsigned short actlen;
  unsigned short auxtype;
  unsigned char prodosdiskblock[512];
  long fileposition;
  int brw, a, b;
  char filename[64], dirname[1024];
  char unixfilename2[1024], filename2[64];	/* used by rename */
      struct timeval tv;
      struct timezone tz;
      struct tm *l;

  lo = _pull_ ();
  hi = _pull_ ();
  PPC = 1 + lo + (hi << 8);
  mlicode = MegaGetMem (PPC++);
  parmaddr = MegaGetMem (PPC++) | (MegaGetMem (PPC++) << 8);
  /*printf("MLI Call: %x\n",mlicode); */
  /*printf("parmaddr: %x\n",parmaddr); */

  switch (mlicode)
    {

    case 0x80:
	A=0;
	break;
    case 0xc7:			/* Get Prefix */
      x = MegaGetMem (parmaddr++);
      if (x != 1)
	{
	  A = 4;
	  break;
	}
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      for (x = 0; prodospfx[x]; x++)
	{
	  MegaPutMem (location + x, prodospfx[x]);
	}
      A = 0;
      break;

    case 0xc6:			/* Set Prefix */
      x = MegaGetMem (parmaddr++);
      if (x != 1)
	{
	  A = 4;
	  break;
	}
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      if (MegaGetMem (location) > 0)
	{
	  A = translate_filename (location, unixfilename);
	  if (A)
	    break;
	  if (chdir (unixfilename) == 0)
	    {
	      if ((MegaGetMem (location + 1) & 0x7f) == '/')
		{		/* Absolute path */
		  for (x = 0; x <= MegaGetMem (location); x++)
		    {
		      prodospfx[x] = MegaGetMem (location + x);
		    }
		}
	      else
		{		/* Relative Path */
		  /* Check this for >63 overflows! */
		  for (x = 0; x < MegaGetMem (location); x++)
		    {
		      prodospfx[++prodospfx[0]] =
			MegaGetMem (location + x + 1);
		    }
		}
	      /* Prodos always adds a trailing / to pathnames */
	      if (prodospfx[*prodospfx] != '/' && *prodospfx < 63)
		{
		  prodospfx[++prodospfx[0]] = '/';
		}
	      A = 0;
	      break;
	    }
	  else
	    {
	      /* chdir didn't work */
	      A = 0x44;
	      break;		/* Bad Path */
	    }
	}
      else
	{
	  A = 0;
	  break;
	}

    case 0xc5:			/* Online volumes */
      x = MegaGetMem (parmaddr++);
      if (x != 2)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      /* We just return one online volume, in Slot 7, Drive 1 */
      if (x == 0)
	{			/* Caller asked for all online volumes */
	  for (y = 1; prodosvolname[y] > 0 && prodosvolname[y] != '/'; y++)
	    {
	      MegaPutMem (location + y, prodosvolname[y]);
	    }
	  y--;
	  MegaPutMem (location, y | 0x70);
	  MegaPutMem (location + 16, 0);
	  A = 0;
	  break;
	}
      else if (x == 0x70)
	{			/* Caller asked for S7D1 */
	  for (y = 1; prodosvolname[y] > 0 && prodosvolname[y] != '/'; y++)
	    {
	      MegaPutMem (location + y, prodosvolname[y]);
	    }
	  y--;
	  MegaPutMem (location, y | 0x70);
	  A = 0;
	  break;
	}
      else if (x == 0x20) {
		fprintf(stderr,"Serial 2\n");
	}
      else
	{
	  A = 0x28;		/* Device not connected */
	  break;
	}

    case 0xc8:			/* Open */
      x = MegaGetMem (parmaddr++);
      if (x != 3)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      A = translate_filename (location, unixfilename);
      if (A)
	break;
      /*printf("open:%s\n",unixfilename);sleep(2); */
      /* Stat the file and see what it is */
      if (stat (unixfilename, &statbuf) != 0)
	{
	  A = 0x46;		/* File Not Found */
	  break;
	}
      if (statbuf.st_mode & S_IFDIR)
	{
	  /* if it is a directory */
	  for (x = 0; prodosofstat[x] > 0 && x < 8; x++)
	    {
	    }
	  if (x == 8)
	    {
	      A = 0x42;		/* Too many open files */
	      break;
	    }
	  opendirp[x] = opendir (unixfilename);
	  if (opendirp[x] == NULL)
	    {
	      A = 0x27;		/* I/O ERROR ? */
	      break;
	    }
	  prodosofstat[x] = 3;
	  prodosofptr[x] = 0;
	  prodosofnewlinechar[x] = 0;
	  prodosofnewlinemask[x] = 0;
	  prodosofeof[x] = -1;	/* We don't know what it is yet, so set it to -1 */
	  dirfilecount[x] = countfiles (opendirp[x]);	/* Get the filecount */
	  strcpy (unixdirname[x], unixfilename);
	  parmaddr += 2;	/* We don't need to use the buffer space inside the
				   Apple's memory, so we just skip this pointer */
	  MegaPutMem (parmaddr, x + 1);	/* Return the fd to the application */
	  /* Prodos calls its fds "reference numbers" */
	  A = 0;
	}
      else
	{
	  /* not a directory */
	  for (x = 0; prodosofstat[x] > 0 && x < 8; x++)
	    {
	    }
	  if (x == 8)
	    {
	      A = 0x42;		/* Too many open files */
	      break;
	    }
	  /*printf("refnum: %d ",x); */
	  if ((prodosfd[x] = open (unixfilename, O_RDWR)) > 0)
	    {
	      prodosofstat[x] = 1;
	    }
	  else if ((prodosfd[x] = open (unixfilename, O_RDONLY)) > 0)
	    {
	      prodosofstat[x] = 2;
	    }
	  else
	    {
	      /*printf("%s:%d",unixfilename,errno); */
	      A = 0x27;		/* I/O ERROR */
	      break;		/* This will happen if permission is denied. */
	    }
	  prodosofnewlinechar[x] = 0;
	  prodosofnewlinemask[x] = 0;
	  parmaddr += 2;	/* We don't need to use the buffer space inside the
				   Apple's memory, so we just skip this pointer */
	  MegaPutMem (parmaddr, x + 1);	/* Return the fd to the application */
	  A = 0;
	}
      break;

    case 0xc9:			/* NEWLINE */
      /* Set newline char and and mask */
      /* See the read call for a description of what this does */
      x = MegaGetMem (parmaddr++);
      if (x != 3)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);	/* Get reference number */
      /* and make sure it's not bogus */
      if (x > 8 || x == 0)
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}
      else
	{
	  prodosofnewlinemask[x - 1] = MegaGetMem (parmaddr++);
	  prodosofnewlinechar[x - 1] = MegaGetMem (parmaddr++);
	  A = 0;
	  break;
	}

    case 0xca:			/* READ */
      x = MegaGetMem (parmaddr++);
      if (x != 4)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      reqlen = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      actlen = 0;
      /*printf("read: fd=%d loc: %x reqlen: %x ",x,location,reqlen); */
      if (x > 8)
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}
      else
	{
	  y = x - 1;
	  if (reqlen == 0 && prodosofstat[y] >= 1 && prodosofstat[y] <= 3)
	    {
	      /* Zero byte read kludge */
	      /* Apparently when Shrinkit unpacks an empty file, it issues a read
	         request for zero bytes.  This causes the routine below to return
	         an EOF, which causes ShrinkIt to abort.  So if we get a read
	         request for zero bytes, we just ignore it and return success */
	      MegaPutMem (parmaddr++, 0);	/* Return zero bytes actual length */
	      MegaPutMem (parmaddr++, 0);
	      A = 0;		/* No error */
	      break;
	    }
	  if (prodosofstat[y] == 1 || prodosofstat[y] == 2)
	    {
	      /* if the open file is not a directory */
	      if (prodosofnewlinemask[y] == 0)
		{
		  /* Read in the file in 512-byte blocks and copy them into
		     the emulator memory */
		  while (reqlen > 0)
		    {
		      /* Check for memory faults */
		      if (location > 0xBD00)
			if ((long) location + (long) reqlen > 0xBF00)
			  {
			    printf
			      ("\12\15\12\15Memory Fault in ProDOS application\12\15");
			    printf
			      ("Application terminated on MLI READ ($CA) call into $BF00-FFFF memory area.\12\15");
			    exit (1);
			  }
		      if (reqlen >= 512)
			{
			  brw = read (prodosfd[y], prodosdiskblock, 512);
			}
		      else
			{
			  brw = read (prodosfd[y], prodosdiskblock, reqlen);
			}
		      if (brw <= 0)
			{
			  reqlen = 0;
			}
		      else
			{
			  for (a = 0; a < brw; a++)
			    {
			      MegaPutMem (location++, prodosdiskblock[a]);
			    }
			  actlen += brw;
			  reqlen -= brw;
			}
		    }
		}
	      else
		{
		  /* line mode */
		  /* Prodos has two modes for reading files, binary mode (above),
		     and line-at-a-time mode.  In this mode, we read data up to
		     the newline character and then stop.  The newline character
		     is usually a CR, but can be anything. */
		  while (reqlen)
		    {
		      if (read (prodosfd[y], prodosdiskblock, 1))
			{
			  MegaPutMem (location++, *prodosdiskblock);
			  reqlen--;
			  actlen++;
			  if (((*prodosdiskblock) & prodosofnewlinemask[y]) ==
			      prodosofnewlinechar[y])
			    reqlen = 0;
			}
		      else
			reqlen = 0;
		    }
		}
	      if (actlen == 0)
		A = 0x4C;	/* EOF */
	      else
		A = 0;
	    }
	  else if (prodosofstat[y] == 3)
	    {
	      /* if we're reading a directory */
	      rewinddir (opendirp[y]);
	      dirblocknum = 0;
	      /* if the file position is not zero, we skip thru some blocks */
	      while (((dirblocknum * 512) + 511) < prodosofptr[y]
		     && generatedirblock (opendirp[y], unixdirname[y],
					  dirfilecount[y]) == 0)
		{
		  if (prodosofeof[y] < (dirblocknum + 1) * 512)
		    prodosofeof[y] = (dirblocknum + 1) * 512;
		}
	      while (generatedirblock
		     (opendirp[y], unixdirname[y], dirfilecount[y]) == 0
		     && reqlen > 0)
		{
		  if (prodosofeof[y] < (dirblocknum + 1) * 512)
		    prodosofeof[y] = (dirblocknum + 1) * 512;
		  a = prodosofptr[y] % 512;
		  while (a < 512 && reqlen > 0)
		    {
		      MegaPutMem (location++, dirblock[a]);
		      reqlen--;
		      a++;
		      actlen++;
		      prodosofptr[y]++;
		    }
		}
	      /* Finish out the last block */
	      a = prodosofptr[y] % 512;
	      while (a < 512 && reqlen > 0 && prodosofptr[y] < prodosofeof[y])
		{
		  MegaPutMem (location++, dirblock[a]);
		  reqlen--;
		  a++;
		  actlen++;
		  prodosofptr[y]++;
		}
	      if (actlen == 0)
		A = 0x4C;	/* EOF */
	      else
		A = 0;
	    }
	  else
	    {
	      A = 0x43;		/* Bad File Number */
	    }
	  /* Return the actual number of bytes read to the application */
	  MegaPutMem (parmaddr++, (actlen & 0xFF));
	  MegaPutMem (parmaddr++, (actlen & 0xFF00) >> 8);
	  /*printf("actlen:%d",actlen);/*sleep(2); */
	}
      break;

    case 0xcb:			/* Write */
      x = MegaGetMem (parmaddr++);
      if (x != 4)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      reqlen = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);

      if ((long) location + (long) reqlen > 0xC000)
	{
	  printf ("\n\nMemory Fault in ProDOS application\n");
	  printf
	    ("Application terminated on MLI WRITE ($CB) call out of range of main (48K) memory.\n");
	  exit (1);
	}

      y = x - 1;
      actlen = 0;
      if (x > 8)
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}
      else
	{
	  if (prodosofstat[y] == 1)
	    {
	      /* if the open file is not a directory and is opened for writing */
	      while (reqlen)
		{
		  /* Load data from the emulated apple memory and write it to disk */
		  if (reqlen >= 512)
		    {
		      for (a = 0; a < 512; a++)
			{
			  prodosdiskblock[a] = MegaGetMem (location++);
			}
		      brw = write (prodosfd[y], prodosdiskblock, 512);
		      actlen += brw;
		      reqlen -= brw;
		    }
		  else
		    {
		      for (a = 0; a < reqlen; a++)
			{
			  prodosdiskblock[a] = MegaGetMem (location++);
			}
		      brw = write (prodosfd[y], prodosdiskblock, reqlen);
		      actlen += brw;
		      reqlen -= brw;
		    }
		  /*printf("Wrote %d ",actlen);/*sleep(2); */
		  if (brw == 0)
		    {
		      reqlen = 0;
		    }
		  else
		    {
		    }
		  if (actlen == 0)
		    A = 0x27;	/* I/O ERROR (or possibly disk full?) */
		  else
		    A = 0;
		}
	    }
	  else if (prodosofstat[y] == 2 || prodosofstat[y] == 3)
	    {
	      A = 0x4E;		/* Write access denied */
	    }
	  else
	    {
	      A = 0x43;		/* Bad File Number */
	    }
	  /* Return the actual number of bytes written to the application */
	  MegaPutMem (parmaddr++, (actlen & 0xFF));
	  MegaPutMem (parmaddr++, (actlen & 0xFF00) >> 8);
	}
      break;

    case 0xcc:			/* Close */
      x = MegaGetMem (parmaddr++);
      if (x != 1)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);
      if (x > 8)
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}
      if (x == 0)
	{			/* Close all open files */
	  for (y = 0; y < 8; y++)
	    {
	      if (prodosofstat[y] == 1 || prodosofstat[y] == 2)
		{
		  close (prodosfd[y]);
		  prodosofstat[y] = 0;
		}
	      if (prodosofstat[y] == 3)
		{
		  closedir (opendirp[y]);
		  prodosofstat[y] = 0;
		}
	    }
	}
      else
	{			/* Close a single open file */
	  y = x - 1;
	  if (prodosofstat[y] == 1 || prodosofstat[y] == 2)
	    {
	      close (prodosfd[y]);
	      prodosofstat[y] = 0;
	    }
	  if (prodosofstat[y] == 3)
	    {
	      closedir (opendirp[y]);
	      prodosofstat[y] = 0;
	    }
	}
      A = 0;
      break;

    case 0xcd:			/* Flush */
      /* The kernel should be doing this automatically, 
         so we ignore this call */
      A = 0;
      break;

    case 0xce:			/* SET_MARK (lseek) */
      x = MegaGetMem (parmaddr++);
      if (x != 2)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);	/* get file reference number */
      if (x == 0 || x > 8)
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}
      fileposition = MegaGetMem (parmaddr++)
	| (MegaGetMem (parmaddr++) << 8) | (MegaGetMem (parmaddr++) << 16);
      /*printf("set_mark fd=%d pos=%d ",x,fileposition);/*sleep(2);/* */
      y = x - 1;
      if (prodosofstat[y] == 1 || prodosofstat[y] == 2)
	{
	  /* if the open file is not a directory */
	  a = lseek (prodosfd[y], fileposition, SEEK_SET);
	  /*printf("actual=%d ",a); */
	  A = 0;
	  break;
	}
      else if (prodosofstat[y] == 3)
	{
	  /* if the open file is a directory */
	  /* simulated directory entries are generated on the fly
	     when the directory is read, so here we only take note of
	     the position */
	  if (prodosofeof[y] > 0 && fileposition > prodosofeof[y])
	    {
	      A = 0x4D;		/* Position Past EOF */
	      break;
	    }
	  else
	    {
	      prodosofptr[y] = fileposition;
	      A = 0;
	      break;
	    }
	}
      else
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}

    case 0xcf:			/* GET_MARK (get file position) */
      x = MegaGetMem (parmaddr++);
      if (x != 2)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);	/* get file reference number */
      if (x == 0 || x > 8)
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}
      y = x - 1;
      if (prodosofstat[y] == 1 || prodosofstat[y] == 2)
	{
	  /* if the open file is not a directory */
	  fileposition = lseek (prodosfd[y], 0, SEEK_CUR);
	  /* printf("get_mark fd=%d pos=%d ",x,fileposition);/*sleep(2);/* */
	  if (fileposition < 0)
	    {
	      A = 0x43;		/*Bad fd */
	      break;
	    }
	  else
	    {
	      MegaPutMem (parmaddr++, (fileposition & 0xFF));
	      MegaPutMem (parmaddr++, (fileposition & 0xFF00) >> 8);
	      MegaPutMem (parmaddr++, (fileposition & 0xFF0000) >> 16);
	      A = 0;
	      break;
	    }
	}
      else if (prodosofstat[y] == 3)
	{
	  /* if the open file is a directory */
	  /*printf("getmark fd=%d %d ",x,prodosofptr[y]);/* */
	  MegaPutMem (parmaddr++, (prodosofptr[y] & 0xFF));
	  MegaPutMem (parmaddr++, (prodosofptr[y] & 0xFF00) >> 8);
	  MegaPutMem (parmaddr++, (prodosofptr[y] & 0xFF0000) >> 16);
	  A = 0;
	  break;
	}
      else
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}

    case 0xD1:			/* Get EOF */
      x = MegaGetMem (parmaddr++);
      if (x != 2)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);	/* get file reference number */
      if (x == 0 || x > 8)
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}
      y = x - 1;
      if (prodosofstat[y] == 1 || prodosofstat[y] == 2)
	{
	  a = fstat (prodosfd[y], &statbuf);
	  /*printf("EOF fd=%d %d(%d) ",x,statbuf.st_size,a); */
	  MegaPutMem (parmaddr++, (statbuf.st_size & 0xFF));
	  MegaPutMem (parmaddr++, (statbuf.st_size & 0xFF00) >> 8);
	  MegaPutMem (parmaddr++, (statbuf.st_size & 0xFF0000) >> 16);
	  A = 0;
	  break;
	}
      else
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}

    case 0xD0:			/* Set EOF */
      /* If the new eof is less than the old one, we truncate the file. */
      x = MegaGetMem (parmaddr++);
      if (x != 2)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      x = MegaGetMem (parmaddr++);	/* get file reference number */
      if (x == 0 || x > 8)
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}
      fileposition = MegaGetMem (parmaddr++)
	| (MegaGetMem (parmaddr++) << 8) | (MegaGetMem (parmaddr++) << 16);
      y = x - 1;
      if (prodosofstat[y] == 1)
	{
	  if (ftruncate (prodosfd[y], fileposition) == 0)
	    A = 0;
	  else
	    A = 0x27;		/* I/O ERROR or ??? */
	  break;
	}
      else if (prodosofstat[y] == 2)
	{
	  A = 0x4e;		/* Write Access Denied */
	  break;
	}
      else
	{
	  A = 0x43;		/* Bad File Number */
	  break;
	}

    case 0xc0:			/* Create */
      x = MegaGetMem (parmaddr++);
      if (x != 7)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      A = translate_filename (location, unixfilename);
      if (A)
	break;
      x = MegaGetMem (parmaddr++);	/* Access Bits */
      y = MegaGetMem (parmaddr++);	/* File Type */
      if (y != 0x0F)
	{
	  /* Not a directory */
	  a = open (unixfilename, O_WRONLY | O_CREAT | O_EXCL, 0666);
	  if (a > 0)
	    {
	      close (a);
	      auxtype =
		MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
	      splitpath (unixfilename, dirname, filename);
	      set_pinfo (dirname, filename, filename, y, auxtype);
	      A = 0;
	      break;
	    }
	  else
	    {
	      if (errno == EEXIST || errno == EISDIR)
		{
		  A = 0x47;	/* Duplicate File Name */
		  break;
		}
	      else if (errno == EACCES)
		{
		  A = 0x4e;	/* Access Denied */
		  break;
		}
	      else if (errno == ENOTDIR || errno == ENOENT || errno == ELOOP)
		{
		  A = 0x44;	/* Bad Path */
		  break;
		}
	      else if (errno == EROFS)
		{
		  A = 0x2B;	/* Disk is write protected */
		  break;
		}
	      else if (errno == ENOSPC)
		{
		  A = 0x48;	/* Disk full */
		  break;
		}
	      else
		{
		  A = 0x27;	/* If we get here, we don't have a clue what the error is,
				   so I/O ERROR is as good a description as any.  */
		  break;
		}
	    }
	}
      else
	{
	  /* Create a directory */
	  mkdir (unixfilename, 0777);	/* umask should take care of this */
	  A = 0;
	  break;
	}

    case 0xC1:			/* DESTROY (delete) */
      x = MegaGetMem (parmaddr++);
      if (x != 1)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      A = translate_filename (location, unixfilename);
      if (A)
	break;
      if (unlink (unixfilename) == 0)
	{
	  /* Also delete its .prodosdir entry if there is one */
	  splitpath (unixfilename, dirname, filename);
	  *prodosdiskblock = 0;
	  set_pinfo (dirname, filename, prodosdiskblock, 0, 0);
	  A = 0;
	  break;
	}
      else
	{
	  if (errno == EACCES)
	    {
	      A = 0x4e;		/* Access Denied */
	      break;
	    }
	  else if (errno == ENOTDIR || errno == ENOENT || errno == ELOOP)
	    {
	      A = 0x46;		/* File Not Found */
	      break;
	    }
	  else if (errno == EROFS)
	    {
	      A = 0x2B;		/* Disk is write protected */
	      break;
	    }
	  else
	    {
	      A = 0x27;		/* If we get here, we don't have a clue what the error is,
				   so I/O ERROR is as good a description as any.  */
	      break;
	    }
	}

    case 0xC2:			/* RENAME */
      x = MegaGetMem (parmaddr++);
      if (x != 2)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      A = translate_filename (location, unixfilename);
      if (A)
	break;
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      A = translate_filename (location, unixfilename2);
      if (A)
	break;
      if (rename (unixfilename, unixfilename2) == 0)
	{
	  /* Also rename its .prodosdir entry if there is one */
	  splitpath (unixfilename, dirname, filename);
	  if (get_pinfo (dirname, filename, &x, &auxtype) == 0)
	    {
	      splitpath (unixfilename2, dirname, filename2);
	      set_pinfo (dirname, filename, filename2, x, auxtype);
	    }
	  A = 0;
	  break;
	}
      else
	{
	  if (errno == EACCES || errno == EPERM)
	    {
	      A = 0x4e;		/* Access Denied */
	      break;
	    }
	  else if (errno == ENOTDIR || errno == ENOENT || errno == ELOOP)
	    {
	      A = 0x46;		/* File Not Found */
	      break;
	    }
	  else if (errno == EROFS)
	    {
	      A = 0x2B;		/* Disk is write protected */
	      break;
	    }
	  else
	    {
	      A = 0x27;		/* If we get here, we don't have a clue what the error is,
				   so I/O ERROR is as good a description as any.  */
	      break;
	    }
	}

/* The Access-Bits parameter to GET_FILE_INFO and SET_FILE_INFO is a
   byte which is bitmapped as follows: DNBxxxWR
    D (bit 7) - If set, file/directory may be destroyed
    N (bit 6) - If set, file/directory may be renamed
    B (bit 5) - Backup bit, set to one every time the file is modified
    W (bit 1) - If set, file may be written to
    R (bit 0) - If set, file may be read
  Setting the other bits results in a $4E (Access Denied) error under Prodos.
  None of this is currently supported by the emulator.
  The read and write bits could be set using chmod.  But what about other
  users?  Do we allow group/other to read the files?
  The delete/rename permissions also present a problem because these are
  usually set on a per-directory basis under unix, instead of for each file.
  And the backup bit?  We could check the date, or store it in .prodosdir
  Fortunately, very few applications check it, so maybe it doesn't matter.
*/

    case 0xc3:			/* SET_FILE_INFO */
      x = MegaGetMem (parmaddr++);
      if (x != 7)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      A = translate_filename (location, unixfilename);
      if (A)
	break;
      x = MegaGetMem (parmaddr++);	/* Access Bits */
      y = MegaGetMem (parmaddr++);	/* File Type */
      auxtype = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      splitpath (unixfilename, dirname, filename);
      if (set_pinfo (dirname, filename, filename, y, auxtype) == 0)
	{
	  A = 0;
	  break;
	}
      else
	{
	  A = 0x27;
	  break;
	}

    case 0xc4:			/* GET_FILE_INFO */
      x = MegaGetMem (parmaddr++);
      if (x != 10)
	{
	  A = 4;
	  break;		/* Bad Parameter Count */
	}
      location = MegaGetMem (parmaddr++) | (MegaGetMem (parmaddr++) << 8);
      A = translate_filename (location, unixfilename);
      if (A)
	break;
      /*printf("getfileinfo:%s\n",unixfilename);sleep(2);/* */
      /* Stat the file and see what it is */
      if (stat (unixfilename, &statbuf) != 0)
	{
	  A = 0x46;		/* File Not Found */
	  break;
	}
      MegaPutMem (parmaddr++, 0xC3);	/* stats, unlocked */
      if (statbuf.st_mode & S_IFDIR)
	{
	  MegaPutMem (parmaddr++, 0x0F);	/* Type DIR */
	  MegaPutMem (parmaddr++, 0);
	  MegaPutMem (parmaddr++, 0);	/* Auxtype=0000 */
	}
      else
	{
	  splitpath (unixfilename, dirname, filename);
	  if (get_pinfo (dirname, filename, &x, &auxtype))
	    {
	      /* No .prodosdir entry, return default type */
	      MegaPutMem (parmaddr++, 0x04);	/* Default Type is TXT */
	      MegaPutMem (parmaddr++, 0);
	      MegaPutMem (parmaddr++, 0);	/* Auxtype=0000 */
	    }
	  else
	    {
	      /* Return the info from the .prodosdir file */
	      MegaPutMem (parmaddr++, x);
	      MegaPutMem (parmaddr++, auxtype & 0xFF);
	      MegaPutMem (parmaddr++, (auxtype & 0xff00) >> 8);
	    }
	}
      /* Storage Type */
      if (statbuf.st_mode & S_IFDIR)
	{
	  MegaPutMem (parmaddr++, 0x0D);	/* $0D for directory */
	}
      else
	{
	  MegaPutMem (parmaddr++, 0x03);	/* I guess 3 is as good as one or two, it probably doesn't matter */
	}
      MegaPutMem (parmaddr++, 0);
      MegaPutMem (parmaddr++, 0);	/* Blocks Used */
      MegaPutMem (parmaddr++, 0);
      MegaPutMem (parmaddr++, 0);	/* Modification Date */
      MegaPutMem (parmaddr++, 0);
      MegaPutMem (parmaddr++, 0);	/* Modification Time */
      MegaPutMem (parmaddr++, 0);
      MegaPutMem (parmaddr++, 0);	/* Creation Date */
      MegaPutMem (parmaddr++, 0);
      MegaPutMem (parmaddr++, 0);	/* Creation Time */
      A = 0;
      break;

    case 0x65:			/* Quit */
      MegaShutDown ();
      exit (0);

    case 0xD2:			/* SET_BUF */
      A = 0;
      break;

    case 0xD3:			/* GET_BUF */
      A = 0;
      break;

/* GET_TIME gets the current date/time,
   stores the current date in $BF92-BF93, and
   stored the current time in $BF93-BF94.
   The date is formatted as (bitwise): YYYYYYYMMMMDDDDD
    where Y=year, M=month, and D=day
   The time is formatted as: HHHHHHHHMMMMMMMM
    where H=hour and M=minute.
   Prodos does not support seconds.
   If there is no clock in the system, these fields are set to all zeros.
   GET_TIME takes no parameters and always returns zero (no error)
*/
    case 0x82:			/* Get Time */
      tz.tz_minuteswest = 60;
      tz.tz_dsttime = 0;
      gettimeofday (&tv, &tz);
      l = localtime (&tv.tv_sec);
      MegaPutMem (0xbf90, l->tm_mday + (((l->tm_mon + 1) << 5) & 224));
      MegaPutMem (0xbf91,
		  (((l->tm_mon + 1) >> 3) + ((l->tm_year % 100) << 1)));
      MegaPutMem (0xbf92, l->tm_min);
      MegaPutMem (0xbf93, l->tm_hour);
      A = 0;			/* Not currently implemented */
      break;

    default:
      fprintf (log,"WARNING: Unknown MLI Call: %x\n", mlicode);
      A = 1;			/* Invalid MLI function */
      break;
    }

  /* Upon return, we set the carry if there is an error */
  _setC_ (A != 0);

  /* Some applications seem to check this too */
  /* They should only be checking the carry bit,
     but this helps to keep things compatible */
  _setZ_ (A == 0);

}

void
InitProdos ()
{
  int n;
  strcpy (prodosvolname, "/UNIX");	/* Should not have trailing / */
  if (!*prodosroot)
    getcwd (prodosroot, 1024);
  strcpy (&prodospfx[1], prodosvolname);
  prodospfx[0] = strlen (prodosvolname);
  prodospfx[++prodospfx[0]] = '/';
  for (n = 0; n < 8; n++)
    {
      prodosfd[n] = 0;
      prodosofstat[n] = 0;
    }
}

void
bf00 ()
{
  if (PPC == 0xbf00)
    mli ();
  else
    {
      printf ("\15\12Segmentation violation in Prodos Application\15\12");
      printf ("Call to unknown location in Prodos Global Page\15\12");
      printf ("Program counter at %x\15\12\15\12", PPC);
      /*MegaQuitDetect=1; */
      /*MegaShutDown(); */
      exit (1);
    }
}

/* Some ProDOS filetypes: 
  00     Typeless file
  01 BAD Bad blocks file
  04 TXT Text file 
  06 BIN Binary file
  0F DIR Subdirectory
  19 ADB Appleworks Database file
  1A AWP Appleworks Word Processor file
  1B ASP Appleworks Spread Sheet file
  E0     NuFX (Shrinkit) file
  EF PAS Apple Pascal
  F0 CMD Added command
  F1-F8  User defined filetypes
  FA INT Integer basic program file
  FB IVR Integer basic variables file
  FC BAS Applesoft Basic program
  FD VAR Applesoft Basic variables file
  FE REL relocatable object code file
  FF SYS Prodos system program file
*/

/* Summary of ProDOS Error Codes:

  00 - No Error
  01 - Invalid MLI function code number
  04 - Incorrect parameter count
  25 - Interrupt table full (Interrupts are not supported in this version)
  27 - I/O Error
         Prodos returns this error when it gets a bad disk block, among other
         things.
  28 - No Device Connected
         The application requested a non-existant device number
  2B - Write Protected
  2E - Disk switched
         The emulator never returns this error.
         Under Unix, (and ProDOS) switching the disk unexpectedly is a good
         way to mess up your filesystem!
  2F - No disk in drive
         The emulator never returns this error.
  40 - Invalid Pathname Syntax
  42 - Too many files open
         Prodos has a limit of 8 open files
  43 - Bad reference number
  44 - Bad Pathname
         One of the components of the pathname is not a directory
  45 - Volume not mounted
  46 - File not found
  47 - File already exists (Duplicate File Name)
         Cannot perform a CREATE or RENAME on a file which already exists
  48 - Disk full
  49 - Directory full
         Under Prodos, there is a limit to the number of files in a directory.
         The emulator does not have this limitation and never returns this
         error.
  4A - Incompatible ProDOS version
         The emulator does not return this error
  4B - Unsupported storage type
         The filename given is not a regular file
         The emulator does not return this error, but probably should for
         sockets, pipes, and other things a ProDOS application can't deal
         with.
  4C - End of File
         Attempted to read past EOF.
  4D - Position past EOF
         Attempted to seek a position past the EOF.
  4E - Access denied
  50 - File already open
         Prodos does not allow multiple opens for writing, or to delete or
         rename an open file.  Unix does, so the emulator never returns this
         error.
  51 - File count bad (directory structure corrupted)
         This error is specific to the ProDOS disk structure.
         The emulator never returns this error.
  52 - Not a Prodos disk
         Prodos gives this error if it does not recognize the disk format.
         The emulator never returns this error.
  53 - Parameter out of range
         The emulator does not return this error, but probably should.
  55 - Too many devices mounted
         This error message is obsolete and is never returned by newer
         versions of ProDOS.
  56 - Bad Buffer Address (Memory Fault)
         Attempted to allocate a buffer in a reserved area of memory
         The emulator doesn't check buffer addresses because it doesn't
         use them, and so never returns this error.
  57 - Duplicate volume name
         Attempted to mount two volumes with the same name.  The emulator
         does not return this error because the current version only mounts
         one volume, /UNIX on slot 7, drive 1.
  5A - Damaged disk free space bit map (filesystem corrupted)
         This error is specific to the ProDOS disk structure.
         The emulator never returns this error.
 */
