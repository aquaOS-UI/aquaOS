; AquaOS Inno Setup Script
; Generates standalone Setup.exe for AquaOS Enterprise Desktop Shell

#define MyAppName "AquaOS"
#ifndef MyAppVersion
  #define MyAppVersion "0.0.1"
#endif
#define MyAppPublisher "AquaOS Team"
#define MyAppURL "https://github.com/AquaOS/AquaOS"
#define MyAppExeName "AquaShell.exe"

[Setup]
AppId={{9FA32B10-A785-4813-9E45-97F10E286C32}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\dist
OutputBaseFilename=AquaOS-v{#MyAppVersion}-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
DisableDirPage=no
DisableProgramGroupPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startup"; Description: "Launch AquaOS automatically when signing into Windows"; GroupDescription: "Startup Options:"

[Files]
Source: "..\build\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\RestoreTaskbar.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\scripts\restore.bat"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\scripts\restore.ps1"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\scripts\run.bat"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autoprograms}\Restore Windows Taskbar"; Filename: "{app}\RestoreTaskbar.exe"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "AquaOS"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\RestoreTaskbar.exe"; Flags: runhidden; RunOnceId: "RestoreTaskbarOnUninstall"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    // Safely terminate running AquaShell and restore Windows Taskbar
    Exec('taskkill.exe', '/F /IM AquaShell.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    if FileExists(ExpandConstant('{app}\RestoreTaskbar.exe')) then
    begin
      Exec(ExpandConstant('{app}\RestoreTaskbar.exe'), '', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    end;
  end;
end;

