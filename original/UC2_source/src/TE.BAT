@echo off
del tst.*

uc a -t3 tst %1 %2 %3 %4
pkzipa -a -ex tst %1 %2 %3 %4

dir tst.*
del tst.*
