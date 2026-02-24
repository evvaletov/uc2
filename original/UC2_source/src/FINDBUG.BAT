@echo off

REM enable 'hidden' beta features
SET BETA=ON

REM enable torture mode
SET HEAVY=ON

REM ***********************************
REM ** DELETE ALL LOCAL *.UC2 FILES! **
REM ***********************************
DEL *.UC2

UCI A -S -TT NORM_T \tmp\*.*
UCI A -S -TF NORM_F \tmp\*.*
UCI A -S -TM NORM_M \tmp\*.*

REM disable huffman compression
SET NOTREE=ON
UCI A -S -TT NOTR_TXX \tmp\*.*
UCI A -S -TF NOTR_FXX \tmp\*.*
UCI A -S -TM NOTR_MXX \tmp\*.*
SET NOTREE=

REM disable advanced match searching (default in #003)
SET NOMALEN=ON
UCI A -S -TT NOML_TYY \tmp\*.*
UCI A -S -TF NOML_FYY \tmp\*.*
UCI A -S -TM NOML_MYY \tmp\*.*
SET NOMALEN=

REM disable LRU general master caching
SET NOCLRUMAS=ON
UCI A -S -TT CLRU_T \tmp\*.*
UCI A -S -TF CLRU_F \tmp\*.*
UCI A -S -TM CLRU_M \tmp\*.*
SET NOCLRUMAS=

REM disable super master caching
SET NOCMASST=ON
UCI A -S -TT CMST_T \tmp\*.*
UCI A -S -TF CMST_F \tmp\*.*
UCI A -S -TM CMST_M \tmp\*.*
SET NOCMASST=

REM disable hash table caching
SET NOCHASH=ON
UCI A -S -TT CHAS_T \tmp\*.*
UCI A -S -TF CHAS_F \tmp\*.*
UCI A -S -TM CHAS_M \tmp\*.*
SET NOCHASH=

REM disable general master caching
SET NOCMAST=ON
UCI A -S -TT CMAS_T \tmp\*.*
UCI A -S -TF CMAS_F \tmp\*.*
UCI A -S -TM CMAS_M \tmp\*.*
SET NOCMAST=

REM disable disk caching
SET NOCDISK=ON
UCI A -S -TT CDSK_T \tmp\*.*
UCI A -S -TF CDSK_F \tmp\*.*
UCI A -S -TM CDSK_M \tmp\*.*
SET NOCDISK=

REM disable virtual memory caching
SET NOCVMEM=ON
UCI A -S -TT CVMM_T \tmp\*.*
UCI A -S -TF CVMM_F \tmp\*.*
UCI A -S -TM CVMM_M \tmp\*.*
SET NOCVMEM=

REM disable all forms of caching
SET NOCLRUMAS=ON
SET NOCMASST=ON
SET NOCHASH=ON
SET NOCMAST=ON
SET NOCDISK=ON
SET NOCVMEM=ON
UCI A -S -TT CASH_T \tmp\*.*
UCI A -S -TF CASH_F \tmp\*.*
UCI A -S -TM CASH_M \tmp\*.*
SET NOCLRUMAS=
SET NOCMASST=
SET NOCHASH=
SET NOCMAST=
SET NOCDISK=
SET NOCVMEM=

SET BETA=
SET HEAVY=

DIR *.UC2 > LOG

TYPE LOG
