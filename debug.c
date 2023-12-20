/* debug.c */

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

int DebugSingle;
int DebugTrace;
ADDR DebugBreak;
FILE *DebugFile;

void
htob (out, byte)
     FILE *out;
     BYTE byte;
{
  register int i;

  for (i = 7; i >= 0; i--)
    (void) fputc (byte & (1 << i) ? '1' : '0', out);
}

void
DebugDisasm (out)
     FILE *out;
{
  int opcode = MegaGetMem (PPC);
  int n;
  char *mne;

  switch (opcode)
    {
    case 0x69:			/* ADC #dd */
      mne = "ADC #$%2X";
      n = 1;
      break;
    case 0x65:			/* ADC aa */
      mne = "ADC  $%2X";
      n = 1;
      break;
    case 0x75:			/* ADC aa,X */
      mne = "ADC  $%2X,X";
      n = 1;
      break;
    case 0x6d:			/* ADC aaaa */
      mne = "ADC  $%4X";
      n = 2;
      break;
    case 0x7d:			/* ADC aaaa,X */
      mne = "ADC  $%4X,X";
      n = 2;
      break;
    case 0x79:			/* ADC aaaa,Y */
      mne = "ADC  $%4X,Y";
      n = 2;
      break;
    case 0x61:			/* ADC (aa,X) */
      mne = "ADC  $(%2X,X)";
      n = 1;
      break;
    case 0x71:			/* ADC (aa),Y */
      mne = "ADC  $(%2X),Y";
      n = 1;
      break;
    case 0x29:			/* AND #dd */
      mne = "AND #$%2X";
      n = 1;
      break;
    case 0x25:			/* AND aa */
      mne = "AND  $%2X";
      n = 1;
      break;
    case 0x35:			/* AND aa,X */
      mne = "AND  $%2X,X";
      n = 1;
      break;
    case 0x2d:			/* AND aaaa */
      mne = "AND  $%4X";
      n = 2;
      break;
    case 0x3d:			/* AND aaaa,X */
      mne = "AND  $%4X,X";
      n = 2;
      break;
    case 0x39:			/* AND aaaa,Y */
      mne = "AND  $%4X,Y";
      n = 2;
      break;
    case 0x21:			/* AND (aa,X) */
      mne = "AND  $(%2X,X)";
      n = 1;
      break;
    case 0x31:			/* AND (aa),Y */
      mne = "AND  ($%2X),Y";
      n = 1;
      break;
    case 0x0a:			/* ASL A */
      mne = "ASL  A";
      n = 0;
      break;
    case 0x06:			/* ASL aa */
      mne = "ASL  $%2X";
      n = 1;
      break;
    case 0x16:			/* ASL aa,X */
      mne = "ASL  $%2X,X";
      n = 1;
      break;
    case 0x0e:			/* ASL aaaa */
      mne = "ASL  $%4X";
      n = 2;
      break;
    case 0x1e:			/* ASL aaaa,X */
      mne = "ASL  $%4X,X";
      n = 2;
      break;
    case 0x90:			/* BCC rr */
      mne = "BCC  $%2X";
      n = -1;
      break;
    case 0xb0:			/* BCS rr */
      mne = "BCS  $%2X";
      n = -1;
      break;
    case 0xf0:			/* BEQ rr */
      mne = "BEQ  $%2X";
      n = -1;
      break;
    case 0x24:			/* BIT aa */
      mne = "BIT  $%2X";
      n = 1;
      break;
    case 0x2c:			/* BIT aaaa */
      mne = "BIT  $%4X";
      n = 2;
      break;
    case 0x30:			/* BMI rr */
      mne = "BMI  $%2X";
      n = -1;
      break;
    case 0xd0:			/* BNE rr */
      mne = "BNE  $%2X";
      n = -1;
      break;
    case 0x10:			/* BPL rr */
      mne = "BPL  $%2X";
      n = -1;
      break;
    case 0x00:			/* BRK */
      mne = "BRK";
      n = 0;
      break;
    case 0x50:			/* BVC rr */
      mne = "BVC  $%2X";
      n = -1;
      break;
    case 0x70:			/* BVS rr */
      mne = "BVS  $%2X";
      n = -1;
      break;
    case 0x18:			/* CLC */
      mne = "CLC";
      n = 0;
      break;
    case 0xd8:			/* CLD */
      mne = "CLD";
      n = 0;
      break;
    case 0x58:			/* CLI */
      mne = "CLI";
      n = 0;
      break;
    case 0xb8:			/* CLV */
      mne = "CLV";
      n = 0;
      break;
    case 0xc9:			/* CMP #dd */
      mne = "CMP #$%2X";
      n = 1;
      break;
    case 0xc5:			/* CMP aa */
      mne = "CMP  $%2X";
      n = 1;
      break;
    case 0xd5:			/* CMP aa,X */
      mne = "CMP  $%2X,X";
      n = 1;
      break;
    case 0xcd:			/* CMP aaaa */
      mne = "CMP  $%4X";
      n = 2;
      break;
    case 0xdd:			/* CMP aaaa,X */
      mne = "CMP  $%4X,X";
      n = 2;
      break;
    case 0xd9:			/* CMP aaaa,Y */
      mne = "CMP  $%4X,Y";
      n = 2;
      break;
    case 0xc1:			/* CMP (aa,X) */
      mne = "CMP  $(%2X,X)";
      n = 1;
      break;
    case 0xd1:			/* CMP (aa),y */
      mne = "CMP  $(%2X),y";
      n = 1;
      break;
    case 0xe0:			/* CPX #dd */
      mne = "CPX #$%2X";
      n = 1;
      break;
    case 0xe4:			/* CPX aa */
      mne = "CPX  $%2X";
      n = 1;
      break;
    case 0xec:			/* CPX aaaa */
      mne = "CPX  $%4X";
      n = 2;
      break;
    case 0xc0:			/* CPY #dd */
      mne = "CPY #$%2X";
      n = 1;
      break;
    case 0xc4:			/* CPY aa */
      mne = "CPY  $%2X";
      n = 1;
      break;
    case 0xcc:			/* CPY aaaa */
      mne = "CPY  $%4X";
      n = 2;
      break;
    case 0xc6:			/* DEC aa */
      mne = "DEC  $%2X";
      n = 1;
      break;
    case 0xd6:			/* DEC aa,X */
      mne = "DEC  $%2X,X";
      n = 1;
      break;
    case 0xce:			/* DEC aaaa */
      mne = "DEC  $%4X";
      n = 2;
      break;
    case 0xde:			/* DEC aaaa,X */
      mne = "DEC  $%4X,X";
      n = 2;
      break;
    case 0xca:			/* DEX */
      mne = "DEX";
      n = 0;
      break;
    case 0x88:			/* DEY */
      mne = "DEY";
      n = 0;
      break;
    case 0x49:			/* EOR #dd */
      mne = "EOR #$%2X";
      n = 1;
      break;
    case 0x45:			/* EOR aa */
      mne = "EOR  $%2X";
      n = 1;
      break;
    case 0x55:			/* EOR aa,X */
      mne = "EOR  $%2X,X";
      n = 1;
      break;
    case 0x4d:			/* EOR aaaa */
      mne = "EOR  $%4X";
      n = 2;
      break;
    case 0x5d:			/* EOR aaaa,X */
      mne = "EOR  $%4X,X";
      n = 2;
      break;
    case 0x59:			/* EOR aaaa,Y */
      mne = "EOR  $%4X,Y";
      n = 2;
      break;
    case 0x41:			/* EOR (aa,X) */
      mne = "EOR  ($%2X,X)";
      n = 1;
      break;
    case 0x51:			/* EOR (aa),Y */
      mne = "EOR  ($%2X),Y";
      n = 1;
      break;
    case 0xe6:			/* INC aa */
      mne = "INC  $%2X";
      n = 1;
      break;
    case 0xf6:			/* INC aa,X */
      mne = "INC  $%2X,X";
      n = 1;
      break;
    case 0xee:			/* INC aaaa */
      mne = "INC  $%4X";
      n = 2;
      break;
    case 0xfe:			/* INC aaaa,X */
      mne = "INC  $%4X,X";
      n = 2;
      break;
    case 0xe8:			/* INX */
      mne = "INX";
      n = 0;
      break;
    case 0xc8:			/* INY */
      mne = "INY";
      n = 0;
      break;
    case 0x4c:			/* JMP aaaa */
      mne = "JMP  $%4X";
      n = 2;
      break;
    case 0x6c:			/* JMP (aaaa) */
      mne = "JMP  ($%4X)";
      n = 2;
      break;
    case 0x20:			/* JSR aaaa */
      mne = "JSR  $%4X";
      n = 2;
      break;
    case 0xa9:			/* LDA #dd */
      mne = "LDA #$%2X";
      n = 1;
      break;
    case 0xa5:			/* LDA aa */
      mne = "LDA  $%2X";
      n = 1;
      break;
    case 0xb5:			/* LDA aa,X */
      mne = "LDA  $%2X,X";
      n = 1;
      break;
    case 0xad:			/* LDA aaaa */
      mne = "LDA  $%4X";
      n = 2;
      break;
    case 0xbd:			/* LDA aaaa,X */
      mne = "LDA  $%4X,X";
      n = 2;
      break;
    case 0xb9:			/* LDA aaaa,Y */
      mne = "LDA  $%4X,Y";
      n = 2;
      break;
    case 0xa1:			/* LDA (aa,X) */
      mne = "LDA  ($%2X,X)";
      n = 1;
      break;
    case 0xb1:			/* LDA (aa),Y */
      mne = "LDA  ($%2X),Y";
      n = 1;
      break;
    case 0xa2:			/* LDX #dd */
      mne = "LDX #$%2X";
      n = 1;
      break;
    case 0xa6:			/* LDX aa */
      mne = "LDX  $%2X";
      n = 1;
      break;
    case 0xb6:			/* LDX aa,Y */
      mne = "LDX  $%2X,Y";
      n = 1;
      break;
    case 0xae:			/* LDX aaaa */
      mne = "LDX  $%4X";
      n = 2;
      break;
    case 0xbe:			/* LDX aaaa,Y */
      mne = "LDX  $%4X,Y";
      n = 2;
      break;
    case 0xa0:			/* LDY #dd */
      mne = "LDY #$%2X";
      n = 1;
      break;
    case 0xa4:			/* LDY aa */
      mne = "LDY  $%2X";
      n = 1;
      break;
    case 0xb4:			/* LDY aa,X */
      mne = "LDY  $%2X,X";
      n = 1;
      break;
    case 0xac:			/* LDY aaaa */
      mne = "LDY  $%4X";
      n = 2;
      break;
    case 0xbc:			/* LDY aaaa,X */
      mne = "LDY  $%4X,X";
      n = 2;
      break;
    case 0x4a:			/* LSR A */
      mne = "LSR";
      n = 0;
      break;
    case 0x46:			/* LSR aa */
      mne = "LSR  $%2X";
      n = 1;
      break;
    case 0x56:			/* LSR aa,X */
      mne = "LSR  $%2X,X";
      n = 1;
      break;
    case 0x4e:			/* LSR aaaa */
      mne = "LSR  $%4X";
      n = 2;
      break;
    case 0x5e:			/* LSR aaaa,X */
      mne = "LSR  $%4X,X";
      n = 2;
      break;
    case 0xea:			/* NOP */
      mne = "NOP";
      n = 0;
      break;
    case 0x09:			/* ORA #dd */
      mne = "ORA #$%2X";
      n = 1;
      break;
    case 0x05:			/* ORA aa */
      mne = "ORA  $%2X";
      n = 1;
      break;
    case 0x15:			/* ORA aa,X */
      mne = "ORA  $%2X,X";
      n = 1;
      break;
    case 0x0d:			/* ORA aaaa */
      mne = "ORA  $%4X";
      n = 2;
      break;
    case 0x1d:			/* ORA aaaa,X */
      mne = "ORA  $%4X,X";
      n = 2;
      break;
    case 0x19:			/* ORA aaaa,Y */
      mne = "ORA  $%4X,Y";
      n = 2;
      break;
    case 0x01:			/* ORA (aa,X) */
      mne = "ORA  ($%2X,X)";
      n = 1;
      break;
    case 0x11:			/* ORA (aa),Y */
      mne = "ORA  ($%2X),Y";
      n = 1;
      break;
    case 0x48:			/* PHA */
      mne = "PHA";
      n = 0;
      break;
    case 0x08:			/* PHP */
      mne = "PHP";
      n = 0;
      break;
    case 0x68:			/* PLA */
      mne = "PLA";
      n = 0;
      break;
    case 0x28:			/* PLP */
      mne = "PLP";
      n = 0;
      break;
    case 0x2a:			/* ROL A */
      mne = "ROL  A";
      n = 0;
      break;
    case 0x26:			/* ROL aa */
      mne = "ROL  $%2X";
      n = 1;
      break;
    case 0x36:			/* ROL aa,X */
      mne = "ROL  $%2X,X";
      n = 1;
      break;
    case 0x2e:			/* ROL aaaa */
      mne = "ROL  $%4X";
      n = 2;
      break;
    case 0x3e:			/* ROL aaaa,X */
      mne = "ROL  $%4X,X";
      n = 2;
      break;
    case 0x6a:			/* ROR A */
      mne = "ROR  A";
      n = 0;
      break;
    case 0x66:			/* ROR aa */
      mne = "ROR  $%2X";
      n = 1;
      break;
    case 0x76:			/* ROR aa,X */
      mne = "ROR  $%2X,X";
      n = 1;
      break;
    case 0x6e:			/* ROR aaaa */
      mne = "ROR  $%4X";
      n = 2;
      break;
    case 0x7e:			/* ROR aaaa,X */
      mne = "ROR  $%4X,X";
      n = 2;
      break;
    case 0x40:			/* RTI */
      mne = "RTI";
      n = 0;
      break;
    case 0x60:			/* RTS */
      mne = "RTS";
      n = 0;
      break;
    case 0xe9:			/* SBC #dd */
      mne = "SBC #$%2X";
      n = 1;
      break;
    case 0xe5:			/* SBC aa */
      mne = "SBC  $%2X";
      n = 1;
      break;
    case 0xf5:			/* SBC aa,x */
      mne = "SBC  $%2X,X";
      n = 1;
      break;
    case 0xed:			/* SBC aaaa */
      mne = "SBC  $%4X";
      n = 2;
      break;
    case 0xfd:			/* SBC aaaa,X */
      mne = "SBC  $%4X,X";
      n = 2;
      break;
    case 0xf9:			/* SBC aaaa,Y */
      mne = "SBC  $%4X,Y";
      n = 2;
      break;
    case 0xe1:			/* SBC (aa,X) */
      mne = "SBC  ($%2X,X)";
      n = 1;
      break;
    case 0xf1:			/* SBC (aa),Y */
      mne = "SBC  ($%2X),Y";
      n = 1;
      break;
    case 0x38:			/* SEC */
      mne = "SEC";
      n = 0;
      break;
    case 0xf8:			/* SED */
      mne = "SED";
      n = 0;
      break;
    case 0x78:			/* SEI */
      mne = "SEI";
      n = 0;
      break;
    case 0x85:			/* STA aa */
      mne = "STA  $%2X";
      n = 1;
      break;
    case 0x95:			/* STA aa,X */
      mne = "STA  $%2X,X";
      n = 1;
      break;
    case 0x8d:			/* STA aaaa */
      mne = "STA  $%4X";
      n = 2;
      break;
    case 0x9d:			/* STA aaaa,X */
      mne = "STA  $%4X,X";
      n = 2;
      break;
    case 0x99:			/* STA aaaa,Y */
      mne = "STA  $%4X,Y";
      n = 2;
      break;
    case 0x81:			/* STA (aa,X) */
      mne = "STA  ($%2X,X)";
      n = 1;
      break;
    case 0x91:			/* STA (aa),Y */
      mne = "STA  ($%2X),Y";
      n = 1;
      break;
    case 0x86:			/* STX aa */
      mne = "STX  $%2X";
      n = 1;
      break;
    case 0x96:			/* STX aa,Y */
      mne = "STX  $%2X,Y";
      n = 1;
      break;
    case 0x8e:			/* STX aaaa */
      mne = "STX  $%4X";
      n = 2;
      break;
    case 0x84:			/* STY aa */
      mne = "STY  $%2X";
      n = 1;
      break;
    case 0x94:			/* STY aa,X */
      mne = "STY  $%2X,X";
      n = 1;
      break;
    case 0x8c:			/* STY aaaa */
      mne = "STY  $%4X";
      n = 2;
      break;
    case 0xaa:			/* TAX */
      mne = "TAX";
      n = 0;
      break;
    case 0xa8:			/* TAY */
      mne = "TAY";
      n = 0;
      break;
    case 0xba:			/* TSX */
      mne = "TSX";
      n = 0;
      break;
    case 0x8a:			/* TXA */
      mne = "TXA";
      n = 0;
      break;
    case 0x9a:			/* TXS */
      mne = "TXS";
      n = 0;
      break;
    case 0x98:			/* TYA */
      mne = "TYA";
      n = 0;
      break;
    default:			/* Undefined opcode */
      mne = "UNDEFINED";
      n = 0;
      break;
    }
  if (!DebugTrace)
    (void) printw ("%4X: ", PPC);
  if (DebugTrace)
    (void) fprintf (out, "%4X: ", PPC);
  switch (n)
    {
    case -1:
      if (!DebugTrace)
	(void) printw (mne, PPC + (char) MegaGetMem (PPC + 1) + 2);
      if (DebugTrace)
	(void) fprintf (out, mne, PPC + (char) MegaGetMem (PPC + 1) + 2);
      break;
    case 0:
      if (!DebugTrace)
	(void) printw (mne);
      if (DebugTrace)
	(void) fprintf (out, mne);
      break;
    case 1:
      if (!DebugTrace)
	(void) printw (mne, MegaGetMem (PPC + 1));
      if (DebugTrace)
	(void) fprintf (out, mne, MegaGetMem (PPC + 1));
      break;
    case 2:
      if (!DebugTrace)
	(void) printw (mne,
		       MegaGetMem (PPC + 1) + 256 * MegaGetMem (PPC + 2));
      if (DebugTrace)
	(void) fprintf (out, mne,
			MegaGetMem (PPC + 1) + 256 * MegaGetMem (PPC + 2));
      break;
    }
  if (!DebugTrace)
    addstr ("\n");
  if (DebugTrace)
    (void) fprintf (out, "\n");
  refresh ();
}




void
Debugger ()
{
  int done = 0;
  FILE *out = stdout;
  char buffer[255];

  echo ();
  standend ();
  move (0, 0);
  refresh ();

  if (DebugTrace)
    out = DebugFile;

  if (DebugTrace)
    (void) fprintf (out, "A=%2X Y=%2X X=%2X S=%2X P=%2X :", A, Y, X, S, P);
  if (!DebugTrace)
    (void) printw ("A=%2X Y=%2X X=%2X S=%2X P=%2X :", A, Y, X, S, P);
  DebugDisasm (out);
  if (!DebugTrace && !DebugSingle)
    {
      (void) printw ("Apple //e Emulator Debugger\n");
    }

  if (DebugTrace)
    goto traceout;

  while (!done)
    {
      (void) printw ("DBG> ");
      refresh ();
      getstr (buffer);

      switch (buffer[0])
	{
	case 'q':		/* quit to UNIX */
	  DebugSingle = 0;
	  DebugTrace = 0;
	  MegaQuitDetect = 1;
	  done = 1;
	  break;
	case 'l':		/* load UNIX file into memory */
	  {
	    ADDR addr = 0x2000;
	    FILE *fp;
	    int ch;

	    (void) printw ("Filename: ");
	    refresh ();
	    getstr (buffer);
	    if ((fp = fopen (buffer, "r")) == NULL)
	      break;
	    while ((ch = fgetc (fp)) != EOF)
	      MegaPutMem (addr++, (BYTE) ch);
	    (void) fclose (fp);
	    (void) printw ("%s: loaded at ,a$2000, l$%x\n", buffer,
			   addr - 0x2000 - 1);
	    refresh ();
	    MegaQuitDetect = 0;
	    done = 0;
	    break;
	  }
	case 'f':		/* change floppy-file assignments */
	  {
	    FILE *fnew;
	    char bf[255];

	    (void) printw ("New floppy filename: ");
	    refresh ();
	    getstr (buffer);
	    if ((fnew = fopen (buffer, "r+")) == NULL)
	      break;
	    (void) fclose (fnew);
	    (void) printw ("1-s7d1 2-s6d1 3-s6d2 4-s5d1 5-s5d2: ");
	    refresh ();
	    getstr (bf);
	    if (bf[0] == '1')
	      {
		(void) fclose (disk1);
		disk1 = fopen (buffer, "r+");
	      }
	    else if (bf[0] == '2')
	      {
		(void) fclose (s6d1);
		s6d1 = fopen (buffer, "r+");
	      }
	    else if (bf[0] == '3')
	      {
		(void) fclose (s6d2);
		s6d2 = fopen (buffer, "r+");
	      }
	    else if (bf[0] == '4')
	      {
		(void) fclose (s5d1);
		s5d1 = fopen (buffer, "r+");
	      }
	    else if (bf[0] == '5')
	      {
		(void) fclose (s5d2);
		s5d2 = fopen (buffer, "r+");
	      }
	    MegaQuitDetect = 0;
	    done = 0;
	    break;
	  }
	case 't':		/* trace file */
	  (void) printw ("Filename: ");
	  refresh ();
	  getstr (buffer);
	  if ((DebugFile = fopen (buffer, "w")) == NULL)
	    break;
	  DebugTrace = 1;
	  MegaQuitDetect = 0;
	  done = 0;
	  break;
	case 'd':		/* boot dos 3.3 */
	  bootdos33 ();
	  RamRead = 0;
	  MegaQuitDetect = 0;
	  done = 1;
	  break;
	case 'p':		/* boot ProDOS */
	  bootprodos ();
	  RamRead = 0;
	  MegaQuitDetect = 0;
	  done = 1;
	  break;
	case 'r':		/* execute a RESET */
	  /* dirty the power-up byte csum */
	  MegaPutMem ((ADDR) 0x3f4, (BYTE) 0);
	  CPUReset ();
	  MegaQuitDetect = 0;
	  done = 1;
	  break;
	case 'c':		/* continue */
	  DebugSingle = 0;
	  MegaQuitDetect = 0;
	  done = 1;
	  break;
	case 's':		/* step */
	case '\0':
	  DebugSingle = 1;
	  DebugTrace = 0;
	  MegaQuitDetect = 0;
	  done = 1;
	  break;
	default:
	  MegaQuitDetect = 0;
	}
    }
traceout:
  raw ();
  noecho ();
  nonl ();
}
