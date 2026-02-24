@echo off
del tst?.*

uc a -t4 tst4 8\*.*
pkzipa -a -ex tst3 8\*.*

dir tst?.*
