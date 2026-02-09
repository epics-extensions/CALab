######################################################################
### CA Lab ReadMe for Windows OS
######################################################################

----------------------------------------------------------------------
--- 1. General Information
----------------------------------------------------------------------

After installation, open the post-installation checklist:
    %CALAB_HOME%\docs\post_install.html
The installer usually opens it automatically.

When you run a CA Lab VI for the first time, LabVIEW may show
warning dialogs caused by a new context. Save the changes to suppress
them. A faster method is described in Chapter 2 (Mass Compiling VIs).

CA Lab should be installed in the default location selected by the
installer. If you move the folder afterwards, LabVIEW paths may break.
Use %CALAB_HOME% as the installed root folder.

Start menu entries are under:
    Start Menu -> CA Lab
The "CA Lab project" shortcut opens the installed VI folder.

----------------------------------------------------------------------
--- 2. First Use of CA Lab
----------------------------------------------------------------------

Mass Compiling VIs:
1. In LabVIEW, select Tools -> Advanced -> Mass Compile...
2. Choose the CA Lab installation directory (%CALAB_HOME%).
3. Click "Mass Compile".
4. Finish with "Done".

Start a first test via Start Menu -> CA Lab -> Start Demo.
This will:
1. Start a demo IOC Shell with prepared test variables.
2. Write random values to those variables.
3. Monitor those variables.
4. Show the currently used EPICS context.

----------------------------------------------------------------------
--- 3. Troubleshooting
----------------------------------------------------------------------

--- 3.1 Environment Variables for CA Lab

CA Lab uses EPICS Channel Access and requires environment variables.
The installer sets:
    CALAB_HOME = <CA Lab install folder>
    PATH += %CALAB_HOME%\lib

EPICS applications also use some more enviroment variables.
Please read more here:
https://epics.anl.gov/base/R3-15/2-docs/CAref.html#EPICS

If these variables are missing, add them manually and restart
LabVIEW and any command prompts.

--- 3.2 Problems executing VIs

All provided VIs depend on correct absolute paths to:
    caLab.dll, ca.dll, Com.dll
These files are located in:
    %CALAB_HOME%\lib

--- 3.3 Issues creating an executable (Application Builder)

If you encounter issues creating a working executable, try changing the
target folder from user.lib to another location (sometimes only the
drive root works).

The following files must be available on the target machine:
    caLab.dll, ca.dll, Com.dll, caRepeater.exe
and the correct Visual C++ Redistributable:
    32-bit: https://aka.ms/vs/17/release/vc_redist.x86.exe
    64-bit: https://aka.ms/vs/17/release/vc_redist.x64.exe

Ensure the folder containing these files is on PATH
(usually %CALAB_HOME%\lib).

If dependencies are missing, install CA Lab on the target computer and
make sure PATH is updated.

--- 3.4 Issues after installing a new CA Lab version

If you upgrade existing VIs to a new CA Lab version, check for greyed
out CA Lab VIs. Right-click them and use "Relink to SubVI".
LabVIEW may also misalign connector panes after a relink.
