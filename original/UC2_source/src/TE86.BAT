@echo off
del tst?.*

uc a -t6 tst6 8\*.*
pkzipa -a -ex tst3 8\*.*

dir tst?.*
