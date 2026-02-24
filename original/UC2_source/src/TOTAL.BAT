@echo off
echo c:\uc\docs\uk\ucdocs     dist
echo c:\uc\docs\uk\crypsafe   dist
echo c:\uc\docs\uk\market     dist
echo c:\uc\tools\stealth      m
echo c:\uc\source             ucbinst
echo c:\uc\tools\useusa       makeall
echo c:\uc\tools\ucrypt       m
echo c:\uc\source             makeuu
echo c:\uc\source             final
pause
c:
echo y | del \uc\dist\*.*
echo y | del \uc\distuu\*.*
echo y | del \uc\final\*.*
cd \uc\docs\uk\ucdocs
call dist
cd \uc\docs\uk\crypsafe
call dist
cd \uc\docs\uk\market
call dist
cd \uc\tools\stealth
call m
cd \uc\source
call ucbinst
cd \uc\tools\useusa
call makeall
cd \uc\tools\ucrypt
call m
cd \uc\source
call makeuu
cd \uc\source
call final
