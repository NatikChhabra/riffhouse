; Riffhouse - free, open-source guitar rig
; One installer, no questions that matter: it installs for the current user, so Windows never
; asks for an administrator password, and updates can go in silently.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build\src\Riffhouse_artefacts\Release"
#endif

[Setup]
AppId={{6C2B7C0E-3C1B-4B7E-9A55-52494646484F}
AppName=Riffhouse
AppVersion={#AppVersion}
AppVerName=Riffhouse {#AppVersion}
AppPublisher=Riffhouse contributors
AppPublisherURL=https://github.com/NatikChhabra/riffhouse
AppSupportURL=https://github.com/NatikChhabra/riffhouse/issues
AppUpdatesURL=https://github.com/NatikChhabra/riffhouse/releases
DefaultDirName={localappdata}\Programs\Riffhouse
DefaultGroupName=Riffhouse
DisableProgramGroupPage=yes
DisableDirPage=yes
DisableReadyPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=Riffhouse-{#AppVersion}-windows-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; the pictures come from tools/brand/make_assets.py
SetupIconFile=riffhouse.ico
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard-small.bmp
UninstallDisplayIcon={app}\Riffhouse.exe
UninstallDisplayName=Riffhouse
; Never let the installer close other programs: a DAW with the plugin open stays open.
CloseApplications=no
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoProductName=Riffhouse

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Put a shortcut on the desktop"; Flags: checkedonce

[Files]
Source: "{#BuildDir}\Standalone\Riffhouse.exe"; DestDir: "{app}"; Flags: ignoreversion
; The plugin for recording software. Skipped, not failed, when a DAW has it open.
Source: "{#BuildDir}\VST3\Riffhouse.vst3\*"; DestDir: "{usercf}\VST3\Riffhouse.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: PluginIsFree

[Icons]
Name: "{userprograms}\Riffhouse"; Filename: "{app}\Riffhouse.exe"
Name: "{userdesktop}\Riffhouse"; Filename: "{app}\Riffhouse.exe"; Tasks: desktopicon

[Run]
; a normal install: offer to start it
Filename: "{app}\Riffhouse.exe"; Description: "Start Riffhouse"; Flags: nowait postinstall skipifsilent
; "Restart and update" from inside the app: start it again without asking
Filename: "{app}\Riffhouse.exe"; Flags: nowait; Check: WantsRelaunch

[Code]
function WantsRelaunch(): Boolean;
begin
  Result := WizardSilent() and (ExpandConstant('{param:RELAUNCH|0}') = '1');
end;

// True when the installed plugin can be replaced. A DAW that has it loaded holds the file open.
function PluginIsFree(): Boolean;
var
  Path: String;
begin
  Path := ExpandConstant('{usercf}\VST3\Riffhouse.vst3\Contents\x86_64-win\Riffhouse.vst3');
  Result := True;
  if FileExists(Path) then
  begin
    if RenameFile(Path, Path + '.old') then
      RenameFile(Path + '.old', Path)
    else
      Result := False;
  end;
end;

// The app starts the installer as it closes. Wait for it to be gone before touching its files.
function InitializeSetup(): Boolean;
var
  Waited: Integer;
begin
  Waited := 0;
  while CheckForMutexes('Global\RiffhouseRunning,Local\RiffhouseRunning,RiffhouseRunning') and (Waited < 150) do
  begin
    Sleep(200);
    Waited := Waited + 1;
  end;
  Result := True;
  if CheckForMutexes('Global\RiffhouseRunning,Local\RiffhouseRunning,RiffhouseRunning') then
  begin
    if not WizardSilent() then
      MsgBox('Riffhouse is still open. Close it, then run this installer again.', mbInformation, MB_OK);
    Result := False;
  end;
end;
