// Setup build script for Windows

[Files]
Source: "Release\calab.dll"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "src\calab.cpp"; DestDir: "{userappdata}\calab\src"; Flags: confirmoverwrite; Components: sources
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\ca.dll"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\caget.exe"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\cainfo.exe"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\camonitor.exe"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\caput.exe"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\caRepeater.exe"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\Com.dll"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\dbCore.dll"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\dbRecStd.dll"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\softIoc.exe"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\..\..\epics\current-epics-base\dbd\softIoc.dbd"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\VC_redist.x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "vis\caLab-errors.txt"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLab.lvlib"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLab.lvlps"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLab.lvproj"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\calab.mnu"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabDisconnect.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabEvent.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabEventUnregister.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabFilter.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabGet.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabInfo.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabInit.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabPut.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\CaLabSoftIOC.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\PV Info.ctl"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\PV Info.vi"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\PV.ctl"; DestDir: "{userappdata}\calab"; Flags: confirmoverwrite; Components: caLab
Source: "vis\demo\db\demo.db"; DestDir: "{userappdata}\calab\Demo\db"; Flags: confirmoverwrite; Components: caLab
Source: "vis\demo\db\TestPV_ai100000.db"; DestDir: "{userappdata}\calab\Demo\db"; Flags: confirmoverwrite; Components: caLab
Source: "vis\demo\DemoIOC.cmd"; DestDir: "{userappdata}\calab\Demo"; Flags: confirmoverwrite; Components: caLab
Source: "vis\demo\TestPV_ai100000.cmd"; DestDir: "{userappdata}\calab\Demo"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\caLab.db"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Event Demo.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Parallel Event Demo Sub\Parallel Event Close.vi"; DestDir: "{userappdata}\calab\Examples\Parallel Event Demo Sub\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Parallel Event Demo Sub\Parallel Event Indicator.vi"; DestDir: "{userappdata}\calab\Examples\Parallel Event Demo Sub\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Parallel Event Demo Sub\Parallel Event Init.vi"; DestDir: "{userappdata}\calab\Examples\Parallel Event Demo Sub\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Parallel Event Demo Sub\Parallel Event Struct.vi"; DestDir: "{userappdata}\calab\Examples\Parallel Event Demo Sub\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Parallel Event Demo Sub\Parallel Event Task.vi"; DestDir: "{userappdata}\calab\Examples\Parallel Event Demo Sub\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Parallel Event Demo.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\pvList.txt"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Read Demo 1.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Read Demo 2.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\SoftIOC Demo Sub.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\SoftIOC Demo.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Write Demo - Looping.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Write Demo - Timed.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Write Demo.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Examples\Write Random TestPV_ai.vi"; DestDir: "{userappdata}\calab\Examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\utilities\add CaLab palette.vi"; DestDir: "{userappdata}\calab\utilities"; Flags: confirmoverwrite; Components: caLab
Source: "vis\utilities\remove CaLab palette.vi"; DestDir: "{userappdata}\calab\utilities"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabDisconnect_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabDisconnect_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabEvent_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabEvent_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabGet_Main_Initialized.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabGet_Main.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabGet_Result_Filter.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabInit_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabInit_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabPut_Main_Initialized.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabPut_Main.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabSoftIocEnd.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CaLabSoftIocStart.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\CheckWindows.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\ConfigurationSet.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\DbdPathName.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\DbPathName.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Get_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Get_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Get_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Get_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Ioc_Config.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\ioc_mbbi_config.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\port5064free.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_Boolean_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_Boolean_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_Boolean-1D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_Boolean-1D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_Boolean-1D_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_Boolean-1D_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_Boolean-2D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_Boolean-2D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_DBL_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_DBL_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_DBL-1D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_DBL-1D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_DBL-1D_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_DBL-1D_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_DBL-2D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_DBL-2D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I16_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I16_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I16-1D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I16-1D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I16-1D_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I16-1D_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I16-2D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I16-2D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I32_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I32_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I32-1D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I32-1D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I32-1D_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I32-1D_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I32-2D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I32-2D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I64_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I64_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I64-1D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I64-1D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I64-1D_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I64-1D_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I64-2D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I64-2D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I8_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I8_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I8-1D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I8-1D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I8-1D_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I8-1D_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I8-2D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_I8-2D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_SGL_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_SGL_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_SGL-1D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_SGL-1D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_SGL-1D_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_SGL-1D_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_SGL-2D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_SGL-2D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_String_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_String_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_String-1D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_String-1D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_String-1D_PV-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_String-1D_PV.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_String-2D_PV-1D-I.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\Put_String-2D_PV-1D.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\RestartCounter.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\SET_EPICS_CA_ADDR_LIST.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "vis\Private\SoftIocPathName.vi"; DestDir: "{userappdata}\calab\Private\"; Flags: confirmoverwrite; Components: caLab
Source: "batch\startDemo.bat"; DestDir: "{userappdata}\calab\batch\"; Flags: confirmoverwrite; Components: caLab
Source: "batch\startTestPV_ai100000.bat"; DestDir: "{userappdata}\calab\batch\"; Flags: confirmoverwrite; Components: caLab
Source: "ReadMeFirst.txt"; DestDir: "{userappdata}\calab"; DestName: "ReadMeFirst.txt"; Flags: confirmoverwrite; Components: caLab
Source: "changelog.txt"; DestDir: "{userappdata}\calab"; DestName: "changelog.txt"
Source: "post_install.html"; DestDir: "{userappdata}\calab"; DestName: "post_install.html"

[Dirs]
Name: "{userappdata}\calab\Examples"; Components: caLab
Name: "{userappdata}\calab"; Components: caLab or caTools
Name: "{userappdata}\calab\Private"; Components: caLab
Name: "{userappdata}\calab\src"; Components: sources

[Types]
Name: full; Description: Full installation
Name: vis; Description: VIs only
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: caLab; Description: LabVIEW™️ VIs for get PVs, put PVs and create new PVs at EPICS; Types: full vis
Name: catools; Description: native tools (caget.exe, camonitor.exe, caput.exe, cainfo.exe); Types: full
Name: sources; Description: sources; Types: full
Name: vcruntimeadmin; Description: "Check if bundled VS runtime install is necessary? (admin may be required)"; Types: full custom; Check: IsAdminInstallMode
Name: vcruntimeuser; Description: "Check if bundled VS runtime install is necessary? (admin may be required)"; Types: full custom; Check: not IsAdminInstallMode

[Languages]
Name: en; MessagesFile: compiler:Default.isl; LicenseFile: "LICENSE"

[Messages]
UninstalledAll=%1 was successfully removed from your computer. Please check if you still need to remove CA Lab from palette set (LabVIEW™️)

[Setup]
AllowNoIcons=true
AlwaysRestart=false
AlwaysShowDirOnReadyPage=true
AlwaysShowGroupOnReadyPage=true
AlwaysUsePersonalGroup=true
AppComments=Before un/installing CA Lab, you should close all EPICS applications!
AppContact=carsten.winkler@helmholtz-berlin.de
AppCopyright=HZB GmbH
AppendDefaultGroupName=true
AppId={{A9194CD2-4489-4268-B21B-7E622C31267F}
AppName=CA Lab 32-bit
AppPublisher=HZB GmbH
AppPublisherURL=www.helmholtz-berlin.de
AppSupportURL=https://hz-b.de/calab
AppUpdatesURL=https://hz-b.de/calab
AppVersion=1.7.3.3


ChangesEnvironment=true
DefaultDirName={userappdata}\calab
DefaultGroupName=National Instruments\caLab
DirExistsWarning=no
DisableStartupPrompt=true
InfoAfterFile=
InfoBeforeFile="changelog.txt"
LanguageDetectionMethod=none
LicenseFile="LICENSE"
OutputBaseFilename=caLabSetup_1733x86
OutputDir=.
PrivilegesRequired=none
SetupIconFile=res\caLab.ico
ShowLanguageDialog=no
UninstallDisplayIcon={userappdata}\calab\caLab.dll,1
UninstallDisplayName=CA Lab 32-bit
UninstallLogMode=append
VersionInfoCompany=HELMHOLTZ-ZENTRUM BERLIN
VersionInfoCopyright=HZB
VersionInfoDescription=CA Lab Setup
VersionInfoProductName=CA Lab
VersionInfoProductVersion=1.7.3.3
VersionInfoTextVersion=1.7.3.3
VersionInfoVersion=1.7.3.3
WizardImageFile="res\WizardImage-IS.bmp"
WizardSmallImageFile=res\WizardSmallImage-IS2.bmp
MinVersion=0,6.1
UsePreviousAppDir=False
VersionInfoProductTextVersion=Version: 1.7.3.3
VersionInfoOriginalFileName=caLabSetup_1733x86.exe

[Icons]
Name: {group}\CA Lab project; Filename: {userappdata}\calab; IconFilename: {userappdata}\calab\caLab.dll; IconIndex: 0
Name: {group}\Online documentation; Filename: https://hz-b.de/calab
Name: {group}\Start Demo; Filename: {userappdata}\calab\batch\startDemo.bat; Flags: createonlyiffileexists; Components: ; WorkingDir: {userappdata}\calab; IconFilename: {userappdata}\calab\caLab.dll; IconIndex: 2
Name: {group}\Demo SoftIOC; Filename: {userappdata}\calab\Demo\DemoIOC.cmd; Flags: createonlyiffileexists; Components: ; WorkingDir: {userappdata}\calab\Demo; IconFilename: {userappdata}\calab\caLab.dll; IconIndex: 2
Name: {group}\Uninstall; Filename: {uninstallexe}; IconFilename: {userappdata}\calab\caLab.dll; IconIndex: 1

[Run]
// install redistributable packages for Visual Studio 2015, 2017, 2019, and 2022
// https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170
// latest version: https://aka.ms/vs/17/release/VC_redist.x86.exe
Components: vcruntimeadmin; Filename: {tmp}\VC_redist.x86.exe; Check: IsAdminInstallMode and NeedsVCRedistInstall; Parameters: "/passive /norestart /Q:a /c:""msiexec /qb /i vcredist.msi"" "; StatusMsg: Checking for VC++ RunTime ...
Components: vcruntimeuser; Filename: {tmp}\VC_redist.x86.exe; Check: (not IsAdminInstallMode) and NeedsVCRedistInstall; Parameters: "/passive /norestart /Q:a /c:""msiexec /qb /i vcredist.msi"" "; Flags: runasoriginaluser; StatusMsg: Checking for VC++ RunTime ...
Filename: "{userappdata}\calab\post_install.html"; Description: "Launch post installation checklist"; Flags: postinstall shellexec

[UninstallRun]
RunOnceId: "UninstallEntry1"; Filename: cmd; Parameters: /c taskkill /f /im camonitor.exe; Flags: RunHidden;
RunOnceId: "UninstallEntry2"; Filename: cmd; Parameters: /c taskkill /f /im softIoc.exe; Flags: RunHidden;
RunOnceId: "UninstallEntry3"; Filename: cmd; Parameters: /c taskkill /f /im caRepeater.exe; Flags: RunHidden;

[Code]
function SplitString(const S, Delimiter: string): TArrayOfString;
var
  I, Start, DelimLen: Integer;
begin
  DelimLen := Length(Delimiter);
  Start := 1;
  SetArrayLength(Result, 0);
  for I := 1 to Length(S) do
    if Copy(S, I, DelimLen) = Delimiter then
    begin
      SetArrayLength(Result, Length(Result) + 1);
      Result[Length(Result)-1] := Copy(S, Start, I - Start);
      Start := I + DelimLen;
    end;
  SetArrayLength(Result, Length(Result) + 1);
  Result[Length(Result)-1] := Copy(S, Start, MaxInt);
end;

function SearchPath(FileName: string): Boolean;
var
  I: Integer;
  FoldersArray: TArrayOfString;
  Folder: string;
begin
  // Get the PATH environment variable
  if RegQueryStringValue(HKLM, 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', Folder) then
  begin
    // Split the PATH variable into an array of folders
    FoldersArray := SplitString(Folder, ';');
    // Loop through each folder in the PATH
    for I := 0 to GetArrayLength(FoldersArray)-1 do
    begin
      // If the file exists in this folder, return true
      if FileExists(FoldersArray[I] + '\' + FileName) then
      begin
        Result := True;
        Exit;
      end;
    end;
  end;
  // If the file was not found, return false
  Result := False;
end;

function InitializeSetup(): Boolean;
var
  foundPath: string;
begin
  if (SearchPath('caRepeater.exe') or
      SearchPath('ca.dll') or
      SearchPath('Com.dll')) then
  begin
    MsgBox('The files caRepeater.exe, ca.dll or Com.dll have already been found on the computer. To avoid incompatibility with CA Lab libraries, the setup will be cancelled.', mbError, MB_OK);
    Result := False;
  end
  else
    Result := True;
end;

function CompareVersion(V1, V2: string): Integer;
// Compare version strings
// Returns 0, if the versions are equal.
// Returns -1, if the V1 is older than the V2.
// Returns 1, if the V1 is newer than the V2.
var
  P, N1, N2: Integer;
begin
  Result := 0;
  while (Result = 0) and ((V1 <> '') or (V2 <> '')) do
  begin
    P := Pos('.', V1);
    if P > 0 then
    begin
      N1 := StrToInt(Copy(V1, 1, P - 1));
      Delete(V1, 1, P);
    end
      else
    if V1 <> '' then
    begin
      N1 := StrToInt(V1);
      V1 := '';
    end
      else
    begin
      N1 := 0;
    end;

    P := Pos('.', V2);
    if P > 0 then
    begin
      N2 := StrToInt(Copy(V2, 1, P - 1));
      Delete(V2, 1, P);
    end
      else
    if V2 <> '' then
    begin
      N2 := StrToInt(V2);
      V2 := '';
    end
      else
    begin
      N2 := 0;
    end;

    if N1 < N2 then Result := -1
      else
    if N1 > N2 then Result := 1;
  end;
end;

function VCinstalled(const regKey: string): Boolean;
{ Function for Inno Setup Compiler }
{ Returns True if same or later Microsoft Visual C++ 2015 Redistributable is installed, otherwise False. }
var
  major: Cardinal;
  minor: Cardinal;
  bld: Cardinal;
  rbld: Cardinal;
  installed_ver, min_ver: String;

begin
  Result := False;
  { Mimimum version of the VC++ Redistributable needed (currently 14.26.28 and later) }
  min_ver := '14.26.28000.0';
  Log('Minimum VC++ 2015-2019 Redist version is: ' + min_ver);

  if RegQueryDWordValue(HKEY_LOCAL_MACHINE, regKey, 'Major', major) then begin
    if RegQueryDWordValue(HKEY_LOCAL_MACHINE, regKey, 'Minor', minor) then begin
      if RegQueryDWordValue(HKEY_LOCAL_MACHINE, regKey, 'Bld', bld) then begin
        if RegQueryDWordValue(HKEY_LOCAL_MACHINE, regKey, 'RBld', rbld) then begin
            installed_ver := IntToStr(major) + '.' + IntToStr(minor) + '.' + IntToStr(bld) + '.' + IntToStr(rbld);
            Log('VC++ 2015-2019 Redist version is: ' + installed_ver);
            { Version info was found. Return true if later or equal to our min_ver redistributable }
            // Note brackets required because of weird operator precendence
            //Result := (major >= 14) and (minor >= 23) and (bld >= 27820) and (rbld >= 0)
            //Log('Installed Version ' + installed_ver + ' >= Minimum Version ' + min_ver + Format(': %d', [IntToStr((CompareVersion(installed_ver, min_ver) >= 0))]));
            Result := (CompareVersion(installed_ver, min_ver) >= 0)
        end;
      end;
    end;
  end;
end;

function NeedsVCRedistInstall: Boolean;
begin
  if NOT IsWin64 then
    { 32-bit OS, 32-bit installer }
    Result := not (VCinstalled('SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X86'))
  else if Is64BitInstallMode then
    { 64-bit OS, 64-bit installer }
    Result := not (VCinstalled('SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64'))
  else
    { 64-bit OS, 32-bit installer }
    Result := not (VCinstalled('SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86'));  
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectComponents then
    if (not IsAdminInstallMode) then
    begin
      // Runtime query/install component unchecked by default
      // in User mode installs. Checked in Admin installs.
      WizardForm.ComponentsList.Checked[3] := False;
      //WizardForm.ComponentsList.ItemEnabled[4] := False;
    end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  msg: String;
begin
  Result := True;
  msg := 'The option to check for/install the VS' + #13#10 +
        'runtime is unchecked. Please make sure a' + #13#10 +
        'compatible version of the Visual Studio' + #13#10 +
        'VC++ runtime is already installed (by you' + #13#10 +
        'or an admin), or click "No" and check' + #13#10 +
        'the box before proceeding.' + #13#10 + #13#10 +
        'You will need admin privileges to' + #13#10 +
        'to install the runtime.' + #13#10 + #13#10 +
        'Do you wish to proceed as is?';
  if CurPageID = wpSelectComponents then begin
    if IsAdminInstallMode then begin
      if (not WizardIsComponentSelected('vcruntimeadmin')) then
        Result := SuppressibleMsgBox(msg, mbInformation, MB_YESNO, IDYES) = IDYES
    end else
      if (not WizardIsComponentSelected('vcruntimeuser')) then
        Result := SuppressibleMsgBox(msg, mbInformation, MB_YESNO, IDYES) = IDYES;
  end;
end;
