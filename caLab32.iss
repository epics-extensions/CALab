// Setup build script for Windows

[Files]
// General Installation Files
Source: "Release\calab.dll"; DestDir: "{app}\lib"; Flags: confirmoverwrite; Components: caLab
Source: "src\*.cpp"; DestDir: "{app}\src"; Flags: confirmoverwrite; Components: sources
Source: "src\*.h"; DestDir: "{app}\src"; Flags: confirmoverwrite; Components: sources

// EPICS Dependencies
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\*.dll"; DestDir: "{app}\lib"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\..\..\epics\current-epics-base\bin\win32-x86\*.exe"; DestDir: "{app}\lib"; Flags: confirmoverwrite; Components: caLab or caTools
Source: "..\..\..\epics\current-epics-base\dbd\softIoc.dbd"; DestDir: "{app}\lib"; Flags: confirmoverwrite; Components: caLab or caTools

// Visual Studio Redistributable
Source: "..\VC_redist.x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

// LabVIEW VIs
Source: "vis\*.txt"; DestDir: "{app}"; Flags: confirmoverwrite; Components: caLab
Source: "vis\*.vi"; DestDir: "{app}"; Flags: confirmoverwrite; Components: caLab
Source: "vis\*.ctl"; DestDir: "{app}"; Flags: confirmoverwrite; Components: caLab
Source: "vis\*.mnu"; DestDir: "{app}"; Flags: confirmoverwrite; Components: caLab
Source: "vis\*.lvlib"; DestDir: "{app}"; Flags: confirmoverwrite; Components: caLab
Source: "vis\*.lvproj"; DestDir: "{app}"; Flags: confirmoverwrite; Components: caLab
Source: "vis\private\*"; DestDir: "{app}\private"; Flags: confirmoverwrite; Components: caLab
Source: "vis\utilities\*"; DestDir: "{app}\utilities"; Flags: confirmoverwrite; Components: caLab

// Demo Files
Source: "vis\demo\*.*"; DestDir: "{app}\demo"; Flags: confirmoverwrite; Components: caLab
Source: "vis\demo\db\*.*"; DestDir: "{app}\demo\db"; Flags: confirmoverwrite; Components: caLab
Source: "vis\examples\*.*"; DestDir: "{app}\examples"; Flags: confirmoverwrite; Components: caLab
Source: "vis\examples\Parallel Event Demo Sub\*.*"; DestDir: "{app}\examples\Parallel Event Demo Sub"; Flags: confirmoverwrite; Components: caLab

// Documentation
Source: "readMeFirst.txt"; DestDir: "{app}\docs"; DestName: "readMeFirst.txt"; Flags: confirmoverwrite; Components: caLab
Source: "changelog.txt"; DestDir: "{app}\docs"; DestName: "changelog.txt"

[Dirs]
Name: "{app}\examples"; Components: caLab
Name: "{app}\private"; Components: caLab
Name: "{app}\src"; Components: sources

[Types]
Name: full; Description: Full installation
Name: vis; Description: VIs only
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: caLab; Description: LabVIEW™️ VIs for get PVs, put PVs and create new PVs at EPICS; Types: full vis
Name: catools; Description: native tools (caget.exe, camonitor.exe, caput.exe, cainfo.exe); Types: full
Name: sources; Description: sources; Types: full
Name: vcruntime; Description: "Check if bundled VS runtime install is necessary?"; Types: full custom; Check: IsAdminInstallMode

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
AppId={{B6C731D9-DF80-4C2E-8A3D-25A4FC5DFB77}
AppName=CA Lab 32-bit
AppPublisher=HZB GmbH
AppPublisherURL=www.helmholtz-berlin.de
AppSupportURL=https://hz-b.de/calab
AppUpdatesURL=https://hz-b.de/calab
AppVersion=1.8.0.2


ChangesEnvironment=true
DefaultDirName={autopf}\user.lib\calab
DefaultGroupName=National Instruments\caLab
DirExistsWarning=no
DisableStartupPrompt=true
InfoAfterFile=
InfoBeforeFile="changelog.txt"
LanguageDetectionMethod=none
LicenseFile="LICENSE"
OutputBaseFilename=caLabSetup_1802x86
OutputDir=.
PrivilegesRequired=none
PrivilegesRequiredOverridesAllowed=dialog
SetupIconFile=res\caLab.ico
ShowLanguageDialog=no
UninstallDisplayIcon={userappdata}\calab\caLab.dll,1
UninstallDisplayName=CA Lab 32-bit
UninstallLogMode=append
UsePreviousAppDir=no
VersionInfoCompany=HELMHOLTZ-ZENTRUM BERLIN
VersionInfoCopyright=HZB
VersionInfoDescription=CA Lab Setup
VersionInfoProductName=CA Lab
VersionInfoProductVersion=1.8.0.2
VersionInfoTextVersion=1.8.0.2
VersionInfoVersion=1.8.0.2
WizardImageFile="res\WizardImage-IS.bmp"
WizardSmallImageFile=res\WizardSmallImage-IS2.bmp
MinVersion=0,6.1sp1
VersionInfoProductTextVersion=Version: 1.8.0.2
VersionInfoOriginalFileName=caLabSetup_1802x86.exe

[Icons]
Name: {group}\CA Lab project; Filename: {app}; IconFilename: {app}\caLab.dll; IconIndex: 0
Name: {group}\Online documentation; Filename: https://hz-b.de/calab
Name: {group}\Start Demo; Filename: {app}\batch\startDemo.bat; Flags: createonlyiffileexists; WorkingDir: {app}; IconFilename: {app}\caLab.dll; IconIndex: 2
Name: {group}\Demo SoftIOC; Filename: {app}\Demo\DemoIOC.cmd; Flags: createonlyiffileexists; Components: ; WorkingDir: {app}\Demo; IconFilename: {app}\caLab.dll; IconIndex: 2
Name: {group}\Uninstall; Filename: {uninstallexe}; IconFilename: {app}\caLab.dll; IconIndex: 1

[Run]
// install redistributable packages for Visual Studio 2015, 2017, 2019, and 2022
// https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170
// latest version: https://aka.ms/vs/17/release/VC_redist.x86.exe
Components: vcruntime; Filename: {tmp}\VC_redist.x86.exe; Check: IsAdminInstallMode and NeedsVCRedistInstall; Parameters: "/passive /norestart /Q:a /c:""msiexec /qb /i vcredist.msi"" "; StatusMsg: Checking for VC++ RunTime ...
Filename: "{app}\docs\post_install.html"; Description: "Important: Open the post installation checklist"; \
    Flags: postinstall shellexec

[UninstallRun]
RunOnceId: "UninstallEntry1"; Filename: cmd; Parameters: /c taskkill /f /im camonitor.exe; Flags: RunHidden;
RunOnceId: "UninstallEntry2"; Filename: cmd; Parameters: /c taskkill /f /im softIoc.exe; Flags: RunHidden;
RunOnceId: "UninstallEntry3"; Filename: cmd; Parameters: /c taskkill /f /im caRepeater.exe; Flags: RunHidden;

[Code]
procedure CreateNextStepsHtml();
var
  Content: String;
  AppPathEscaped: String;
  PaletteSection: String;
begin
  AppPathEscaped := ExpandConstant('{app}');
  StringChange(AppPathEscaped, '\', '\\');
  PaletteSection := '';
  if not IsAdminInstallMode then
  begin
    PaletteSection :=
      '<label for="step2" class="step"> Add CA Lab to palette set (User Controls / User Libraries)</label><br>' +
      '<div class="substep">' +
      '<input type="checkbox" id="substep4" name="substep4" value="substep4">' +
      '<label for="substep4"> go to LabVIEW<sup>&trade;</sup> &rarr; and run <code id="code1">"<span id="lvPath"></span>\utilities\add CaLab palette.vi"</code>' +
      '<button type="button" onclick="copyToClipboard(''code1'')">copy code</button></label><br>' +
      '</div>' +
      '<label for="step3" class="step"> Let''s see if everything is working fine</label><br>' +
      '<div class="substep">' +
      '<input type="checkbox" id="substep5" name="substep5" value="substep5">' +
      '<label for="substep5"> go to LabVIEW<sup>&trade;</sup> and run<br>' +
      '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<code id="code1a">"<span id="lvPath"></span>\examples\SoftIOC Demo.vi"</code>' +
      '<button type="button" onclick="copyToClipboard(''code1a'')">copy code</button></label><br>' +
      '</div>';
  end;

  Content := '<!DOCTYPE html> <html> <head> <title>CA Lab Post Installation</title>' +
    '<meta name="viewport" content="width=device-width, initial-scale=1"> <style>' +
    'body { font-family: Verdana, sans-serif; margin: 0; padding: 0; background-color: #f0f0f0; }' +
    'form { margin-bottom: 250px; padding: 0; } svg { margin-top: 0; }' +
    'input[type="checkbox"] { transform: scale(1.5); margin-right: 10px; background-color: #f0f0f0; }' +
    'input[type="checkbox"]:checked + label { color: #000; } label { color: #666; }' +
    'button { background-color: #4caf4f30; } code { background-color: #afafaf; padding: 2px; }' +
    '.step { font-weight: bold; margin-top: 30px; } .substep { font-weight: normal; margin-left: 30px; margin-bottom: 5px; }' +
    '.char { opacity: 0; animation: fadeIn 0.3s forwards; }' +
    '@keyframes fadeIn { to { opacity: 1; } }' +
    '/* Animation delays for each character */' +
    '.char:nth-child(n) { animation-delay: calc(0.2s * var(--char-index)); }' +
    '.progress-container { width: 100%; height: 20px; background-color: #4caf4f30; border-radius: 10px; margin: 20px 0; position: relative; }' +
    '#progress-bar { width: 0%; height: 100%; background-color: #4CAF50; border-radius: 10px; transition: width 0.2s ease-in-out; }' +
    '#progress-text { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); color: #000; }' +
    '</style> </head> <body style="font-size: larger; margin: 0em 1em 1em 1em;">' +
    '<div style="position: relative; width: 500px; height: 260px; margin: 0 auto;">' +
    '<img src="https://github.com/epics-extensions/CALab/releases/download/v1.7.2.2/CaLab.png" alt="CA Lab Logo"' +
    ' style="position: absolute; top: 0; left: 50%; transform: translateX(-50%); height: 260px; width: 260px;">' +
    '<svg width="500" height="160" style="position: absolute; top: 130px; left: 50%; transform: translate(-50%, 30px);">' +
    '<path id="curve" d="M 120 -5 Q 230 185 375 10" fill="transparent" />' +
    '<text width="500" y="20" style="font: 28px Verdana, sans-serif; text-anchor: middle;">' +
    '<textPath href="#curve" startOffset="50%">' +
    '<tspan class="char" style="--char-index: 1">N</tspan>' +
    '<tspan class="char" style="--char-index: 2">e</tspan>' +
    '<tspan class="char" style="--char-index: 3">x</tspan>' +
    '<tspan class="char" style="--char-index: 4">t</tspan>' +
    '<tspan class="char" style="--char-index: 5">&nbsp;</tspan>' +
    '<tspan class="char" style="--char-index: 6">S</tspan>' +
    '<tspan class="char" style="--char-index: 7">t</tspan>' +
    '<tspan class="char" style="--char-index: 8">e</tspan>' +
    '<tspan class="char" style="--char-index: 9">p</tspan>' +
    '<tspan class="char" style="--char-index: 10">s</tspan>' +
    '</textPath> </text> </svg> </div>' +
    '<div class="progress-container">' +
    '<div id="progress-bar"></div>' +
    '<div id="progress-text">0%</div>' +
    '</div>' +
    '<form action="/submit" method="get">' +
    '<label for="step1" class="step"> Compile CA Lab to current LabVIEW<sup>&trade;</sup> version</label><br>' +
    '<div class="substep">' +
    '<input type="checkbox" id="substep1" name="substep1" value="substep1">' +
    '<label for="substep1"> go to LabVIEW<sup>&trade;</sup> &rarr; Tools &rarr; Advanced &rarr; Mass Compile &hellip;</label><br>' +
    '<input type="checkbox" id="substep2" name="substep2" value="substep2">' +
    '<label for="substep2"> then open <code id="code0">"<span id="lvPath"></span>"</code> (could be repeated)</label>' +
    '<button type="button" onclick="copyToClipboard(''code0'')">copy code</button><br>' +
    '<input type="checkbox" id="substep3" name="substep3" value="substep3">' +
    '<label for="substep3"> click &quot;Mass Compile&quot; (you can ignore &quot;Insane Array&quot; warnings)</label><br>' +
    '</div>' +
    PaletteSection +
    '<label for="step3" class="step"> Lets see if everything is working fine</label><br>' +
    '<div class="substep"><input type="checkbox" id="substep5" name="substep5" value="substep5">' +
    '<label for="substep5"> go to LabVIEW<sup>&trade;</sup> and run<br>' +
    '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<code id="code1">"<span id="lvPath"></span>\examples\SoftIOC Demo.vi"</code> <button type="button" onclick="copyToClipboard(''code1'')">copy code</button><br>' +
    '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(softIoc and caRepeater may trigger the firewall. You should grant them access.)</label></div><br>' +
    '<label for="step4" class="step"> EPICS Base control commands</label><br>' +
    '<label for="substep21">open a command prompt</label><br>' +
    '<input type="checkbox" id="substep22" name="substep22" value="substep22">' +
    '<label for="substep22">caget, camonitor, caput and softIoc should now be available as commands<br>' +
    '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(They may all trigger the firewall. You should grant them access.)</label><br>' +
    '</form>' +
    '<script>' +
    'function updateProgress() {' +
    '  const checkboxes = document.querySelectorAll(''input[type="checkbox"]'');' +
    '  const total = checkboxes.length;' +
    '  const checked = Array.from(checkboxes).filter(cb => cb.checked).length;' +
    '  const percentage = Math.round((checked / total) * 100);' +
    '  document.getElementById(''progress-bar'').style.width = percentage + ''%'';' +
    '  document.getElementById(''progress-text'').textContent = percentage + ''%'';' +
    '}' +
    'function copyToClipboard(id) {' +
    '  const codeElement = document.getElementById(id);' +
    '  const text = codeElement.innerText || codeElement.textContent;' +
    '  navigator.clipboard.writeText(text);' +
    '}' +
    'document.querySelectorAll(''input[type="checkbox"]'').forEach(checkbox => {' +
    '  checkbox.addEventListener(''change'', updateProgress);' +
    '});' +
    'const lvPathElements = document.querySelectorAll(''#lvPath'');' +
    'lvPathElements.forEach(element => {' +
    '  element.textContent = "' + AppPathEscaped + '";' +
    '});' +
    'updateProgress();' +
    '</script>' +
    '<a href="https://www.helmholtz-berlin.de/zentrum/organisation/it/calab/index_en.html"' +
    ' style="position: fixed; bottom: 0; right: 0; padding: 10px 20px;' +
    ' background-color: #4caf50; color: #fff; text-decoration: none;">More documentation</a>' +
    '</body> </html>';
  SaveStringToFile(ExpandConstant('{app}\docs\post_install.html'), Content, False);
end;

// Function to split a string into an array of strings by a delimiter 
function SplitString(const S, Delimiter: string): TArrayOfString;
{ Function for Inno Setup Compiler }
{ Returns array of strings split by the given delimiter }
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

// Function to check if a file exists in system PATH
function SearchPath(FileName: string): Boolean;
{ Function for Inno Setup Compiler }
{ Returns True if file is found in any PATH directory, False otherwise }
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

// Function to perform pre-installation checks
function InitializeSetup(): Boolean;
{ Function for Inno Setup Compiler }
{ Returns True if setup can proceed, False if conflicting files are found }
begin
  // Check for conflicting files on the system
  if (SearchPath('caRepeater.exe') or SearchPath('ca.dll') or SearchPath('Com.dll')) then
  begin
    MsgBox('The files caRepeater.exe, ca.dll or Com.dll have already been found on the computer. To avoid incompatibility with CA Lab libraries, the setup will be cancelled.', mbError, MB_OK);
    Result := False;
  end
  else
    Result := True;
end;

// Function to initialize the installation wizard
procedure InitializeWizard();
{ Function for Inno Setup Compiler }
{ Sets up initial directory paths based on LabVIEW installation }
var
  lvPath : String;
begin
  if IsAdminInstallMode then
    begin
      lvPath := '';
      RegQueryStringValue(HKEY_LOCAL_MACHINE, 'Software\National Instruments\LabVIEW\CurrentVersion\', 'Path', lvPath);
      WizardForm.DirEdit.Text := AddBackslash(lvPath) + 'user.lib\calab';
    end
  else
    WizardForm.DirEdit.Text := ExpandConstant('{userappdata}\calab');
end;

// Function to add installation directory to system PATH
procedure ModifyPath;
{ Function for Inno Setup Compiler }
{ Adds application lib directory to system PATH environment variable }
var
  RegKey: string;
  CurrentPath: string;
  NewPath: string;
  AppLibPath: string;
  RootKey: Integer;
begin
  // Pre-calculate registry root key
  RootKey := HKEY_CURRENT_USER;
  if IsAdminInstallMode then
    RootKey := HKEY_LOCAL_MACHINE;

  // Set appropriate registry key based on install mode
  if IsAdminInstallMode then
    RegKey := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment'
  else
    RegKey := 'Environment';

  // Get the current PATH using pre-calculated root key
  if not RegQueryStringValue(RootKey, RegKey, 'Path', CurrentPath) then
    CurrentPath := '';

  // Prepare path to add
  AppLibPath := ExpandConstant('{app}\lib');

  // Check if path already exists
  if Pos(Uppercase(AppLibPath), Uppercase(CurrentPath)) = 0 then
  begin
    // Add the new path with semicolon separator if needed
    if CurrentPath <> '' then
      NewPath := CurrentPath + ';' + AppLibPath
    else
      NewPath := AppLibPath;

    // Write the new PATH using pre-calculated root key
    RegWriteExpandStringValue(RootKey, RegKey, 'Path', NewPath);
  end;
  RegWriteExpandStringValue(RootKey, RegKey, 'CALAB_HOME', ExpandConstant('{app}'));
end;

// Function to remove installation directory from system PATH
procedure RemovePath;
{ Function for Inno Setup Compiler }
{ Removes application lib directory from system PATH environment variable }
var
  RegKey: string;
  CurrentPath, NewPath: string;
  AppLibPath: string;
  RootKey: Integer;
  PathParts: TArrayOfString;
  I: Integer;
begin
  // Pre-calculate registry root key
  RootKey := HKEY_CURRENT_USER;
  if IsAdminInstallMode then
    RootKey := HKEY_LOCAL_MACHINE;

  // Set appropriate registry key based on install mode
  if IsAdminInstallMode then
    RegKey := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment'
  else
    RegKey := 'Environment';

  // Get the current PATH
  if RegQueryStringValue(RootKey, RegKey, 'Path', CurrentPath) then
  begin
    AppLibPath := ExpandConstant('{app}\lib');
    
    // Split path into parts
    PathParts := SplitString(CurrentPath, ';');
    NewPath := '';
    
    // Rebuild path excluding our entry
    for I := 0 to GetArrayLength(PathParts)-1 do
    begin
      if Uppercase(PathParts[I]) <> Uppercase(AppLibPath) then
      begin
        if NewPath <> '' then
          NewPath := NewPath + ';';
        NewPath := NewPath + PathParts[I];
      end;
    end;

    // Write back cleaned path
    if NewPath <> CurrentPath then
      RegWriteExpandStringValue(RootKey, RegKey, 'Path', NewPath);
  end;

  // remove CALAB_HOME if it exists
  if RegValueExists(RootKey, RegKey, 'CALAB_HOME') then
    RegDeleteValue(RootKey, RegKey, 'CALAB_HOME');
end;

// Function to compare version strings
function CompareVersion(V1, V2: string): Integer;
{ Function for Inno Setup Compiler }
{ Returns 0 if versions equal, -1 if V1 older than V2, 1 if V1 newer than V2 }
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

// Function to check if Visual C++ Redistributable is installed
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

// Function to check if VC++ redistributable installation is needed
function NeedsVCRedistInstall: Boolean;
{ Function for Inno Setup Compiler }
{ Returns True if VC++ redistributable needs to be installed, False otherwise }
begin
  if NOT IsWin64 then
    Result := not (VCinstalled('SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X86'))
  else if Is64BitInstallMode then
    Result := not (VCinstalled('SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64'))
  else
    Result := not (VCinstalled('SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86'));  
end;

// Procedure called after installation steps
procedure CurStepChanged(CurStep: TSetupStep);
{ Function for Inno Setup Compiler }
{ Handles post-installation PATH modifications }
begin
  if CurStep = ssPostInstall then
    CreateNextStepsHtml();
    ModifyPath();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
{ Function for Inno Setup Compiler }
{ Handles PATH cleanup and file removal during uninstallation }
var
  VersionFile: string;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // Delete version file if it exists
    VersionFile := ExpandConstant('{app}\docs\post_install.html');
    if FileExists(VersionFile) then
      DeleteFile(VersionFile);
    VersionFile := ExpandConstant('{app}\CaLab.aliases');
    if FileExists(VersionFile) then
      DeleteFile(VersionFile);
    // Remove docs directory
    RemoveDir(ExpandConstant('{app}\docs'));

    // Remove PATH entry
    RemovePath;
  end;
end;
