/*
 *mega2.c -- Apple ][ soft switches->UNIX emulation for Apple ][ Emulator
 *
 * Modified 4/20/1990 Randy Frank randy@tessa.iaf.uiowa.edu
 *
 *  DOS 3.3 added and ProDOS emulation patched.
 *  Reshaped memory and added //e emulation.
 *  Emulated a bunch of new softswitches.
 *  Support for 80col card added.
 *  Support for s5 drives added.
 *  Ctrl-A hook enhanced.
 *
 *  Known bugs:
 *       Paddles not emulated currently.
 *       Several switches emulated but ignored.
 *       The 16k ram card works but is not quite right...
 *       
 */

/*
  APPLE2E COPYRIGHT (C) 1990, RANDY FRANK

  RANDY  FRANK ("AUTHOR") GRANTS TO THE PARTY  HEREBY  RECEIVING "APPLE2E"
  FROM  AN AUTHORIZED PROVIDER ("RECIPIENT") A NON-EXCLUSIVE, ROYALTY-FREE
  LICENSE TO  COPY, DISPLAY, DISTRIBUTE, AND PRODUCE  DERIVATIVE  WORKS OF
  "APPLE2E",  PROVIDED  THAT  THE ABOVE  COPYRIGHT NOTICE APPEARS  IN  ALL
  COPIES MADE  OF "APPLE2E" AND BOTH THE COPYRIGHT NOTICE AND THIS LICENSE
  APPEAR IN SUPPORTING DOCUMENTATION, AND THAT  THE NAME OF  AUTHOR NOT BE
  USED IN  ADVERTISING OR PUBLICITY PERTAINING  TO "APPLE2E".   THE AUTHOR
  REQUESTS TO BE MADE AWARE OF ANY DERIVATIVE WORKS.

  "APPLE2E"  IS  PROVIDED  "AS  IS" WITHOUT  EXPRESS OR IMPLIED  WARRANTY.
  AUTHOR DOES  NOT AND CANNOT  WARRANT THE PERFORMANCE OF "APPLE2E" OR THE
  RESULTS THAT MAY BE OBTAINED BY ITS USE OR ITS  FITNESS FOR ANY SPECIFIC
  USE  BY RECIPIENT  OR ANY THIRD PARTY.  IN NO EVENT SHALL AUTHOR  BECOME
  LIABLE  TO  RECIPIENT  OR  ANY  OTHER  PARTY, FOR ANY  LOSS  OR DAMAGES,
  CONSEQUENTIAL  OR OTHERWISE,  INCLUDING  BUT NOT LIMITED TO TIME, MONEY,
  OR  GOODWILL,  ARISING  FROM USE OR  POSSESSION, AUTHORIZED OR  NOT,  OF
  "APPLE2E" BY RECIPIENT OR ANY OTHER PARTY.
*/

#include "apple.h"
#include <curses.h>
#include <execinfo.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifdef NOBEEP
#define beep()   {}
#endif
/*
 * Emulation globals:
 */
BYTE AMemory[65536];		/* Aux memory */
BYTE MMemory[65536];		/* Main memory                         */
BYTE Rom[16384];		/* 16K ROM bank of Main memory c0000-ffff */
BYTE RamRead = 0;		/* set if 16K RAM readable             */
BYTE RamWrite = 0;		/* set if 16K RAM writeable            */
BYTE Bank2Enable = 0;		/* set if bank 2 Ram enabled           */

BYTE MegaRand = 0;		/* Always contains 8-bit random number */
BYTE MegaLastKey = 0;		/* $C00X keyboard latch value          */
BYTE MegaQuitDetect = 0;	/* Set if user requests to quit        */
BYTE MegaSerial = 0;		/* Set if input from serial      */
BYTE MegaSerialOut = 0;		/* Set if output to serial      */

int BeepNoise = 1;

/* variables for //e emulation */
int slotcxROM;
int slotc3ROM;
int RAMRD;
int RAMWRT;
int STORE80;
int PAGE2;
int HIRES;
int ALTZP;
int DHGR;
int ALTCHAR;
int TEXT;
int MIXED;
int BUTTON0;
int BUTTON1;
int VID80;

int PDL0;			/* counters for the paddles */
int PDL1;
float paddle0;
float paddle1;
/*
   Switches emulated for //e operation...
   write c000 STORE80 off
   write c001 STORE80 on
   write c002 RAMRD off
   write c003 RAMRD on
   write c004 RAMWRT off
   write c005 RAMWRT on
   write c006 slotcxROM off = 0
   write c007 slotcxROM on  internalrom  c100-cfff
   write c008 ALTZP off
   write c009 ALTZP on 
   write c00a slotc3ROM off  internalC3rom
   write c00b slotc3ROM on 
   write c00c for VID80 off
   write c00d for VID80 on
   write c00e for ALTCHAR off
   write c00f for ALTCHAR on
         c010 clear keyboard strobe
         c011 and c012 are 16k ram card status
   read  c013 for RAMRD status
   read  c014 for RAMWRT status
   read  c015 slotcxrom status 
   read  c016 for ALTZP status
   read  c017 slotc3rom status
   read  c018 for STORE80 status
         c019 is the VBL switch (use random numbers)
   read  c01a for TEXT status
   read  c01b for MIXED status
   read  c01c for PAGE2 status
   read  c01d for HIRES status
   read  c01e for ALTCHAR status
   read  c01f for VID80 status 
   R/W   c050 for TEXT off  (graphics on)
   R/W   c051 for TEXT on  
   R/W   c052 for MIXED off (only text or only graphics)
   R/W   c053 for MIXED on
   R/W   c054 for PAGE2 off 
   R/W   c055 for PAGE2 on
   R/W   c056 for HIRES off
   R/W   c057 for HIRES on
   R/W   c05e for DHGR on
   R/W   c05f for DHGR off
   read  c061 for button0
   read  c062 for button1
   read  c07f for DHGR status
*/

void PutC000 ();
void PutC010 ();
void PutC05670 ();
void Put0400 ();
void PutC080 ();
void ChangeVID ();
BYTE GetC080 ();
BYTE GetC000 ();
BYTE GetC200 ();
BYTE GetC010 ();
BYTE GetC030 ();
BYTE GetC05670 ();

void ProInit ();
void ProFormat ();
void ProRead ();
void ProWrite ();
void ProStatus ();

void Dos33init ();
char *ip;

/*
 * Base address table for 24 lines of text/lores page 1 ($400..$7F8);
 * 40 bytes for each line. Note that screen "holes" exist:
 */

int LBasCalc[24] = {
  1024,
  1152,
  1280,
  1408,
  1536,
  1664,
  1792,
  1920,
  1064,
  1192,
  1320,
  1448,
  1576,
  1704,
  1832,
  1960,
  1104,
  1232,
  1360,
  1488,
  1616,
  1744,
  1872,
  2000
};

#define MegaPutChar(c) addch(c)

void
Rsoftswitches ()
{
  slotcxROM = 0;		/* should be 0 */
  slotc3ROM = 0;		/* essentially ignored as there is no slot3 card */
  RAMRD = 0;
  RAMWRT = 0;
  STORE80 = 0;
  PAGE2 = 0;
  HIRES = 0;
  ALTZP = 0;
  DHGR = 0;
  ALTCHAR = 0;
  TEXT = 1;
  MIXED = 0;
  BUTTON0 = 0;
  BUTTON1 = 0;
  VID80 = 0;
/* record any changes in the state table */
  (void) setupstates ();
}

/**************************************************************************/
/* Dispatch routines: The MegaPutMem and MegaGetMem, below, have default  */
/* behavior for I/O space.  They may also contain calls to other routines */
/* in this file which handle Apple II behavior.  If these latter routines */
/* need initialization/shutdown, that code should be placed in the two    */
/* MegaStartUp and MegaShutDown routines, below.                          */
/**************************************************************************/

#ifdef NEVER
void
MegaPutChar (c)
     char c;			/* This makes a function out of the "putchar" macro */

{
  addch (c);
}
#endif

/* This routine is called at emulation startup time.  All initialization
   stuff, like setting terminal modes, opening files, etc. goes here. */
void
MegaStartUp ()
{
  register int i;		/* Iterator */

  /* init the pseudo disk */
/*  ProInit();*/
/*  Dos33init();*/

  speedinit ();

  /* Set input modes on terminal: */
  noecho ();
  raw ();

  /* Fill 40-column screen with simulated initial memory pattern: */
  clear ();
/*
  for (i = 0;   i <= 23;   i++)
    mvaddstr (i, 0, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
  refresh();
/*

  /* Initialize some variables: */
  MegaQuitDetect = 0;
  MegaLastKey = 0;
  MegaRand = 0;
  MegaSerial = 0;
}



/* This routine is called at emulation shutdown time.  All things
   started in MegaStartUp, above, should be cleaned up here. */
void
MegaShutDown ()
{
  /* Clear screen: */
  clear ();
  refresh ();

  /* Reset sane modes on terminal: */
  echo ();
  noraw ();
  endwin ();

  fprintf(log,"Stop %s\n",rcsid);
  fflush(log);
}

/* This routine handles ALL stores to the 64K address space, in order to
   faciliate special Apple ][ - specific effects.  Not all behaviors may
   be implemented; to install a new one, install the call in this routine.
   Returns: Nothing. */
void
MegaPutMem (addr, byte)
     register ADDR addr;
     register BYTE byte;
{
  register int rbank;		/* temp var */

  /* Make sure these are in range: */
  addr &= 0xffff;
  byte &= 0xff;

  /* Do random number generation: */
  MegaRand = (MegaRand + addr + byte) & 0xff;

/* where and how */
  switch (modetable[area[addr >> 8]][writestate])
    {
    case 0:
      MMemory[addr] = byte;
      rbank = 0;
      if (area[addr >> 8] == 2)
	Put0400 (addr, byte, rbank);
      return;
      break;
    case 1:
      AMemory[addr] = byte;
      rbank = 1;
      if (area[addr >> 8] == 2)
	Put0400 (addr, byte, rbank);
      return;
      break;
    case 2:
      MMemory[addr - (0x1000 * Bank2Enable)] = byte;
      return;
      break;
    case 3:
      AMemory[addr - (0x1000 * Bank2Enable)] = byte;
      return;
      break;
    case 4:
      /* writing to ROM so NOP */
      return;
      break;
    case 5:
      /* handle c000 - cffff */
      if ((addr >= 0xc010) && (addr <= 0xc01f))	/* soft switches */
	PutC010 (addr, byte);
      else if ((addr >= 0xc000) && (addr <= 0xc00f))	/* soft switches */
	PutC000 (addr, byte);
      else if ((addr >= 0xc050) && (addr <= 0xc07f))
	PutC05670 (addr, byte);
      else if (addr >= 0xc080 && addr < 0xc090)
	PutC080 (addr, byte);
/* record any changes in the state table */
      (void) setupstates ();
      return;
      break;
    }
}


/* This routine handles ALL fetches from the 64K address space, in order to
   facilitate special Apple ][ - specific effects.  Not all behaviors may
   be implemented; to install a new one, install the call in this routine.
   Returns: Value at location (could be random if I/O, etc.). */
BYTE
MegaGetMem (addr)
     register ADDR addr;
{
  register BYTE data;		/* Data from memory space to be returned */
  register int rbank;		/* temp var */

  /* Make sure we're in range: */
  addr &= 0xffff;

/* where and how */
  switch (modetable[area[addr >> 8]][readstate])
    {
    case 0:
      return (MMemory[addr]);
      break;
    case 1:
      return (AMemory[addr]);
      break;
    case 2:
      return (MMemory[addr - (0x1000 * Bank2Enable)]);
      break;
    case 3:
      return (AMemory[addr - (0x1000 * Bank2Enable)]);
      break;
    case 4:
      return (Rom[addr - 0xc000]);
      break;
    case 5:
      /* handle c000 - cffff */
      if ((addr >= 0xc800) && (addr <= 0xcfff))	/* rom read c800-cfff */
	return (Rom[addr - 0xc000]);
      else if ((addr >= 0xc100) && (addr <= 0xc7ff) && (slotcxROM == 1))
	return (Rom[addr - 0xc000]);
      else if ((addr >= 0xc300) && (addr <= 0xc3ff))	/*  && (slotc3ROM == 0) */
	return (Rom[addr - 0xc000]);

      /* We must be in C0x0 space.  Default to random value: */
      data = MegaRand & 0xff;
      MegaRand = (MegaRand + addr * 25 + data) & 0xff;

      /* Now do the appropriate memory-mapped INPUT functions, if any: */
      if ((addr >= 0xc000) && (addr <= 0xc00f))
	data = GetC000 (addr);
      else if ((addr >= 0xc010) && (addr <= 0xc01f))
	data = GetC010 (addr);
      else if ((addr >= 0xc030) && (addr <= 0xc03f))
	data = GetC030 (addr);
      else if ((addr >= 0xc050) && (addr <= 0xc07f))
	data = GetC05670 (addr);
      else if ((addr >= 0xc080) && (addr <= 0xc08f))
	data = GetC080 (addr);
/*
  		else if ((addr == 0xc0fd) && (dosver == 2)) {
    			(void) doclock();
    			data = 0x60;
        		}
  		else if (addr == 0xc0fe) {
    			(void) dounix();
    			data = 0x60;
        		}
  		else if (addr == 0xc701)
    			data = 0x20;
  		else if (addr == 0xc703)
    			data = 0x00;
  		else if (addr == 0xc705)
    			data = 0x03;
  		else if (addr == 0xc707)
    			data = 0x3c;
  		else if (addr == 0xc7ff)
    			data = 0x80;
  		else if (addr == 0xc7fe)
    			data = 0xdf;
  		else if (addr == 0xc7fc)
    			data = 0x00;
  		else if (addr == 0xc7fd)
    			data = 0x00;
  		else if (addr == 0xc780)
    			data = 0x60;
  		else if (addr == 0xc501)
    			data = 0x20;
  		else if (addr == 0xc503)
    			data = 0x00;
  		else if (addr == 0xc505)
    			data = 0x03;
  		else if (addr == 0xc507)
    			data = 0x3c;
  		else if (addr == 0xc5ff)
    			data = 0x80;
  		else if (addr == 0xc5fe)
    			data = 0xdf;
  		else if (addr == 0xc5fc)
    			data = 0x00;
  		else if (addr == 0xc5fd)
    			data = 0x00;
  		else if (addr == 0xc580)
    			data = 0x60;
*/
/* note: make s6 look like a card so PR#6 works under pdos */
      else if ((addr >= 0xc600) && (addr <= 0xc6ff))
	data = 0x60;
      else if ((addr >= 0xc200) && (addr <= 0xc2ff))
	data = GetC200 (addr);
      else if (addr == 0xc0a8)
	data = GetC200 (addr);
/* record any changes in the state table */
      (void) setupstates ();
/* Return the data we came up with to the user: */
      return (BYTE) (data & 0xff);
      break;
    }
}


/**************************************************************************/
/* Memory-Mapped I/O routines: These routines actually perform the spe-   */
/* cific UNIX operations for get/put to certain addresses.  Not all Get   */
/* or Put routines must necessarily have a companion Put or Get routine:  */
/*                                                                        */
/*     * GetXXXXXX: Return a byte of data from an I/O address             */
/*     * PutXXXXXX: Receive a byte of data for an I/O address             */
/*                                                                        */
/**************************************************************************/

int CAcount;
int sockfd;
struct sockaddr_in server_addr;
int port = 1977;

BYTE
GetC200 (addr)
     ADDR addr;
{
  struct sockaddr_in client_addr;
  char buffer[1024];
  socklen_t addr_size;
  int n;

  bzero (buffer, 1024);
  addr_size = sizeof (client_addr);
  n =
    recvfrom (sockfd, buffer, 1024, MSG_DONTWAIT,
	      (struct sockaddr *) &client_addr, &addr_size);
/*  fprintf (stderr, "[+]Data recv: %d %s\n\r", n, buffer); */

  if (n>1) {
	fprintf(log,"recv to big\n");
	fflush(log);
}
  if (buffer[0])
    {
      fprintf (stderr, "%x\n", buffer[0]);
      return (BYTE) (buffer[0] & 128);
    }
  else
    return (BYTE) (MegaRand & 0xff);

}

/* Get keyboard data in low 7 bits. Msb indicates if hit since last C010: */
BYTE
GetC000 (addr)
     ADDR addr;
{
  register int fflags;
  char data = '*';
      struct sockaddr_in client_addr;
      char buffer[1024];
      socklen_t addr_size;
      int n;

  switch (addr & 0x000F)
    {
    case 0x00:
      /* Set nonblocking input: */
      fflags = fcntl (0, F_GETFL, 0);
      (void) fcntl (0, F_SETFL, fflags | O_NDELAY);
      /* See if a key was pressed.  If yes, update and set hi bit; */
      /* else, leave keyboard latch with last value: */
      if (read (0, &data, 1) > 0)
	MegaLastKey = (int) data | 0x80;

      /* Reset nonblocking input: */
      (void) fcntl (0, F_SETFL, fflags);

      if (MegaLastKey >= 0x80)
	{
	  if ((CAcount == 0) && (data == MEGAQUITKEY))	/* a first ctrl-A */
	    {
	      MegaLastKey = (int) data;
	      CAcount = 1;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == MEGAQUITKEY))	/* two ctrl-As */
	    {
	      CAcount = 0;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == 0x30))
	    {
	      BUTTON0 = 1 - BUTTON0;
	      CAcount = 0;
	      MegaLastKey = (int) data;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == 0x31))
	    {
	      BUTTON1 = 1 - BUTTON1;
	      CAcount = 0;
	      MegaLastKey = (int) data;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == 0x62))
	    {
	      CAcount = 0;
	      void *callstack[128];
	      int i, frames = backtrace (callstack, 128);
	      char **strs = backtrace_symbols (callstack, frames);
	      for (i = 0; i < frames; ++i)
		{
		  fprintf (log,"%s\n\r", strs[i]);
		}
	      free (strs);
	      fflush(log);
	      MegaLastKey = (int) data;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == 0x63))
	    {
	      MegaSerial = 1 - MegaSerial;
	      if (MegaSerial)
		{
		  int n;
		  sockfd = socket (AF_INET, SOCK_DGRAM, 0);
		  if (sockfd < 0)
		    {
		      perror ("[-]socket error");
		      exit (1);
		    }
		  memset (&server_addr, '\0', sizeof (server_addr));
		  server_addr.sin_family = AF_INET;
		  server_addr.sin_port = htons (port);
		  server_addr.sin_addr.s_addr = inet_addr (ip);

		  n =
		    bind (sockfd, (struct sockaddr *) &server_addr,
			  sizeof (server_addr));
		  if (n < 0)
		    {
		      perror ("[-]bind error");
		      exit (1);
		    }
		  fprintf (log, "Bound to sock %d\n",sockfd);
		  fflush(log);
		}
	      else
		{
#ifdef DEBUG
		  fprintf (stderr, "Unbound\n\r");
#endif
		}
	      CAcount = 0;
	      MegaLastKey = (int) data;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == 0x64))
	    {
	      CAcount = 0;
	      DebugSingle = 1;
	      MegaLastKey = (int) data;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == 0x65))
	    {
	      CAcount = 0;
	      MegaSerialOut = 1 - MegaSerialOut;
	      if (MegaSerialOut)
		{
		  sockfd = socket (AF_INET, SOCK_DGRAM, 0);
		  if (sockfd < 0)
		    {
		      perror ("[-]socket error");
		      exit (1);
		    }
		}
	      MegaLastKey = (int) data;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == 0x71))
	    {
	      CAcount = 0;
	      MegaQuitDetect = 1;
	      MegaLastKey = (int) data;
	      return (BYTE) MegaLastKey;
	    }
	  if ((CAcount == 1) && (data == 0x73))
	    {
	      CAcount = 0;
	      MegaLastKey = (int) 0x93;	/* ctrl-a s -> crtl-s */
	      return (BYTE) MegaLastKey;
	    }
	  CAcount = 0;
	}
      return (BYTE) MegaLastKey;
      break;
    case 0x8:			/* serial */

      bzero (buffer, 1024);
      addr_size = sizeof (client_addr);
      n =
	recvfrom (sockfd, buffer, 1024, MSG_DONTWAIT,
		  (struct sockaddr *) &client_addr, &addr_size);
#ifdef DEBUG
      if (n != -1)
	fprintf (stderr, "[+]Data recv: %d %s\n\r", n, buffer);
#endif
	if (n>1) {
		fprintf(log,"recvfrom too big\n");
		fflush(log);
	}
      if (buffer[0])
	{
	  MegaLastKey = buffer[0] | 128;
#ifdef DEBUG
	  fprintf (stderr, "%x\n", buffer[0]);
#endif
	  return (BYTE) MegaLastKey;
	}
      else
	return (BYTE) (MegaRand & 0xff);

      break;

    }
  return (BYTE) (MegaRand & 0xff);
}


/* Clear keyboard strobe for C000 msb: */
BYTE
GetC010 (addr)
     ADDR addr;
{
  switch (addr & 0x001F)
    {
    case 0x10:
      /* keyboard strobe */
      MegaLastKey &= 0x7f;	/* Clear strobe bit */
      return (BYTE) (MegaRand & 0xff);
      break;
    case 0x11:
      /*  which bank enabled */
      return (BYTE) (0xFF * Bank2Enable);
      break;
    case 0x12:
      /* 16k write enabled? */
      return (BYTE) (0xFF * RamRead);
      break;
    case 0x13:
      return (BYTE) (0xFF * RAMRD);
      break;
    case 0x14:
      return (BYTE) (0xFF * RAMWRT);
      break;
    case 0x15:
      /* Cxrom */
      return (BYTE) (0xFF * slotcxROM);
      break;
    case 0x16:
      return (BYTE) (0xFF * ALTZP);
      break;
    case 0x17:
      /* C3rom */
      return (BYTE) (0xFF * slotc3ROM);
      break;
    case 0x18:
      return (BYTE) (0xFF * STORE80);
      break;
/* c019 is the VBL line and a random num does just fine */
    case 0x1a:
      /* text */
      return (BYTE) (0xFF * TEXT);
      break;
    case 0x1b:
      /* mixed */
      return (BYTE) (0xFF * MIXED);
      break;
    case 0x1c:
      /* page2 */
      return (BYTE) (0xFF * PAGE2);
      break;
    case 0x1d:
      /* hires */
      return (BYTE) (0xFF * HIRES);
      break;
    case 0x1e:
      return (BYTE) (0xFF * ALTCHAR);
      break;
    case 0x1f:
      /* vid80 */
      return (BYTE) (0xFF * VID80);
      break;
    }
  return (BYTE) (MegaRand & 0xff);
}

BYTE
GetC080 (addr)
     ADDR addr;
{
  switch (addr & 0x000F)
    {
    case 0x00:
      RamRead = 1;
      RamWrite = 0;
      Bank2Enable = 1;
      break;
    case 0x01:
      RamRead = 0;
      RamWrite = 1;
      Bank2Enable = 1;
      break;
    case 0x02:
      RamRead = 0;
      RamWrite = 0;
      Bank2Enable = 1;
      break;
    case 0x03:
      RamRead = 1;
      RamWrite = 1;
      Bank2Enable = 1;
      break;
    case 0x08:
      RamRead = 1;
      RamWrite = 0;
      Bank2Enable = 0;
      break;
    case 0x09:
      RamRead = 0;
      RamWrite = 1;
      Bank2Enable = 0;
      break;
    case 0x0a:
      RamRead = 0;
      RamWrite = 0;
      Bank2Enable = 0;
      break;
    case 0x0b:
      RamRead = 1;
      RamWrite = 1;
      Bank2Enable = 0;
      break;
    }
  return (BYTE) (MegaRand & 0xff);
}

 /*ARGSUSED*/ void
PutC010 (addr, data)
     ADDR addr;
     BYTE data;
{
  (void) GetC010 (addr);	/* Same thing; either one works */
}

void
PutC05670 (addr, data)
     ADDR addr;
     BYTE data;
{
  (void) GetC05670 (addr);	/* for c050 - c07f */
/* for these switches writes do nothing or flip the switch like the read
   so this wrapper is appropriate here */
}

BYTE
GetC05670 (addr)
     ADDR addr;
{
  switch (addr & 0x007F)
    {
    case 0x50:
      TEXT = 0;
      break;
    case 0x51:
      TEXT = 1;
      break;
    case 0x52:
      MIXED = 0;
      break;
    case 0x53:
      MIXED = 1;
      break;
    case 0x54:
      PAGE2 = 0;
      break;
    case 0x55:
      PAGE2 = 1;
      break;
    case 0x56:
      HIRES = 0;
      break;
    case 0x57:
      HIRES = 1;
      break;
    case 0x5e:
      DHGR = 1;
      break;
    case 0x5f:
      DHGR = 0;
      break;
    case 0x61:
      return (BYTE) (0xFF * BUTTON0);
      break;
    case 0x62:
      return (BYTE) (0xFF * BUTTON1);
      break;
    case 0x64:
      if (PDL0 > 0)
	{
	  return (BYTE) (0xff);
	}
      else
	{
	  return (BYTE) (0x00);
	};
      break;
    case 0x65:
      if (PDL1 > 0)
	{
	  return (BYTE) (0xff);
	}
      else
	{
	  return (BYTE) (0x00);
	};
      break;
    case 0x70:
      PDL0 = paddle0 * 2830;
      PDL1 = paddle1 * 2830;
      break;
    case 0x7f:
      return (BYTE) (0xFF * DHGR);
      break;
    }
  return (BYTE) (MegaRand & 0xff);
}

void
PutC000 (addr, data)
     ADDR addr;
     BYTE data;
{
  switch (addr & 0x007F)
    {
    case 0x00:
      STORE80 = 0;
      break;
    case 0x01:
      STORE80 = 1;
      break;
    case 0x02:
      RAMRD = 0;
      break;
    case 0x03:
      RAMRD = 1;
      break;
    case 0x04:
      RAMWRT = 0;
      break;
    case 0x05:
      RAMWRT = 1;
      break;
    case 0x06:
      slotcxROM = 0;
      break;
    case 0x07:
      slotcxROM = 1;
      break;
    case 0x08:
      ALTZP = 0;
      break;
    case 0x09:
      ALTZP = 1;
      break;
    case 0x0a:
      slotc3ROM = 0;
      break;
    case 0x0b:
      slotc3ROM = 1;
      break;
    case 0x0c:
      if (VID80 == 0)
	break;			/* don't bother if it does nothing */
      VID80 = 0;
      ChangeVID ();
      break;
    case 0x0d:
      if (VID80 == 1)
	break;			/* don't bother if it does nothing */
      VID80 = 1;
      ChangeVID ();
      break;
    case 0x0e:
      ALTCHAR = 0;
      break;
    case 0x0f:
      ALTCHAR = 1;
      break;
    }
}

void
PutC080 (addr, data)
     ADDR addr;
     BYTE data;
{
  (void) GetC080 (addr);	/* Same thing; either one works */
}

/* Beeps speaker if accessed a lot: */
BYTE
GetC030 (addr)
     ADDR addr;
{
  static int count = 0;

/* note std beep hits c030 0xc0 times */
  if (count++ >= 190)
    {
      count = 0;
      if (BeepNoise)
	beep ();
    }
  return (BYTE) (MegaRand & 0xff);
}

void
ChangeVID ()
{
  int rbank;
  ADDR addr;
  BYTE data;

  clear ();
  if (VID80 == 1)
    {
      rbank = 1;
      for (addr = 0x400; addr < 0x800; addr++)
	{
	  data = AMemory[addr];
	  Put0400 (addr, data, rbank);
	}
    }
  rbank = 0;
  for (addr = 0x400; addr < 0x800; addr++)
    {
      data = MMemory[addr];
      Put0400 (addr, data, rbank);
    }
}

/* Handles stores to text/lowres page 1 memory */
void
Put0400 (addr, data, rbank)
     register ADDR addr;
     BYTE data;
     int rbank;
{
  register int linenum;		/* Searching for Y-coordinate */
  register int columnnum;	/* Searching for X-coordinate */
  int screenhole = 1;		/* Are we in screen hole?     */

  /* Find which line the given address is in: */
  for (linenum = 0; linenum <= 23; linenum++)
    if ((addr >= LBasCalc[linenum]) && (addr <= (LBasCalc[linenum] + 39)))
      {
	columnnum = addr - LBasCalc[linenum];
	screenhole = 0;
	break;
      }

  /* If not on screen, don't draw anything: */
  if (screenhole)
    return;

  /* handle 80 column card if on */
  if ((rbank == 0) && (VID80 == 1))
    {
      columnnum = (columnnum * 2) + 1;
    };
  if ((rbank == 1) && (VID80 == 1))
    {
      columnnum = (columnnum * 2);
    };

  /* Put the terminal cursor at the right location on the screen: */
  /* We assume 80x24 terminal; no need to avoid drawing on last char. */
  move (linenum, columnnum);

  /*
     Set terminal into appropriate output mode and do it:
     tputs (TcapInverse,1,MegaPutChar);
   */
/* closer to the ALT charset rendition */
  if (data >= 128)
    {
      data = data & 0x7f;
    }
  else
    {
      standout ();
    }
  if (data < 0x20)
    data += 0x40;

  if (!iscntrl ((char) data))
    MegaPutChar ((char) data);
  else
    MegaPutChar ((char) (data - 64));
  standend ();

}

#define _setN_(b)      if ((b)!=0) P |= 128; else P &= 0x7f
#define _setV_(b)      if ((b)!=0) P |= 64;  else P &= 0xbf
  /* This bit not implemented */
#define _setB_(b)      if ((b)!=0) P |= 16;  else P &= 0xef
#define _setD_(b)      if ((b)!=0) P |= 8;   else P &= 0xf7
#define _setI_(b)      if ((b)!=0) P |= 4;   else P &= 0xfb
#define _setZ_(b)      if ((b)!=0) P |= 2;   else P &= 0xfd
#define _setC_(b)      if ((b)!=0) P |= 1;   else P &= 0xfe

static unsigned char buffer[512];
FILE *disk1,			/* slot 7, drive 1 280 block 5.25 floppy   */
 *disk2;			/* slot 7, drive 2 1024 blocks "hard disk" */

FILE *s6d1, *s6d2, *s5d1, *s5d2;

#define NBLOCKSD1 280
#define NBLOCKSD2 1024

void
ProInit ()
{
  MegaPutMem (0x0043, (BYTE) 0x70);	/* boot disk */
  if ((disk1 = fopen ("PRODOS.IMAGE.D1", "r+")) == NULL)
    {
      MegaShutDown ();
      exit (1);
    };
  if ((disk2 = fopen ("PRODOS.IMAGE.D2", "r+")) == NULL)
    {
      disk2 = 0;
    };
  if ((s5d1 = fopen ("s5d1", "r+")) == NULL)
    {
      s5d1 = 0;
    };
  if ((s5d2 = fopen ("s5d2", "r+")) == NULL)
    {
      s5d2 = 0;
    };
}

void
Dos33init ()
{
  if ((s6d1 = fopen ("s6d1", "r+")) == NULL)
    {
      MegaShutDown ();
      exit (1);
    };
  if ((s6d2 = fopen ("s6d2", "r+")) == NULL)
    {
      s6d2 = 0;
    };
}

void
ProFormat (drive, slot)
     int drive, slot;
{
  int numblocks;
  FILE *disk;

  disk = 0;
  if (slot == 0x70)
    {
      disk = drive ? disk2 : disk1;
      numblocks = drive ? NBLOCKSD2 : NBLOCKSD1;
    };
  if (slot == 0x50)
    {
      disk = drive ? s5d2 : s5d1;
      numblocks = NBLOCKSD1;
    };
  if (disk == 0)
    {
      _setC_ (1);
      A = 40;
      return;
    };

  (void) fseek (disk, (long) (512 * numblocks), 0);
  (void) fwrite (" ", 1, 1, disk);

  _setC_ (0);
  A = 0;

}

void
ProRead (drive, slot)
     int drive, slot;
{
  register int i;
  int block = MegaGetMem (0x46) + MegaGetMem (0x47) * 0x0100;
  int buf = MegaGetMem (0x44) + MegaGetMem (0x45) * 0x0100;
  FILE *disk;

  disk = 0;
  if (slot == 0x70)
    disk = drive ? disk2 : disk1;
  if (slot == 0x50)
    disk = drive ? s5d2 : s5d1;
  if (disk == 0)
    {
      _setC_ (1);
      A = 40;
      return;
    };

  (void) fseek (disk, (long) (block * 512), 0);

  (void) fread ((char *) buffer, 1, 512, disk);
  for (i = 0; i < 512; i++)
    MegaPutMem (buf + i, (BYTE) buffer[i]);
  _setC_ (0);			/* CLC */
  A = 0;
}

void
ProWrite (drive, slot)
     int drive, slot;
{
  register int i;
  int block = MegaGetMem (0x46) + MegaGetMem (0x47) * 0x0100;
  int buf = MegaGetMem (0x44) + MegaGetMem (0x45) * 0x0100;
  FILE *disk;

  disk = 0;
  if (slot == 0x70)
    disk = drive ? disk2 : disk1;
  if (slot == 0x50)
    disk = drive ? s5d2 : s5d1;
  if (disk == 0)
    {
      _setC_ (1);
      A = 40;
      return;
    };

  (void) fseek (disk, (long) (block * 512), 0);

  for (i = 0; i < 512; i++)
    buffer[i] = MegaGetMem (buf + i);

  (void) fwrite ((char *) buffer, 1, 512, disk);
  _setC_ (0);			/* CLC */
  A = 0;
}

void
ProStatus (drive, slot)
     int drive, slot;
{
  int numblocks;
  FILE *disk;

  disk = 0;
  if (slot == 0x70)
    disk = drive ? disk2 : disk1;
  if (slot == 0x50)
    disk = drive ? s5d2 : s5d1;
  if (disk == 0)
    {
      _setC_ (1);
      A = 40;
      return;
    };

  if (slot == 0x70)
    numblocks = drive ? NBLOCKSD2 : NBLOCKSD1;
  if (slot == 0x50)
    numblocks = NBLOCKSD1;

  _setC_ (0);			/* CLC */
  A = 0;			/* LDA #0 */
  Y = numblocks / 256;
  X = numblocks % 256;
}

void
prodos ()
{
  int slot;
  int drive = ((MegaGetMem (0x43) >= 128) ? 1 : 0);

  slot = (MegaGetMem (0x43) & 0x70);

  switch (MegaGetMem (0x42))
    {
    case 0:
      ProStatus (drive, slot);
      break;
    case 1:
      ProRead (drive, slot);
      break;
    case 2:
      ProWrite (drive, slot);
      break;
    case 3:
      ProFormat (drive, slot);
      break;
    }
}

void
bootdos33 ()
{
/* patched for interleaving change... */
  static int interleave[16] = {
    0x0, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,
    0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0xf
  };

  int addr, byte, i;

  addr = 0xb600;
  i = 0;
  for (i = 0; i <= 9; i++)
    {				/* sectors 0-9 */
      (void) fseek (s6d1, interleave[i] * 256L, 0);
      for (byte = 0; byte <= 255; byte++)
	MMemory[addr++] = fgetc (s6d1);
    }
  MMemory[0xbd00] = 0x60;
  PPC = 0xb700;
  X = 0x60;
}

void
bootprodos ()
{
  FILE *fp;
  int addr, byte;

  if ((fp = fopen ("PRODOS", "r")) == NULL)
    {
      MegaShutDown ();
      exit (1);
    };
  addr = 0x2000;
  while ((byte = fgetc (fp)) != EOF)
    MMemory[addr++] = byte;
  (void) fclose (fp);

  PPC = 0x2000;
  MegaPutMem (0x0043, (BYTE) 0x70);	/* boot disk */
}




void
dos33 ()
{
/* dos emulation routine
*/
  static int interleave[16] = {
    0x0, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,
    0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0xf
  };

  int ad;			/* address temp var */
  int slot, drive, volume, sector, track, cmd;	/* RWTS parameters */
  int dest;			/* destination and number of bytes */
  unsigned bytes;
  int stat;
  unsigned char buff[256];
  FILE *fid;
  long pos;
  long pos2;
  int i, j;

  /* get RWTS address */
  ad = Y + (256 * A);		/* Yreg-low Areg-high */

  /* parse RWTS info */
  slot = MegaGetMem (ad + 1);
  drive = MegaGetMem (ad + 2);
  volume = MegaGetMem (ad + 3);
  track = MegaGetMem (ad + 4);
  sector = interleave[MegaGetMem (ad + 5)];
  dest = MegaGetMem (ad + 8) + (MegaGetMem (ad + 9) * 256);
  bytes = MegaGetMem (ad + 11);
  if (bytes == 0)
    bytes = 256;
/* note ignore the bytes parameter... */
  bytes = 256;
  cmd = MegaGetMem (ad + 12);
  pos = (track * 4096L) + (sector * 256L);
  /* assume the cmd went ok */
  MegaPutMem (ad + 13, 0);
  MegaPutMem (ad + 14, drive);	/* return new last s,d,v */
  MegaPutMem (ad + 15, slot);
  MegaPutMem (ad + 16, drive);	/* returned vol # is same as drive # */
  _setC_ (0);
  if ((cmd == 0) || (cmd == 4))
    return;			/* seek and init ok */
  /* select file id */
  fid = 0;
  X = slot;
  Y = 0x0d;
  if ((slot == 0x60) && (drive == 1))
    fid = s6d1;
  if ((slot == 0x60) && (drive == 2))
    fid = s6d2;
  if ((slot == 0x50) && (drive == 1))
    fid = s5d1;
  if ((slot == 0x50) && (drive == 2))
    fid = s5d2;
  /* i/o error */
  if (fid == 0)
    {
      MegaPutMem (ad + 13, 0x40);
      _setC_ (1);
      return;
    };
  /* set position */
  (void) fseek (fid, (long) (pos), 0);
  /* read */
  if (cmd == 1)
    {
      (void) fread (buff, 1, bytes, fid);
      /* copy into memory */
      for (j = 0; j < bytes; j++)
	{
	  MegaPutMem (dest + j, buff[j]);
	};
      return;
    };
  /* write */
  if (cmd == 2)
    {
      /* copy from memory */
      for (j = 0; j < bytes; j++)
	{
	  buff[j] = MegaGetMem (dest + j);
	};
      fwrite (buff, 1, bytes, fid);
      return;
    };
}

int readstate;
int writestate;
int area[256];
int modetable[8][64] = {
/* 0000 - 01ff */
  {0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1},
/* 0200 - 03ff,0800 - 1fff,6000 - bfff */
  {0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1},
/* 0400 - 07ff */
  {0, 0, 0, 0, 0, 0, 1, 1,
   0, 0, 0, 0, 0, 0, 1, 1,
   0, 0, 0, 0, 0, 0, 1, 1,
   0, 0, 0, 0, 0, 0, 1, 1,
   1, 1, 0, 0, 1, 1, 1, 1,
   1, 1, 0, 0, 1, 1, 1, 1,
   1, 1, 0, 0, 1, 1, 1, 1,
   1, 1, 0, 0, 1, 1, 1, 1},
/* 2000 - 3fff */
  {0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 1, 1,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 0, 0, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 0, 0, 1, 1, 1, 1},
/* 4000 - 5fff */
  {0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1},
/* c000 - cfff */
  {5, 5, 5, 5, 5, 5, 5, 5,
   5, 5, 5, 5, 5, 5, 5, 5,
   5, 5, 5, 5, 5, 5, 5, 5,
   5, 5, 5, 5, 5, 5, 5, 5,
   5, 5, 5, 5, 5, 5, 5, 5,
   5, 5, 5, 5, 5, 5, 5, 5,
   5, 5, 5, 5, 5, 5, 5, 5,
   5, 5, 5, 5, 5, 5, 5, 5},
/* d000 - dfff */
  {4, 4, 4, 4, 4, 4, 4, 4,
   4, 4, 4, 4, 4, 4, 4, 4,
   2, 3, 2, 3, 2, 3, 2, 3,
   2, 3, 2, 3, 2, 3, 2, 3,
   4, 4, 4, 4, 4, 4, 4, 4,
   4, 4, 4, 4, 4, 4, 4, 4,
   2, 3, 2, 3, 2, 3, 2, 3,
   2, 3, 2, 3, 2, 3, 2, 3},
/* e000 - ffff */
  {4, 4, 4, 4, 4, 4, 4, 4,
   4, 4, 4, 4, 4, 4, 4, 4,
   0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1,
   4, 4, 4, 4, 4, 4, 4, 4,
   4, 4, 4, 4, 4, 4, 4, 4,
   0, 1, 0, 1, 0, 1, 0, 1,
   0, 1, 0, 1, 0, 1, 0, 1},
};

void
setupstates ()
{
  readstate = ((ALTZP) | (STORE80 << 1) | (PAGE2 << 2) | (HIRES << 3) |
	       (RamRead << 4) | (RAMRD << 5));
  writestate = ((ALTZP) | (STORE80 << 1) | (PAGE2 << 2) | (HIRES << 3) |
		(RamWrite << 4) | (RAMWRT << 5));
}

void
speedinit ()
{
  int i;
/* initialize the area table */
  for (i = 0; i < 256; i++)
    {
      area[i] = 1;
      if (i < 2)
	{			/* 0 - 1ff */
	  area[i] = 0;
	}
      else if ((i >= 4) && (i < 8))
	{			/* 400 - 7ff */
	  area[i] = 2;
	}
      else if ((i >= 0x20) && (i < 0x40))
	{			/* 2000 - 3fff */
	  area[i] = 3;
	}
      else if ((i >= 0x40) && (i < 0x60))
	{			/* 4000 - 5fff */
	  area[i] = 4;
	}
      else if ((i >= 0xc0) && (i < 0xd0))
	{			/* c000 - cfff */
	  area[i] = 5;
	}
      else if ((i >= 0xd0) && (i < 0xe0))
	{			/* d000 - dfff */
	  area[i] = 6;
	}
      else if (i >= 0xe0)
	{			/* e000 - ffff */
	  area[i] = 7;
	}
    }
}
