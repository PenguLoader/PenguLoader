
#define MyAppName "League Loader"
#define MyAppPublisher "Nomi-san"
#define MyAppURL "https://leagueloader.app"
#define MyAppExeName "League Loader.exe"
#define MyAppCopyright "Copyright © Nomi. All rights reserved."

#define Major
#define Minor
#define Rev
#define Build
#define MyAppVersion GetVersionComponents("..\bin\League Loader.exe", Major, Minor, Rev, Build), Str(Major) + "." + Str(Minor) + "." + Str(Rev)

[Setup]
AppId={{3975A51B-215D-4331-A521-C54C85D1640F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} v{#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
VersionInfoCompany=GitHub, Inc
VersionInfoCopyright={#MyAppCopyright}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
PrivilegesRequired=admin
OutputDir=.\
OutputBaseFilename=league-loader-v{#MyAppVersion}
SetupIconFile=icon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=classic
UninstallDisplayName={#MyAppName}   
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\plugins\*"; DestDir: "{app}\plugins"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent runascurrentuser

[Code]
function InitializeUninstall(): Boolean;
var
  ResultCode: Integer;
begin
  if Exec(ExpandConstant('{app}\League Loader.exe'), '--uninstall', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    Result := ResultCode = 0;
  end
  else begin
    Result := False;
  end;
end;
