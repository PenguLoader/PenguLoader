
#define MyAppName "Pengu Loader"
#define MyAppPublisher "Pengu Loader"
#define MyAppURL "https://pengu.lol"
#define MyAppExeName "Pengu Loader.exe"
#define MyAppCopyright "© 2024 Pengu Loader. All rights reserved."

#define Major
#define Minor
#define Rev
#define Build
#define MyAppVersion GetVersionComponents("..\bin\Pengu Loader.exe", Major, Minor, Rev, Build), Str(Major) + "." + Str(Minor) + "." + Str(Rev)

[Setup]
AppId={{3975A51B-215D-4331-A521-C54C85D1640F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} v{#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright={#MyAppCopyright}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={commonpf64}\{#MyAppName}
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
LicenseFile=..\LICENSE
OutputDir=.\
OutputBaseFilename=pengu-v{#MyAppVersion}-setup
SetupIconFile=..\loader\Resources\icon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{app}"; Permissions: everyone-full
Name: "{app}\plugins"; Permissions: everyone-full
Name: "{commonappdata}\{#MyAppName}"; Permissions: everyone-full; Flags: uninsneveruninstall
Name: "{commonappdata}\{#MyAppName}\plugins"; Permissions: everyone-full; Flags: uninsneveruninstall

[Files]
Source: "..\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\version"; DestDir: "{app}"; Flags: skipifsourcedoesntexist

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Icons]
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
; runascurrentuser

[Code]
function InitializeUninstall(): Boolean;
var
  ResultCode: Integer;
begin
  if Exec(ExpandConstant('{app}\Pengu Loader.exe'), '--uninstall', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    Result := ResultCode = 0;
  end
  else begin
    Result := False;
  end;
end;
