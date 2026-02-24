       @echo off
       echo q > U$~CHK1
       UC004 a %1 U$~CHK1
       del U$~CHK1
       UC004 xs %1 *.* U$~CHK1
       echo q > U$~CHK2
