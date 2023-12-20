
/*
 *apple.h -- Globals' xdefs and equates for system calls for Apple ][ Emulator
 */

/* Character which ends the emulation: */

#define		MEGAQUITKEY		001		/* Control-A */

/* XENIX/BSD - Compatible includes: */

#ifndef BUFSIZ
#include <stdio.h>
#endif

#ifndef isalpha
#include <ctype.h>
#endif

#ifndef CBREAK
/*#include <sgtty.h>*/
#include <sgtty.h>
#endif

#ifndef TIOCGETP
#include <sys/ioctl.h>
#endif

#ifndef O_NDELAY
#include <fcntl.h>
#endif

#define BYTE unsigned char
#define ADDR int


/* 6502 Globals: */

extern int A,X,Y,P,S;
extern ADDR PPC;

extern int sockfd;

/* Emulation Globals: */

extern BYTE MMemory[];
extern BYTE AMemory[];
extern BYTE Rom[];
extern BYTE RamRead;
extern BYTE RamWrite;
extern BYTE Bank2Enable;
extern int  cycles[];
extern int  dosver;

/* global variables for //e emulation */
extern int slotcxROM;
extern int slotc3ROM;
extern int RAMRD;
extern int RAMWRT;
extern int STORE80;
extern int PAGE2;
extern int HIRES;
extern int ALTZP;
extern int DHGR;
extern int ALTCHAR;
extern int TEXT;
extern int MIXED;
extern int BUTTON0;
extern int BUTTON1;
extern int VID80;
extern int PDL0;    /* paddle counters which condition c06x flags */
extern int PDL1;
extern float paddle0;   /* current paddle positions [0,1] */
extern float paddle1;

extern FILE *s6d1,*s6d2,*disk1,*disk2,*s5d1,*s5d2;

extern BYTE MegaRand;
extern BYTE MegaLastKey;
extern BYTE MegaQuitDetect;
extern BYTE MegaSerial;
extern BYTE MegaSerialOut;
extern int DebugSingle;
extern int DebugTrace;
extern ADDR DebugBreak;
extern int BeepNoise; 


/* Apple ROM Contents: */

extern BYTE MegaGetMem();

/* Termcap stuff: */

extern void Rsoftswitches();
extern void Debugger();
extern void MegaShutDown();
extern void MegaStartUp();
extern void MegaPutMem();

extern void prodos();
extern void dos33();  

extern void CPUShutDown();
extern void CPUReset();
extern void CPUExecute();

extern void setupstates();
extern int readstate;
extern int writestate;
extern int area[256];
extern int modetable[8][64];
extern void speedinit();
extern void InitProdos();
extern BYTE GetC000(ADDR a);
extern FILE * log;
extern char * ip;
extern char * rcsid;
