@echo off
:: set EPICS enviroment variables
set EPICS_CA_ADDR_LIST=localhost
set EPICS_CA_AUTO_ADDR_LIST=NO

:: start ioc shell
cd /d %APPDATA%\calab\demo
start DemoIOC.cmd

:: read values with EPICS BASE tool "camonitor"
cd /d %APPDATA%\calab
start camonitor caLab:aiExample caLab:double1 caLab:double2 caLab:bi caLab:long caLab:mbbi caLab:stringin caLab:string caLab:waveDouble caLab:waveLong caLab:waveString

:: wait 5 seconds for initialisation
ping 123.45.67.89 -n 1 -w 5000 > nul

:: write some values with EPICS BASE tool "caput"
caput caLab:double1 3.141
caput caLab:double2 2.718
caput caLab:bi 1
caput caLab:long 12345
caput caLab:mbbi 0
caput caLab:stringin "EPICS is great"
caput caLab:string "try CA Lab for your project"
caput -a caLab:waveDouble 6 1.2 2.4 3.6 4.8 5.1 6.12
caput -a caLab:waveLong 8 71828 18284 59045 23536 2874 71352 66249 77572
caput -a caLab:waveString 8 It's easy to access EPICS variables via "CA Lab". It's fast! It's free! Try it!

:: read values with LabVIEW
cd /d %APPDATA%\calab\Examples
"Read Demo 2.vi"
