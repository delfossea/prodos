
/*
 *6502.c -- Mostek/Rockwell/Synertek/Siliconix 6502 for Apple II Emulator
 *
 * Modified 4/20/1990 Randy Frank randy@tessa.iaf.uiowa.edu
 *  Added 65c02 support and fxed bugs in ADC, SBC, PLP, and RTI emulation
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

/* remove the next line to disable 65c02 emulation. */
#define is65C02
/* this is necessary for some apple ][+ code */

int cycles[256] = {
  7, 6, 2, 2, 5, 3, 5, 2, 3, 2, 2, 2, 6, 4, 6, 2,
  2, 5, 5, 2, 5, 4, 6, 2, 2, 4, 2, 2, 6, 4, 7, 2,
  6, 6, 2, 2, 3, 3, 5, 2, 4, 2, 2, 2, 4, 4, 6, 2,
  2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 2, 2, 4, 4, 7, 2,
  6, 6, 2, 2, 2, 3, 5, 2, 3, 2, 2, 2, 3, 4, 6, 2,
  2, 5, 5, 2, 2, 4, 6, 2, 2, 4, 3, 2, 2, 4, 7, 2,
  6, 6, 2, 2, 3, 3, 5, 2, 4, 2, 2, 2, 5, 4, 6, 2,
  2, 5, 5, 2, 4, 4, 6, 2, 2, 4, 4, 2, 6, 4, 7, 2,
  2, 6, 2, 2, 3, 3, 3, 2, 2, 2, 2, 2, 4, 4, 4, 2,
  2, 6, 5, 2, 4, 4, 4, 2, 2, 5, 2, 2, 4, 5, 5, 2,
  2, 6, 2, 2, 3, 3, 3, 2, 2, 2, 2, 2, 4, 4, 4, 2,
  2, 5, 5, 2, 4, 4, 4, 2, 2, 4, 2, 2, 4, 4, 4, 2,
  2, 6, 2, 2, 3, 3, 5, 2, 2, 2, 2, 2, 4, 4, 6, 2,
  2, 5, 5, 2, 2, 4, 6, 2, 2, 4, 3, 2, 2, 4, 7, 2,
  2, 6, 2, 2, 3, 3, 5, 2, 2, 2, 2, 2, 4, 4, 6, 2,
  2, 5, 5, 2, 2, 4, 6, 2, 2, 4, 4, 2, 2, 4, 7, 2
};

/*
 * 6502 Globals:
 */

int A, X, Y, P, S;
ADDR PPC;

int dosver;			/* 0=none 1=dos33 2=pdos */

void CPUReset ( /*_  _*/ );
void _push_ ( /*_ BYTE _*/ );
BYTE _pull_ ( /*_ _*/ );
BYTE _eaimm_ ( /*_ _*/ );
BYTE _eazp_ ( /*_ _*/ );
BYTE _eazpx_ ( /*_ _*/ );
BYTE _eazpy_ ( /*_ _*/ );
ADDR _eaabs_ ( /*_ _*/ );
ADDR _eaabsind_ ( /*_ _*/ );
ADDR _eaabsx_ ( /*_ _*/ );
ADDR _eaabsy_ ( /*_ _*/ );
ADDR _eaindx_ ( /*_ _*/ );
ADDR _eaindy_ ( /*_ _*/ );

#ifdef is65C02
ADDR _eazpind_ ( /*_ _*/ );
ADDR _eaabsindx_ ( /*_ _*/ );
#endif


void
CPUReset ()
{
  /* Setup registers: */
  A = X = Y = 0;
  S = 0xff;
  P = 0x30;
  dosver = 0;

  /* set up the video & bank switching vars    */
  RamRead = 0;			/* Read ROM            */
  RamWrite = 1;			/* Write RAM           */
  Bank2Enable = 0;		/* Bank 1 selected     */
  Rsoftswitches ();

  /* Set PPC to pointer at $FFFC: */
  PPC = MegaGetMem (0xfffc) + MegaGetMem (0xfffd) * 0x0100;

  /* Should execute instructions forever after this */
}

/* This internal routine pushes a byte onto the stack */
void
_push_ (byte)
     BYTE byte;
{
  MegaPutMem ((ADDR) (0x100 + S--), byte);
  S &= 0xff;
}

/* This internal routine pulls a byte from the stack */
BYTE
_pull_ ()
{
  S++;
  S &= 0xff;
  return MegaGetMem (0x100 + S);
}

/* This internal routine fetches an immediate operand value */
BYTE
_eaimm_ ()
{
  register BYTE i;

  i = MegaGetMem (PPC++);
  PPC &= 0xffff;
  return i;
}

/* This internal routine fetches a zero-page operand address */
BYTE
_eazp_ ()
{
  register BYTE a;

  a = MegaGetMem (PPC++);
  PPC &= 0xffff;

  return a;
}

/* This internal routine fetches a zpage,X operand address */
BYTE
_eazpx_ ()
{
  register BYTE a;

  a = MegaGetMem (PPC++);
  PPC &= 0xffff;
  a += X;
  a &= 0xff;

  return a;
}

/* This internal routine fetches a zpage,Y operand address */
BYTE
_eazpy_ ()
{
  register BYTE a;

  a = MegaGetMem (PPC++);
  PPC &= 0xffff;
  a += Y;
  a &= 0xff;

  return a;
}

/* This internal routine fetches an absolute operand address */
ADDR
_eaabs_ ()
{
  register BYTE lo, hi;

  lo = MegaGetMem (PPC++);
  PPC &= 0xffff;
  hi = MegaGetMem (PPC++);
  PPC &= 0xffff;

  return lo + hi * 0x0100;
}

/* This internal routine fetches an indirect absolute operand address */
ADDR
_eaabsind_ ()
{
  register int a, lo, hi;

  a = _eaabs_ ();
  lo = MegaGetMem ((ADDR) a++);
  a &= 0xffff;
  hi = MegaGetMem ((ADDR) a);

  return lo + hi * 0x0100;
}

/* This internal routine fetches an absolute,X operand address */
ADDR
_eaabsx_ ()
{
  register int a;

  a = _eaabs_ () + X;

  return (a & 0xffff);		/* Not entirely correct */
}

/* This internal routine fetches an absolute,Y operand address */
ADDR
_eaabsy_ ()
{
  register int a;

  a = _eaabs_ () + Y;

  return (a & 0xffff);		/* Not entirely correct */
}

/* This internal routine fetches a (zpage,X) operand address */
ADDR
_eaindx_ ()
{
  register int a, lo, hi;

  a = _eazpx_ ();
  lo = MegaGetMem ((ADDR) a++);
  a &= 0xff;
  hi = MegaGetMem ((ADDR) a);

  return lo + hi * 0x0100;
}

/* This internal routine fetches a (zpage),Y operand address */
ADDR
_eaindy_ ()
{
  register int a, lo, hi;

  a = _eazp_ ();
  lo = MegaGetMem ((ADDR) a++);
  a &= 0xff;
  hi = MegaGetMem ((ADDR) a);

  return ((Y + lo + hi * 0x0100) & 0xffff);	/* Not entirely correct */
}

#ifdef is65C02
ADDR
_eazpind_ ()
/* this routine returns a (zp) address */
{
  register int a, lo, hi;
  a = _eazp_ ();
  lo = MegaGetMem (a);
  a++;
  a &= 255;
  hi = MegaGetMem (a);
  return ((lo + hi * 256) & 65535);
}

ADDR
_eaabsindx_ ()
/* this routine returns a (abs,x) address for 65c02 JMP */
{
  register int a, lo, hi;
  a = (_eaabs_ () + X) & 65535;
  lo = MegaGetMem (a);
  a++;
  a &= 65535;
  hi = MegaGetMem (a);
  return ((lo + hi * 256) & 65535);
}
#endif

#ifdef NEVER
/* Macros to set the P flags: */
#define _setN_(b)	if ((b)!=0) P |= 0x80;   else P &= ~0x80
#define _setV_(b)	if ((b)!=0) P |= 0x40;   else P &= ~0x40
/* This bit not implemented */
#define _setB_(b)	if ((b)!=0) P |= 0x10;   else P &= ~0x10
#define _setD_(b)	if ((b)!=0) P |= 0x08;   else P &= ~0x08
#define _setI_(b)	if ((b)!=0) P |= 0x04;   else P &= ~0x04
#define _setZ_(b)	if ((b)!=0) P |= 0x02;   else P &= ~0x02
#define _setC_(b)	if ((b)!=0) P |= 0x01;   else P &= ~0x01
#endif
/* Macros to set the P flags: */
void
_setN_ (b)
{
  if ((b) != 0)
    P |= 0x80;
  else
    P &= ~0x80;
}

void
_setV_ (b)
{
  if ((b) != 0)
    P |= 0x40;
  else
    P &= ~0x40;
}

/* This bit not implemented */
void
_setB_ (b)
{
  if ((b) != 0)
    P |= 0x10;
  else
    P &= ~0x10;
}

void
_setD_ (b)
{
  if ((b) != 0)
    P |= 0x08;
  else
    P &= ~0x08;
}

void
_setI_ (b)
{
  if ((b) != 0)
    P |= 0x04;
  else
    P &= ~0x04;
}

void
_setZ_ (b)
{
  if ((b) != 0)
    P |= 0x02;
  else
    P &= ~0x02;
}

void
_setC_ (b)
{
  if ((b) != 0)
    P |= 0x01;
  else
    P &= ~0x01;
}

  /* Macros to read the P flags: */
#define _getN_  ((P & 0x80) ? 1 : 0)
#define _getV_  ((P & 0x40) ? 1 : 0)
/* This bit not implemented */
#define _getB_  ((P & 0x10) ? 1 : 0)
#define _getD_  ((P & 0x08) ? 1 : 0)
#define _getI_  ((P & 0x04) ? 1 : 0)
#define _getZ_  ((P & 0x02) ? 1 : 0)
#define _getC_  ((P & 0x01) ? 1 : 0)
void
bootserial() {
	fprintf(stderr,"BOOT\n");
}

/* This routine executes a single instruction. */
void
CPUExecute ()
{
  register int opcode;		/* Scratch: Opcode fetched */
  register int d;		/* Scratch: Data byte fetched */
  register int lo;		/* Scratch: Lo8 for building ptr */
  register int hi;		/* Scratch: Hi8 for building ptr */
  register int al;		/* Scratch: Accumulator lo nibble */
  register int ah;		/* Scratch: Accumulator hi nibble */
  register int ol;		/* Scratch: Operand lo nibble */
  register int oh;		/* Scratch: Operand hi nibble */
  register int Vtmp;		/* Scratch: for correct overflow calcs */

/* do prodos stuff */

/*
  if ((PPC == 0xc780) && (slotcxROM == 0)) {
    prodos();
    dosver = 2;
  };
  if ((PPC == 0xc580) && (slotcxROM == 0)) {
    prodos();
    dosver = 2;
  };
*/

/* do RWTS emulation */

/*
  if ((PPC == 0xbd00) && (dosver == 1))
    dos33();
*/

/* handle pr#6 and pr#7 */

/*
  if ((PPC == 0xc600) && (slotcxROM == 0)) {
	dosver = 1;
	bootdos33();
	};
  if ((PPC == 0xc700) && (slotcxROM == 0)) {
	dosver = 2;
	bootprodos();
	};
*/
  if ((PPC == 0xc200) && (slotcxROM == 0)) {
	bootserial();
	}
  if ((PPC & 0xFF00) == 0xBF00)
    {
      bf00 ();
    }

  opcode = MegaGetMem (PPC++);
  PPC &= 0xffff;

/* do the joystick counter... */
  if (PDL0 > 0)
    PDL0 = PDL0 - cycles[opcode & 0xff];
  if (PDL1 > 0)
    PDL1 = PDL1 - cycles[opcode & 0xff];

  switch (opcode)
    {
    case 0x69:			/* ADC #dd */
      d = _eaimm_ ();
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
/* condition V as bit 7 of Ain D Aout and Carryout */
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x65:			/* ADC aa */
      d = MegaGetMem ((ADDR) _eazp_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x75:			/* ADC aa,X */
      d = MegaGetMem ((ADDR) _eazpx_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x6d:			/* ADC aaaa */
      d = MegaGetMem (_eaabs_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x7d:			/* ADC aaaa,X */
      d = MegaGetMem (_eaabsx_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x79:			/* ADC aaaa,Y */
      d = MegaGetMem (_eaabsy_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x61:			/* ADC (aa,X) */
      d = MegaGetMem (_eaindx_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x71:			/* ADC (aa),Y */
      d = MegaGetMem (_eaindy_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;



    case 0x29:			/* AND #dd */
      A &= _eaimm_ ();
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x25:			/* AND aa */
      A &= MegaGetMem ((ADDR) _eazp_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x35:			/* AND aa,X */
      A &= MegaGetMem ((ADDR) _eazpx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x2d:			/* AND aaaa */
      A &= MegaGetMem (_eaabs_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x3d:			/* AND aaaa,X */
      A &= MegaGetMem (_eaabsx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x39:			/* AND aaaa,Y */
      A &= MegaGetMem (_eaabsy_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x21:			/* AND (aa,X) */
      A &= MegaGetMem (_eaindx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x31:			/* AND (aa),Y */
      A &= MegaGetMem (_eaindy_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;



    case 0x0a:			/* ASL A */
      _setC_ (A >= 0x80);
      A = (A << 1) & 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x06:			/* ASL aa */
      hi = _eazp_ ();
      d = MegaGetMem ((ADDR) hi);
      _setC_ (d >= 0x80);
      d = (d << 1) & 0xff;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x16:			/* ASL aa,X */
      hi = _eazpx_ ();
      d = MegaGetMem ((ADDR) hi);
      _setC_ (d >= 0x80);
      d = (d << 1) & 0xff;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x0e:			/* ASL aaaa */
      hi = _eaabs_ ();
      d = MegaGetMem ((ADDR) hi);
      _setC_ (d >= 0x80);
      d = (d << 1) & 0xff;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x1e:			/* ASL aaaa,X */
      hi = _eaabsx_ ();
      d = MegaGetMem ((ADDR) hi);
      _setC_ (d >= 0x80);
      d = (d << 1) & 0xff;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;



    case 0x90:			/* BCC rr */
      lo = _eaimm_ ();
      if (lo >= 0x80)
	lo -= 0x0100;
      if (!_getC_)
	PPC += lo;
      break;



    case 0xb0:			/* BCS rr */
      lo = _eaimm_ ();
      if (lo >= 0x80)
	lo -= 0x0100;
      if (_getC_)
	PPC += lo;
      break;



    case 0xf0:			/* BEQ rr */
      lo = _eaimm_ ();
      if (lo >= 0x80)
	lo -= 0x0100;
      if (_getZ_)
	PPC += lo;
      break;



    case 0x24:			/* BIT aa */
      d = MegaGetMem ((ADDR) _eazp_ ());
      _setN_ (d >= 0x80);
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      break;

    case 0x2c:			/* BIT aaaa */
      d = MegaGetMem (_eaabs_ ());
      _setN_ (d >= 0x80);
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      break;



    case 0x30:			/* BMI rr */
      lo = _eaimm_ ();
      if (lo >= 0x80)
	lo -= 0x0100;
      if (_getN_)
	PPC += lo;
      break;



    case 0xd0:			/* BNE rr */
      lo = _eaimm_ ();
      if (lo >= 0x80)
	lo -= 0x0100;
      if (!_getZ_)
	PPC += lo;
      break;



    case 0x10:			/* BPL rr */
      lo = _eaimm_ ();
      if (lo >= 0x80)
	lo -= 0x0100;
      if (!_getN_)
	PPC += lo;
      break;



    case 0x00:			/* BRK */
      lo = (PPC + 1) & 65535;
      _push_ ((BYTE) (lo / 0x0100));
      _push_ ((BYTE) (lo % 0x0100));
      _push_ ((BYTE) P);
      _setB_ (1);
      lo = MegaGetMem (0xfffe);
      hi = MegaGetMem (0xffff);
      PPC = lo + (hi * 0x0100);
      break;



    case 0x50:			/* BVC rr */
      lo = _eaimm_ ();
      if (lo >= 0x80)
	lo -= 0x0100;
      if (!_getV_)
	PPC += lo;
      break;



    case 0x70:			/* BVS rr */
      lo = _eaimm_ ();
      if (lo >= 0x80)
	lo -= 0x0100;
      if (_getV_)
	PPC += lo;
      break;



    case 0x18:			/* CLC */
      _setC_ (0);
      break;



    case 0xd8:			/* CLD */
      _setD_ (0);
      break;



    case 0x58:			/* CLI */
      _setI_ (0);
      break;



    case 0xb8:			/* CLV */
      _setV_ (0);
      break;



    case 0xc9:			/* CMP #dd */
      d = _eaimm_ ();
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = A;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xc5:			/* CMP aa */
      d = MegaGetMem ((ADDR) _eazp_ ());
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = A;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xd5:			/* CMP aa,X */
      d = MegaGetMem ((ADDR) _eazpx_ ());
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = A;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xcd:			/* CMP aaaa */
      d = MegaGetMem (_eaabs_ ());
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = A;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xdd:			/* CMP aaaa,X */
      d = MegaGetMem (_eaabsx_ ());
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = A;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xd9:			/* CMP aaaa,Y */
      d = MegaGetMem (_eaabsy_ ());
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = A;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xc1:			/* CMP (aa,X) */
      d = MegaGetMem (_eaindx_ ());
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = A;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xd1:			/* CMP (aa),y */
      d = MegaGetMem (_eaindy_ ());
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = A;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;



    case 0xe0:			/* CPX #dd */
      d = _eaimm_ ();
      _setZ_ (X == d);
      _setC_ (X >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = X;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xe4:			/* CPX aa */
      d = MegaGetMem ((ADDR) _eazp_ ());
      _setZ_ (X == d);
      _setC_ (X >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = X;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xec:			/* CPX aaaa */
      d = MegaGetMem (_eaabs_ ());
      _setZ_ (X == d);
      _setC_ (X >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = X;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;



    case 0xc0:			/* CPY #dd */
      d = _eaimm_ ();
      _setZ_ (Y == d);
      _setC_ (Y >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = Y;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xc4:			/* CPY aa */
      d = MegaGetMem ((ADDR) _eazp_ ());
      _setZ_ (Y == d);
      _setC_ (Y >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = Y;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;

    case 0xcc:			/* CPY aaaa */
      d = MegaGetMem (_eaabs_ ());
      _setZ_ (Y == d);
      _setC_ (Y >= d);
      if (d >= 0x80)
	d -= 0x0100;
      lo = Y;
      if (lo >= 0x80)
	lo -= 0x0100;
      hi = lo - d;
      _setN_ ((hi >= 0x80) || (hi < 0));
      if (hi < -0x80)
	_setN_ (0);
      break;



    case 0xc6:			/* DEC aa */
      hi = _eazp_ ();
      d = MegaGetMem ((ADDR) hi) - 1;
      if (d < 0)
	d += 0x0100;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0xd6:			/* DEC aa,X */
      hi = _eazpx_ ();
      d = MegaGetMem ((ADDR) hi) - 1;
      if (d < 0)
	d += 0x0100;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0xce:			/* DEC aaaa */
      hi = _eaabs_ ();
      d = MegaGetMem ((ADDR) hi) - 1;
      if (d < 0)
	d += 0x0100;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0xde:			/* DEC aaaa,X */
      hi = _eaabs_ ();
      d = MegaGetMem ((ADDR) hi) - 1;
      if (d < 0)
	d += 0x0100;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;



    case 0xca:			/* DEX */
      X--;
      if (X < 0)
	X += 0x0100;
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;



    case 0x88:			/* DEY */
      Y--;
      if (Y < 0)
	Y += 0x0100;
      _setN_ (Y >= 0x80);
      _setZ_ (Y == 0);
      break;



    case 0x49:			/* EOR #dd */
      d = _eaimm_ ();
      A ^= d;
      A &= 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x45:			/* EOR aa */
      d = MegaGetMem ((ADDR) _eazp_ ());
      A ^= d;
      A &= 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x55:			/* EOR aa,X */
      d = MegaGetMem ((ADDR) _eazpx_ ());
      A ^= d;
      A &= 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x4d:			/* EOR aaaa */
      d = MegaGetMem (_eaabs_ ());
      A ^= d;
      A &= 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x5d:			/* EOR aaaa,X */
      d = MegaGetMem (_eaabsx_ ());
      A ^= d;
      A &= 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x59:			/* EOR aaaa,Y */
      d = MegaGetMem (_eaabsy_ ());
      A ^= d;
      A &= 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x41:			/* EOR (aa,X) */
      d = MegaGetMem (_eaindx_ ());
      A ^= d;
      A &= 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x51:			/* EOR (aa),Y */
      d = MegaGetMem (_eaindy_ ());
      A ^= d;
      A &= 0xff;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;



    case 0xe6:			/* INC aa */
      hi = _eazp_ ();
      d = MegaGetMem ((ADDR) hi) + 1;
      d &= 0xff;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0xf6:			/* INC aa,X */
      hi = _eazpx_ ();
      d = MegaGetMem ((ADDR) hi) + 1;
      d &= 0xff;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0xee:			/* INC aaaa */
      hi = _eaabs_ ();
      d = MegaGetMem ((ADDR) hi) + 1;
      d &= 0xff;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0xfe:			/* INC aaaa,X */
      hi = _eaabsx_ ();
      d = MegaGetMem ((ADDR) hi) + 1;
      d &= 0xff;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;



    case 0xe8:			/* INX */
      X++;
      X &= 0xff;
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;



    case 0xc8:			/* INY */
      Y++;
      Y &= 0xff;
      _setN_ (Y >= 0x80);
      _setZ_ (Y == 0);
      break;



    case 0x4c:			/* JMP aaaa */
      PPC = _eaabs_ ();
      break;

    case 0x6c:			/* JMP (aaaa) */
      PPC = _eaabsind_ ();
      break;



    case 0x20:			/* JSR aaaa */
      /* Push address of 3rd byte of jsr: */
      lo = (PPC + 1) & 65535;
      _push_ ((BYTE) (lo / 0x0100));
      _push_ ((BYTE) (lo % 0x0100));
      PPC = _eaabs_ ();
      break;



    case 0xa9:			/* LDA #dd */
      A = _eaimm_ ();
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0xa5:			/* LDA aa */
      A = MegaGetMem ((ADDR) _eazp_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0xb5:			/* LDA aa,X */
      A = MegaGetMem ((ADDR) _eazpx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0xad:			/* LDA aaaa */
      A = MegaGetMem (_eaabs_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0xbd:			/* LDA aaaa,X */
      A = MegaGetMem (_eaabsx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0xb9:			/* LDA aaaa,Y */
      A = MegaGetMem (_eaabsy_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0xa1:			/* LDA (aa,X) */
      A = MegaGetMem (_eaindx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0xb1:			/* LDA (aa),Y */
      A = MegaGetMem (_eaindy_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;



    case 0xa2:			/* LDX #dd */
      X = _eaimm_ ();
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;

    case 0xa6:			/* LDX aa */
      X = MegaGetMem ((ADDR) _eazp_ ());
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;

    case 0xb6:			/* LDX aa,Y */
      X = MegaGetMem ((ADDR) _eazpy_ ());
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;

    case 0xae:			/* LDX aaaa */
      X = MegaGetMem (_eaabs_ ());
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;

    case 0xbe:			/* LDX aaaa,Y */
      X = MegaGetMem (_eaabsy_ ());
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;



    case 0xa0:			/* LDY #dd */
      Y = _eaimm_ ();
      _setN_ (Y >= 0x80);
      _setZ_ (Y == 0);
      break;

    case 0xa4:			/* LDY aa */
      Y = MegaGetMem ((ADDR) _eazp_ ());
      _setN_ (Y >= 0x80);
      _setZ_ (Y == 0);
      break;

    case 0xb4:			/* LDY aa,X */
      Y = MegaGetMem ((ADDR) _eazpx_ ());
      _setN_ (Y >= 0x80);
      _setZ_ (Y == 0);
      break;

    case 0xac:			/* LDY aaaa */
      Y = MegaGetMem (_eaabs_ ());
      _setN_ (Y >= 0x80);
      _setZ_ (Y == 0);
      break;

    case 0xbc:			/* LDY aaaa,X */
      Y = MegaGetMem (_eaabsx_ ());
      _setN_ (Y >= 0x80);
      _setZ_ (Y == 0);
      break;



    case 0x4a:			/* LSR A */
      _setC_ (A & 1);
      A = A >> 1;
      _setN_ (0);
      _setZ_ (A == 0);
      break;

    case 0x46:			/* LSR aa */
      hi = _eazp_ ();
      d = MegaGetMem ((ADDR) hi);
      _setC_ (d & 1);
      d = d >> 1;
      _setN_ (0);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x56:			/* LSR aa,X */
      hi = _eazpx_ ();
      d = MegaGetMem ((ADDR) hi);
      _setC_ (d & 1);
      d = d >> 1;
      _setN_ (0);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x4e:			/* LSR aaaa */
      hi = _eaabs_ ();
      d = MegaGetMem ((ADDR) hi);
      _setC_ (d & 1);
      d = d >> 1;
      _setN_ (0);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x5e:			/* LSR aaaa,X */
      hi = _eaabsx_ ();
      d = MegaGetMem ((ADDR) hi);
      _setC_ (d & 1);
      d = d >> 1;
      _setN_ (0);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;



    case 0xea:			/* NOP */
    nop:
      break;



    case 0x09:			/* ORA #dd */
      A |= _eaimm_ ();
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x05:			/* ORA aa */
      A |= MegaGetMem ((ADDR) _eazp_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x15:			/* ORA aa,X */
      A |= MegaGetMem ((ADDR) _eazpx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x0d:			/* ORA aaaa */
      A |= MegaGetMem (_eaabs_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x1d:			/* ORA aaaa,X */
      A |= MegaGetMem (_eaabsx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x19:			/* ORA aaaa,Y */
      A |= MegaGetMem (_eaabsy_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x01:			/* ORA (aa,X) */
      A |= MegaGetMem (_eaindx_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x11:			/* ORA (aa),Y */
      A |= MegaGetMem (_eaindy_ ());
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;



    case 0x48:			/* PHA */
      _push_ ((BYTE) A);
      break;



    case 0x08:			/* PHP */
      _push_ ((BYTE) P);
      break;



    case 0x68:			/* PLA */
      A = _pull_ ();
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;



    case 0x28:			/* PLP */
      P = _pull_ ();
      P |= 0x30;
      break;



    case 0x2a:			/* ROL A */
      al = _getC_;
      _setC_ (A >= 0x80);
      A = (A << 1) & 0xff;
      A |= al;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x26:			/* ROL aa */
      hi = _eazp_ ();
      d = MegaGetMem ((ADDR) hi);
      al = _getC_;
      _setC_ (d >= 0x80);
      d = (d << 1) & 0xff;
      d |= al;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x36:			/* ROL aa,X */
      hi = _eazpx_ ();
      d = MegaGetMem ((ADDR) hi);
      al = _getC_;
      _setC_ (d >= 0x80);
      d = (d << 1) & 0xff;
      d |= al;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x2e:			/* ROL aaaa */
      hi = _eaabs_ ();
      d = MegaGetMem ((ADDR) hi);
      al = _getC_;
      _setC_ (d >= 0x80);
      d = (d << 1) & 0xff;
      d |= al;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x3e:			/* ROL aaaa,X */
      hi = _eaabsx_ ();
      d = MegaGetMem ((ADDR) hi);
      al = _getC_;
      _setC_ (d >= 0x80);
      d = (d << 1) & 0xff;
      d |= al;
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;



    case 0x6a:			/* ROR A */
      al = _getC_;
      _setC_ (A & 1);
      A = A >> 1;
      A |= (al * 0x80);
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

    case 0x66:			/* ROR aa */
      hi = _eazp_ ();
      d = MegaGetMem ((ADDR) hi);
      al = _getC_;
      _setC_ (d & 1);
      d = d >> 1;
      d |= (al * 0x80);
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x76:			/* ROR aa,X */
      hi = _eazpx_ ();
      d = MegaGetMem ((ADDR) hi);
      al = _getC_;
      _setC_ (d & 1);
      d = d >> 1;
      d |= (al * 0x80);
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x6e:			/* ROR aaaa */
      hi = _eaabs_ ();
      d = MegaGetMem ((ADDR) hi);
      al = _getC_;
      _setC_ (d & 1);
      d = d >> 1;
      d |= (al * 0x80);
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;

    case 0x7e:			/* ROR aaaa,X */
      hi = _eaabsx_ ();
      d = MegaGetMem ((ADDR) hi);
      al = _getC_;
      _setC_ (d & 1);
      d = d >> 1;
      d |= (al * 0x80);
      _setN_ (d >= 0x80);
      _setZ_ (d == 0);
      MegaPutMem ((ADDR) hi, (BYTE) d);
      break;



    case 0x40:			/* RTI */
      P = _pull_ () | 0x30;	/* fixing unused bit bug */
      lo = _pull_ ();
      hi = _pull_ ();
      PPC = lo + (hi * 0x0100);
      break;




    case 0x60:			/* RTS */
      lo = _pull_ ();
      hi = _pull_ ();
      PPC = 1 + lo + (hi * 0x0100);
      break;



    case 0xe9:			/* SBC #dd */
      d = _eaimm_ ();
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0xe5:			/* SBC aa */
      d = MegaGetMem ((ADDR) _eazp_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0xf5:			/* SBC aa,x */
      d = MegaGetMem ((ADDR) _eazpx_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0xed:			/* SBC aaaa */
      d = MegaGetMem (_eaabs_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0xfd:			/* SBC aaaa,X */
      d = MegaGetMem (_eaabsx_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0xf9:			/* SBC aaaa,Y */
      d = MegaGetMem (_eaabsy_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0xe1:			/* SBC (aa,X) */
      d = MegaGetMem (_eaindx_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0xf1:			/* SBC (aa),Y */
      d = MegaGetMem (_eaindy_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;



    case 0x38:			/* SEC */
      _setC_ (1);
      break;



    case 0xf8:			/* SED */
      _setD_ (1);
      break;



    case 0x78:			/* SEI */
      _setI_ (1);
      break;



    case 0x85:			/* STA aa */
      hi = _eazp_ ();
      MegaPutMem ((ADDR) hi, (BYTE) A);
      break;



    case 0x95:			/* STA aa,X */
      hi = _eazpx_ ();
      MegaPutMem ((ADDR) hi, (BYTE) A);
      break;



    case 0x8d:			/* STA aaaa */
      hi = _eaabs_ ();
      MegaPutMem ((ADDR) hi, (BYTE) A);
      break;



    case 0x9d:			/* STA aaaa,X */
      hi = _eaabsx_ ();
      MegaPutMem ((ADDR) hi, (BYTE) A);
      break;



    case 0x99:			/* STA aaaa,Y */
      hi = _eaabsy_ ();
      MegaPutMem ((ADDR) hi, (BYTE) A);
      break;



    case 0x81:			/* STA (aa,X) */
      hi = _eaindx_ ();
      MegaPutMem ((ADDR) hi, (BYTE) A);
      break;



    case 0x91:			/* STA (aa),Y */
      hi = _eaindy_ ();
      MegaPutMem ((ADDR) hi, (BYTE) A);
      break;



    case 0x86:			/* STX aa */
      hi = _eazp_ ();
      MegaPutMem ((ADDR) hi, (BYTE) X);
      break;



    case 0x96:			/* STX aa,Y */
      hi = _eazpy_ ();
      MegaPutMem ((ADDR) hi, (BYTE) X);
      break;



    case 0x8e:			/* STX aaaa */
      hi = _eaabs_ ();
      MegaPutMem ((ADDR) hi, (BYTE) X);
      break;



    case 0x84:			/* STY aa */
      hi = _eazp_ ();
      MegaPutMem ((ADDR) hi, (BYTE) Y);
      break;



    case 0x94:			/* STY aa,X */
      hi = _eazpx_ ();
      MegaPutMem ((ADDR) hi, (BYTE) Y);
      break;



    case 0x8c:			/* STY aaaa */
      hi = _eaabs_ ();
      MegaPutMem ((ADDR) hi, (BYTE) Y);
      break;



    case 0xaa:			/* TAX */
      X = A;
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;



    case 0xa8:			/* TAY */
      Y = A;
      _setN_ (Y >= 0x80);
      _setZ_ (Y == 0);
      break;



    case 0xba:			/* TSX */
      X = S;
      _setN_ (X >= 0x80);
      _setZ_ (X == 0);
      break;



    case 0x8a:			/* TXA */
      A = X;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;



    case 0x9a:			/* TXS */
      S = X;
      break;



    case 0x98:			/* TYA */
      A = Y;
      _setN_ (A >= 0x80);
      _setZ_ (A == 0);
      break;

#ifdef is65C02

/* new 65C02 opcodes */

    case 0x80:			/* BRA - */
      lo = _eaimm_ ();
      if (lo >= 128)
	lo -= 256;
      PPC += lo;
      break;

    case 0x12:			/* ORA (zp) - */
      A |= MegaGetMem (_eazpind_ ());
      _setN_ (A >= 128);
      _setZ_ (A == 0);
      break;

    case 0x32:			/* AND (zp) - */
      A &= MegaGetMem (_eazpind_ ());
      _setN_ (A >= 128);
      _setZ_ (A == 0);
      break;

    case 0x52:			/* EOR (zp) - */
      d = MegaGetMem (_eazpind_ ());
      A ^= d;
      A &= 255;
      _setN_ (A >= 128);
      _setZ_ (A == 0);
      break;

    case 0x72:			/* ADC (zp) - */
      d = MegaGetMem ((ADDR) _eazpind_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A = A + d + _getC_;
	  _setC_ (A > 0xff);
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al = al + _getC_ + ol;
	  if (al >= 10)
	    {
	      al -= 10;
	      ah++;
	    }
	  ah += oh;
	  _setC_ (ah >= 10);
	  if (ah >= 10)
	    ah -= 10;
	  A = al + (ah * 16);
	}
      _setV_ (((Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x92:			/* STA (zp) - */
      hi = _eazpind_ ();
      MegaPutMem (hi, A);
      break;

    case 0xb2:			/* LDA (zp) - */
      A = MegaGetMem (_eazpind_ ());
      _setN_ (A >= 128);
      _setZ_ (A == 0);
      break;

    case 0xd2:			/* CMP (zp) - */
      d = MegaGetMem (_eazpind_ ());
      _setZ_ (A == d);
      _setC_ (A >= d);
      if (d >= 128)
	d -= 256;
      lo = A;
      if (lo >= 128)
	lo -= 256;
      hi = lo - d;
      _setN_ ((hi >= 128) || (hi < 0));
      if (hi < -128)
	_setN_ (0);
      break;

    case 0xf2:			/* SBC (zp) - */
      d = MegaGetMem ((ADDR) _eazpind_ ());
      Vtmp = d ^ A;
      if (!_getD_)
	{
	  A -= d;
	  A -= !_getC_;
	  _setC_ (!(A < 0));
	}
      else
	{
	  ah = A / 16;
	  al = A % 16;
	  oh = d / 16;
	  ol = d % 16;
	  al -= !_getC_;
	  al -= ol;
	  if (al < 0)
	    {
	      al += 10;
	      ah--;
	    }
	  ah -= oh;
	  _setC_ (!(ah < 0));
	  if (ah < 0)
	    ah += 10;
	  A = al + (ah * 16);
	}
      _setV_ (((0x80 ^ Vtmp ^ A ^ (_getC_ * 255)) & 0x80));
      if (A < 0)
	A += 0x0100;
      A %= 0x0100;
      _setZ_ (A == 0);
      _setN_ (A >= 0x80);
      break;

    case 0x04:			/* TSB zp - */
      lo = _eazp_ ();
      d = MegaGetMem (lo);
      _setN_ (d >= 0x80);	/* set flags like BIT */
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      d |= A;			/* and set bits... */
      MegaPutMem ((ADDR) lo, (BYTE) d);
      break;

    case 0x14:			/* TRB zp - */
      lo = _eazp_ ();
      d = MegaGetMem (lo);
      _setN_ (d >= 0x80);	/* set flags like BIT */
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      d &= (~A);		/* and reset bits... */
      MegaPutMem ((ADDR) lo, (BYTE) d);
      break;

    case 0x34:			/* BIT zp,X - */
      d = MegaGetMem (_eazpx_ ());
      _setN_ (d >= 128);
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      break;

    case 0x89:			/* BIT #imm - */
      d = _eaimm_ ();
      _setN_ (d >= 128);
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      break;

    case 0x64:			/* STZ zp - */
      hi = _eazp_ ();
      MegaPutMem (hi, 0);
      break;

    case 0x74:			/* STZ zp,X - */
      hi = _eazpx_ ();
      MegaPutMem (hi, 0);
      break;

    case 0x1a:			/* INA - */
      A++;
      A &= 255;
      _setN_ (A >= 128);
      _setZ_ (A == 0);
      break;

    case 0x3a:			/* DEA - */
      A--;
      A &= 255;
      _setN_ (A >= 128);
      _setZ_ (A == 0);
      break;

    case 0x5a:			/* PHY - */
      _push_ (Y);
      break;

    case 0x7a:			/* PLY - */
      Y = _pull_ ();
      break;

    case 0xda:			/* PHX - */
      _push_ (X);
      break;

    case 0xfa:			/* PLX - */
      X = _pull_ ();
      break;

    case 0x0c:			/* TSB abs - */
      lo = _eaabs_ ();
      d = MegaGetMem (lo);
      _setN_ (d >= 0x80);	/* set flags like BIT */
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      d |= A;			/* and set bits... */
      MegaPutMem ((ADDR) lo, (BYTE) d);
      break;

    case 0x1c:			/* TRB abs - */
      lo = _eaabs_ ();
      d = MegaGetMem (lo);
      _setN_ (d >= 0x80);	/* set flags like BIT */
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      d &= (~A);		/* and reset bits... */
      MegaPutMem ((ADDR) lo, (BYTE) d);
      break;

    case 0x3c:			/* BIT abs,X - */
      d = MegaGetMem (_eaabsx_ ());
      _setN_ (d >= 128);
      _setV_ ((d & 64) != 0);
      _setZ_ ((d & A) == 0);
      break;

    case 0x7c:			/* JMP (abs,X) - */
      PPC = _eaabsindx_ ();
      break;

    case 0x9c:			/* STZ abs - */
      hi = _eaabs_ ();
      MegaPutMem (hi, 0);
      break;

    case 0x9e:			/* STZ abs,X - */
      hi = _eaabsx_ ();
      MegaPutMem (hi, 0);
      break;

#endif


    default:			/* Undefined opcode */
      goto nop;
      break;
    }
}
