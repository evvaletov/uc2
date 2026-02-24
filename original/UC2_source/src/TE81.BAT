@echo off
del tst?.*

uc a -t1 tst1 8\*.*
pkzipa -a -es tst1 8\*.*

dir tst?.*
del tst?.*
