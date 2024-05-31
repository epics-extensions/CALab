######################################################################
### CA Lab ReadMe for Windows® OS
######################################################################

----------------------------------------------------------------------
--- 1. General Information
----------------------------------------------------------------------

Before you start, please follow the instructions in "post_install.html".

When you run any caLab-VI for the first time, you may see some warnings
due to a new context. To prevent these warnings from appearing again,
save the changes.
A more efficient method is described in Chapter 2 (Mass Compiling VIs).

If CA Lab is not installed in the standard path of LabVIEW™,
errors may occur due to incorrect paths for installed libraries.
To resolve this issue, refer to Chapter 3.2.

If the setup created a start menu shortcut for CA Lab, it will appear
as a sub-entry of National Instruments. This sub-entry contains
"CA Lab project", which directs to the installed VIs.

----------------------------------------------------------------------
--- 2. First Use of CA Lab
----------------------------------------------------------------------

Mass Compiling VIs:
1. Select Tools->Advanced->Mass Compile...
2. Choose the CA Lab directory in %APPDATA%\caLab and click the "Select" button.
3. Click the "Mass Compile" button.
4. Finish with the "Done" button.

To start your first test, choose "startDemo" from Start:
    Programs->National Instruments->caLab->"start Demo"
This will do the following:
1. Start a demo IOC Shell with prepared test variables.
2. Write random values to those variables.
3. Monitor those variables.
4. Show the currently used EPICS context.

----------------------------------------------------------------------
--- 3. Resolved Issues
----------------------------------------------------------------------

--- 3.1 Environment Variables for CA Lab

Environment variables need to be set for EPICS applications.
Please read this document:
             http://www.aps.anl.gov/epics/base/R3-14/1-docs/CAref.html
It's necessary to add %APPDATA%\caLab to the PATH environment variable.

--- 3.2 Problems to execute VIs

All provided VIs depend on correct set absolute path to "caLab.dll", "ca.dll"
and "com.dll". This libraries are in %APPDATA%\calab

--- 3.3 Issues with Creating an Executable with the Application Builder

If you encounter issues when creating a working executable with the
Application Builder, try changing the target folder from user.lib to
another target (sometimes only root works).
The following libraries and services are required by CA Lab:
caLab.dll, ca.dll, Com.dll, caRepeater.exe, and either
https://aka.ms/vs/17/release/vc_redist.x86.exe (32bit) or 
https://aka.ms/vs/17/release/vc_redist.x64.exe (64bit).
The PATH environment variable needs to be set to these files.
If you encounter dependency issues, install CA Lab on the target
computer and set the PATH environment variable.

--- 3.4 Issues After Installing a New CA Lab Version

If you upgrade existing VIs with CA Lab, please check for greyed out
CA Lab VIs. Right-click on greyed out CA Lab VIs and use
"Relink to SubVI".
Be aware that LabVIEW™ may result in a misalignment of connectors.