// Copyright 1992, all rights reserved, AIP, Nico de Vries
// BITIO.H

void _fastcall InitBitsOut (void);
void _fastcall FlushBitsOut (void);

void _fastcall PUTBITS (WORD val, BYTE len);



void _fastcall InitBitsIn (WORD *spec);
void _fastcall ExitBitsIn ();

WORD _fastcall GETBITS (BYTE len);

WORD _fastcall SNOOP13 (void);
void _fastcall SKIP (BYTE len);