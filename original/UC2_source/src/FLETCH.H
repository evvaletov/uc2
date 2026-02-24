// FLETCH.H

struct FREC {
   WORD sum1, sum2;
};

void FletchInit (struct FREC *fr); // initialize
void FletchUpdate (struct FREC *fr, BYTE far *dptr, unsigned len);
    // update with data block
WORD Fletcher (FREC *fr); // get actual fletcher value

extern FREC Fin, Fout;
