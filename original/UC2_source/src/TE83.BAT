@echo off
del tst?.*

uc a -t3 tst3 8\*.*
pkzipa -a -ex tst3 8\*.*

dir tst?.*
del tst?.*
