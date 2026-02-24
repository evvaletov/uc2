/*
 *                   XSPAWN
 *                Version 1.32
 *  (C) Copyright 1990 Whitney Software, Inc.
 *             All Rights Reserved
 */

#include "main.h"
#undef malloc
#undef free

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifdef LATTICE
#include <dos.h>
#else
#include <io.h>
#endif
#include "xspawnp.h"

typedef struct _vector {
   char number;                       /* vector number */
   char flag;                         /* 0-CURRENT, 1-IRET, 2-free, 3-end */
   unsigned int vseg;                 /* vector segment */
   unsigned int voff;                 /* vector offset */
} VECTOR;

static void savevect(void);
static int testfile(char *, char *, int *);
static int tempfile(char *, int *);
static int cmdenv(char **, char **, char *, char **, char **);
static int doxspawn(char *, char **, char **);
#ifdef LATTICE
extern int __xspawn(char *, char *, char *, VECTOR *, int, int, char *, int);
extern int __xsize(unsigned int, long *, long *);
extern int __chkems(char *, int *);
extern int __savemap(char *);
extern int __restmap(char *);
extern int __getems(int, int *);
extern int __dskspace(int, unsigned int *, unsigned int *);
extern int __getrc(void);
extern int __create(char *, int *);
extern int __getcd(int, char *);
extern int __getdrv(void);
extern void __getvect(int, unsigned int *, unsigned int *);
extern void __setvect(VECTOR *);
#else
extern int _xspawn(char *, char *, char *, VECTOR *, int, int, char *, int);
extern int _xsize(unsigned int, long *, long *);
extern int _chkems(char *, int *);
extern int _savemap(char *);
extern int _restmap(char *);
extern int _getems(int, int *);
extern int _dskspace(int, unsigned int *, unsigned int *);
extern int _getrc(void);
extern int _create(char *, int *);
extern int _getcd(int, char *);
extern int _getdrv(void);
extern void _getvect(int, unsigned int *, unsigned int *);
extern void _setvect(VECTOR *);
#endif

#ifdef LATTICE
extern char **environ;
#endif

int _swap = 0;                         /* if 0, do swap */
char *_swappath = NULL;                /* swap path */
int _useems = 0;                       /* if 0, use EMS */
int _required = 0;                     /* child memory requirement in K */
static long swapsize;                  /* swap size requirement in bytes */
static int ems = 2;                    /* if 0, EMS is available */
static int mapsize;                    /* size of page map information */
static unsigned int tempno = 1;        /* tempfile number */
static char errtab[] =                 /* error table */
{
    0,
    EINVAL,
    ENOENT,
    ENOENT,
    EMFILE,
    EACCES,
    EBADF,
    ENOMEM,
    ENOMEM,
    ENOMEM,
    E2BIG,
    ENOEXEC,
    EINVAL,
    EINVAL,
    -1,
    EXDEV,
    EACCES,
    EXDEV,
    ENOENT,
    -1
};


static VECTOR vectab1[] =
{
    {0,    1,     0,  0},
    {1,    1,     0,  0},
    {2,    1,     0,  0},
    {3,    1,     0,  0},
    {0x1B, 1,     0,  0},
    {0x23, 1,     0,  0},
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    2,     0,  0},                /* free record */
    {0,    3,     0,  0}                 /* end record */
};
static VECTOR vectab2[sizeof vectab1 / sizeof vectab1[0]];

int addvect(number,opcode)
int number;
int opcode;
{
   register VECTOR *vect = vectab1;

   if (number < 0 || number > 0xFF ||
       (opcode != IRET && opcode != CURRENT))
   {
      errno = EINVAL;
      return(-1);
   }

   /* see if number is already in table */
   while (vect->flag != 3 && (vect->flag == 2 ||
          vect->number != (char)number))
   {
      vect++;
   }

   if ( vect->flag == 3 )
   {
      /* look for a free record */
      vect = vectab1;
      while (vect->flag == CURRENT || vect->flag == IRET)
         vect++;
   }

   if (vect->flag != 3)
   {
      vect->number = (char)number;
      vect->flag = (char)opcode;
      if (opcode == CURRENT)
#ifdef LATTICE
         __getvect(number,&vect->vseg,&vect->voff);
#else
         _getvect(number,&vect->vseg,&vect->voff);
#endif
      return(0);
   }

   errno = ENOMEM;
   return(-1);
}

static void savevect()
{
   register VECTOR *vect1 = vectab1;
   register VECTOR *vect2 = vectab2;

   while (vect1->flag != 3)
   {
      if (vect1->flag != 2)
      {
         vect2->number = vect1->number;
         vect2->flag = CURRENT;
#ifdef LATTICE
         __getvect(vect1->number,&vect2->vseg,&vect2->voff);
#else
         _getvect(vect1->number,&vect2->vseg,&vect2->voff);
#endif
      }
      else
         vect2->flag = 2;             /* free */
      vect1++;
      vect2++;
   }
   vect2->flag = 3;                   /* end */
}

static int testfile( p, file, handle )
register char *p;
register char *file;
int *handle;
{
    unsigned int startno = tempno;
    int drive = ( *file | 32 ) - 96;   /* a = 1, b = 2, etc. */
    int root;
    unsigned int bytes;                /* bytes per cluster */
    unsigned int clusters;             /* free clusters */
    int need;                          /* clusters needed for swap file */
    int rc;                            /* return code */

    if ( file + 2 == p )
    {
        *p++ = '\\';
#ifdef LATTICE
        if ( __getcd( drive, p ))      /* get current directory */
#else
        if ( _getcd( drive, p ))       /* get current directory */
#endif
            return( 1 );               /* invalid drive */
        p = file + strlen( file );
    }
    else
    {
        *p = '\0';
        if ( access( file, 0 ))
            return( 1 );               /* path does not exist */
    }
    if ( *( p - 1 ) != '\\' && *( p - 1 ) != '/' )
        *p++ = '\\';
    if ( p - file == 3 )
        root = 1;                      /* is root directory */
    else
        root = 0;                      /* is not root directory */
    strcpy( p, TMPPREF"SWP" );
    p += strlen (p);

#ifdef LATTICE
    if ( __dskspace( drive, &bytes, &clusters ) != 0 )
#else
    if ( _dskspace( drive, &bytes, &clusters ) != 0 )
#endif
        return( 1 );                   /* invalid drive */
    need = ( int )( swapsize / bytes );
    if ( swapsize % bytes )
        need++;
    if ( root == 0 )                   /* if subdirectory */
        need++;                        /* in case the directory needs space */
    if ( clusters < ( unsigned int )need )
        return( 1 );                   /* insufficient free disk space */

    do
    {
again:  tempno = ( ++tempno ) ? tempno : 1;
        if ( tempno == startno )
            return( 1 );               /* extremely unlikely */
#ifdef LATTICE
        stcu_d( p, tempno );
#else
        ltoa(( long )tempno, p, 10 );
#endif
    }
    while ( !access( file, 0 ));

/*
 *  The return code from _create will equal 80 if the user is running DOS 3.0
 *  or above and the file was created by another program between the access
 *  call and the _create call.
 */

#ifdef LATTICE
    if (( rc = __create( file, handle )) == 80 )
#else
    if (( rc = _create( file, handle )) == 80 )
#endif
        goto again;
    return( rc );
}

static int tempfile( file, handle )
char *file;
int *handle;
{
    register char *s = _swappath;
    register char *p = file;

    if ( s )
    {
        for ( ;; s++ )
        {
            while ( *s && *s != ';' )
                *p++ = *s++;
            if ( p > file )
            {
                if ( p == file + 1 || file[ 1 ] != ':' )
                {
#ifdef MSC4
                    memcpy( file + 2, file, ( int )( p - file ));
#else
                    memmove( file + 2, file, ( int )( p - file ));
#endif
#ifdef LATTICE
                    *file = ( char )( __getdrv() + 'a' );
#else
                    *file = ( char )( _getdrv() + 'a' );
#endif
                    file[ 1 ] = ':';
                    p += 2;
                }
                if ( testfile( p, file, handle ) == 0 )
                    return( 0 );
                p = file;
            }
            if ( *s == '\0' )
                break;
        }
    }
    else                               /* try the current directory */
    {
#ifdef LATTICE
        *p++ = ( char )( __getdrv() + 'a' );
#else
        *p++ = ( char )( _getdrv() + 'a' );
#endif
        *p++ = ':';
        if ( testfile( p, file, handle ) == 0 )
            return( 0 );
    }

    errno = EACCES;
    return( 1 );
}

static int cmdenv( argv, envp, command, env, memory )
char **argv;
char **envp;
char *command;
char **env;
char **memory;
{
    register char **vp;
    unsigned int elen = 0;             /* environment length */
    char *p;
    int cnt;
    int len;

    /* construct environment */

    if ( envp == NULL )
        envp = environ;

    if ( envp )
    {
        for ( vp = envp; *vp; vp++ )
        {
            elen += strlen( *vp ) + 1;
            if ( elen > 32766 )        /* 32K - 2 */
            {
                errno = E2BIG;
                return( -1 );
            }
        }
    }

    if (( p = malloc( ++elen + 15 )) == NULL )
    {
        errno = ENOMEM;
        return( -1 );
    }
    *memory = p;

    *( unsigned int * )&p = *( unsigned int * )&p + 15 & ~15;
    *env = p;

    if ( envp )
    {
        for ( vp = envp; *vp; vp++ )
            p = strchr( strcpy( p, *vp ), '\0' ) + 1;
    }

    *p = '\0';                         /* final element */

    /* construct command-line */

    vp = argv;
    p = command + 1;
    cnt = 0;

    if ( *vp )
    {
        while ( *++vp )
        {
            *p++ = ' ';
            cnt++;
            len = strlen( *vp );
            if ( cnt + len > 125 )
            {
                errno = E2BIG;
                free( *memory );
                return( -1 );
            }
            strcpy( p, *vp );
            p += len;
            cnt += len;
        }
    }

    *p = '\r';
    *command = ( char )cnt;

    return(( int )elen );              /* return environment length */
}

static int doxspawn( path, argv, envp )
char *path;                      /* file to be executed */
char *argv[];                    /* array of pointers to arguments */
char *envp[];                    /* array of pointers to environment strings */
{
    register int rc = 0;               /* assume do xspawn */
    int doswap = 0;                    /* assume do swap */
    int elen;                          /* environment length */
    char *memory;
    char *env;                         /* environment */
    char command[ 128 ];               /* command-line */
    long totalsize;                    /* parent and free memory in bytes */
    int handle;
    int pages;
    char file[ 79 ];
    char *mapbuf = NULL;               /* buffer for map information */

    /* construct the command-line and the environment */
    if (( elen = cmdenv( argv, envp, command, &env, &memory )) == -1 )
        return( -1 );

    if ( _swap == 0 )
    {
        if ( _useems == 0 )
        {
            if ( ems == 2 )
#ifdef LATTICE
                ems = __chkems( "EMMXXXX0", &mapsize );
#else
                ems = _chkems( "EMMXXXX0", &mapsize );
#endif
            if ( ems == 0 && ( mapbuf = malloc( mapsize )) == NULL )
            {
                errno = ENOMEM;
                free( memory );
                return( -1 );
            }
        }
#ifdef LATTICE
        if (( rc = __xsize(( unsigned int )(( unsigned long )_PSP >> 16 ),
            &swapsize, &totalsize )) == 0 )
#else
        if (( rc = _xsize( _psp, &swapsize, &totalsize )) == 0 )
#endif
        {
            if ( _required == 0 ||
                totalsize - swapsize - 272 < (( long )_required << 10 ))
            {
                if ( ems == 0 && _useems == 0 )
                {
                    pages = ( int )( swapsize >> 14 );
                    if ((( long )pages << 14 ) < swapsize )
                        pages++;
#ifdef LATTICE
                    if ( __savemap( mapbuf ) == 0 &&
                        __getems( pages, &handle ) == 0 )
#else
                    if ( _savemap( mapbuf ) == 0 &&
                        _getems( pages, &handle ) == 0 )
#endif
                        *file = '\0';  /* use EMS */
                    else if ( tempfile( file, &handle ) != 0 )
                        rc = -1;       /* don't do xspawn */
                }
                else if ( tempfile( file, &handle ) != 0 )
                    rc = -1;           /* don't do xspawn */
            }
            else
                doswap = 1;            /* don't do swap */
        }
        else
        {
            errno = errtab[ rc ];
            rc = -1;                   /* don't do xspawn */
        }
    }
    else
        doswap = 1;                    /* don't do swap */

    if ( rc == 0 )
    {
        savevect();                    /* save current vectors */
#ifdef LATTICE
        rc = __xspawn( path, command, env, vectab1, doswap, elen, file,
            handle );
#else
        rc = _xspawn( path, command, env, vectab1, doswap, elen, file,
            handle );
#endif
#ifdef LATTICE
        __setvect( vectab2 );          /* restore saved vectors */
#else
        _setvect( vectab2 );           /* restore saved vectors */
#endif
        if ( rc == 0 )
#ifdef LATTICE
            rc = __getrc();            /* get child return code */
#else
            rc = _getrc();             /* get child return code */
#endif
        else
        {
            errno = errtab[ rc ];
            rc = -1;
        }
        /*
         *  If EMS was used, restore the page-mapping state of the expanded
         *  memory hardware.
         */
#ifdef LATTICE
        if ( doswap == 0 && *file == '\0' && __restmap( mapbuf ) != 0 )
#else
        if ( doswap == 0 && *file == '\0' && _restmap( mapbuf ) != 0 )
#endif
        {
            errno = EACCES;
            rc = -1;
        }
    }

    if ( mapbuf )
        free( mapbuf );
    free( memory );
    return( rc );
}

int xspawnve( modeflag, path, argv, envp )
int modeflag;                    /* execution mode for parent process */
register char *path;             /* file to be executed */
char *argv[];                    /* array of pointers to arguments */
char *envp[];                    /* array of pointers to environment strings */
{
    register char *p;
    char *s;
    int rc = -1;
    char buf[ 80 ];

    if ( modeflag != P_WAIT )
    {
        errno = EINVAL;
        return( -1 );
    }

    p = strrchr( path, '\\' );
    s = strrchr( path, '/' );
    if ( p == NULL && s == NULL )
        p = path;
    else if ( p == NULL || s > p )
        p = s;

    if ( strchr( p, '.' ))
    {
        if ( !access( path, 0 ))
            rc = doxspawn( path, argv, envp );
        /* If file not found, access will have set errno to ENOENT. */
    }
    else
    {
        strcpy( buf, path );
        strcat( buf, ".com" );
        if ( !access( buf, 0 ))
            rc = doxspawn( buf, argv, envp );
        else
        {
            strcpy( strrchr( buf, '.' ), ".exe" );
            if ( !access( buf, 0 ))
                rc = doxspawn( buf, argv, envp );
            /* If file not found, access will have set errno to ENOENT. */
        }
    }

    return( rc );
}
