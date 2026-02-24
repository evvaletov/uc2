@echo off
del tst?.*

uc a -t2 tst2 8\*.*
pkzipa -a -en tst2 8\*.*

dir tst?.*
