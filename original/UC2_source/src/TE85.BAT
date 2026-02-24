@echo off
del tst?.*

uc a -t5 tst5 8\*.*
pkzipa -a -ex tst3 8\*.*
hpacka a tst 8\*

dir tst?.*
