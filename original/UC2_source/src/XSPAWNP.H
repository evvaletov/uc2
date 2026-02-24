/*
 *                   XSPAWN
 *                Version 1.32
 *  (C) Copyright 1990 Whitney Software, Inc.
 *             All Rights Reserved
 */

#define P_WAIT   0
#define CURRENT  0
#define IRET     1

extern int xspawnl( int, char *, char * );
extern int xspawnle( int, char *, char * );
extern int xspawnlp( int, char *, char * );
extern int xspawnlpe( int, char *, char * );
extern int xspawnv( int, char *, char ** );
extern int xspawnve( int, char *, char **, char ** );
extern int xspawnvp( int, char *, char ** );
extern int xspawnvpe( int, char *, char **, char ** );
extern int xsystem( char * );

extern int addvect( int, int );
