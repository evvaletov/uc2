@echo off
PATH c:\win;C:\Borlandc\BIN;C:\TOOLS\BAT;C:\QEMM;C:\TOOLS\NORT60;C:\TOOLS\MSDOS;C:\TOOLS\UTILS;C:\STACKER
ncache /usehigh=on /optimize=s /ext=3000 /read=17 /quick=on
rem c:\qemm\loadhi /r:2 adcache -w0.5 -a3000 -i -r+
image
c:\qemm\loadhi /r:2 nde
rem c:\qemm\loadhi /r:1 dmp /mxp /lpt1:99
c:\qemm\loadhi /r:1 mouse
c:\qemm\loadhi /r:1 buffers=40
c:\qemm\loadhi /r:1 lastdrive L
loadfix c:\cpav\bootsafe
del c:\$pspool.* > nul
mode con: rate=30 delay=3
call b
SET WP= /ne /nx
SET PCPLUS=D:\PCP\
set TEMP=c:\tmp
set INCLUDE=%include%;c:\zortech\include
set LIB=%lib%;c:\zortech\lib
set PATH=c:\zortech\bin;%PATH%
set clipper=f45
rem c:\gv\gvload c:\uc\mem\world
call p
call f
cls
aip
