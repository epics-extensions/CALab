@echo off
:: set EPICS enviroment variables
set EPICS_CA_ADDR_LIST=localhost
set EPICS_CA_AUTO_ADDR_LIST=NO

:: start ioc shell
cd /d %APPDATA%\calab
cd ..\demo
start TestPV_ai100000.cmd
