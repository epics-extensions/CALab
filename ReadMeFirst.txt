######################################################################
### CA Lab ReadMe for Windows® OS
######################################################################

----------------------------------------------------------------------
--- 1. General information
----------------------------------------------------------------------

In some cases it's necessary to restart computer because environment
variables have been changed by setup.
Please read chapter 3.1 if there are any warnings for environment variables.

Please check whether the specific user has full access to folder and files
of CA Lab before first run. You find the CA Lab folder in user.lib of
your LabVIEW™ installation.

At first run of any caLab-VI some warnings will appear because there is a 
new context. Please save changes therewith warnings won't appear again.
A better way is described in chapter 2 (mass compiling VIs).

Errors will occur if CA Lab has not been installed to standard path of LabVIEW™.
The reason is the wrong path for installed libraries.
Read chapter 3.2 to solve the problem with needed libraries for several VIs.

If setup created a start menu shortcut for CA Lab then it will appear as a
sub-entry of National Instruments. This sub-entry contains "CA Lab project"
which directs to installed VIs.

----------------------------------------------------------------------
--- 2. First using of CA Lab
----------------------------------------------------------------------

Mass compiling VIs
1. Select Tools->Advanced->Mass Compile
2. Choose CA Lab directory in your\Path\to\LabVIEW\user.lib\caLab
   and click the "Select" button
3. Click the "Mass Compile" button
4. Finish with "Done" button

Start your first test, by choosing "startDemo" from Start:
    Programs->National Instruments->caLab->"start Demo"
This will do following:
1. Start a demo IOC Shell with prepared test variables
2. Write random values to those variables
3. Monitors those variables
4. Shows currently used EPICS context

----------------------------------------------------------------------
--- 3. Solved problems
----------------------------------------------------------------------

--- 3.1 Environment variables for CA Lab

Environment variables need to be set for EPICS applications.
Please read this document:
             http://www.aps.anl.gov/epics/base/R3-14/1-docs/CAref.html
Setup tries to configure "CATOOLS".
Setup will not overwrite existing variables.
If any variable exists a warning will appear. In this case you should check
CATOOLS consists a path to library folder of ca Lab. This should contain
ca.dll, caLab.dll and com.dll.
e.g. "C:\Program Files\National Instruments\LabVIEW 2012\user.lib\caLab\lib".
It's also necessary to extend the environment variable PATH with %CATOOLS%.



--- 3.2 Problems to execute VIs

All provided VIs depend on correct set absolute path to "caLab.dll", "ca.dll"
and "com.dll". This libraries are in subfolder "lib". You can navigate from
Start->Programs->National Instruments->caLab->"CA Lab project" to file "path.txt"
to get the right path.
This file contains the needed path for "CaLabGet.vi", "CaLabPut.vi", ...

--- 3.3 Problems to create an executable with the Application Builder

If you have problems to create a working executable with the Application Builder
change target folder from user.lib to another target (sometimes only root works).
Following libraries and services are needed by CA Lab: caLab.dll, ca.dll, Com.dll,
caRepeater.exe and https://aka.ms/vs/17/release/vc_redist.x86.exe (32bit)
OR https://aka.ms/vs/17/release/vc_redist.x64.exe (64bit).
Environment variable PATH needs to be set to this files.
If you have problems with dependencies you should install CA Lab at target computer.

--- 3.4 Problems after installing new CA Lab version

If you upgrade existing VIs with CA Lab please check for greyed out CA Lab VIs.
Click right on greyed out CA Lab VIs and use "Relink to SubVI".
Watch out! LabVIEW™ may result in a misalignment of connectors!!
