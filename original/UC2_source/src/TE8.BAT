@echo off
del tst?.*

uc a -t1 tst1 8\*.*
pkzipa -a -es tst1 8\*.*
uc a -t2 tst2 8\*.*
pkzipa -a -en tst2 8\*.*
uc a -t3 tst3 8\*.*
pkzipa -a -ex tst3 8\*.*
uc a -t4 tst4 8\*.*
uc a -t5 tst5 8\*.*

dir tst?.*
