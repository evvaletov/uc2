copy c:\uc\source\uc2ins.exe c:\uc\final
copy c:\uc\source\crysaf.uc2 c:\uc\final
if q%1q==q/nq goto skip
set ALLOW_ALL_IN_USEAL=TRUE
usealaip c:\uc\final\uc2ins.exe
set ALLOW_ALL_IN_USEAL=
usealaip c:\uc\final\crysaf.uc2
:skip
fd \uc\final\*.* /d01-01-94 /t2:00:00
echo Resulting UC2INS.EXE and CRYSAF.UC2 are in C:\UC\FINAL
