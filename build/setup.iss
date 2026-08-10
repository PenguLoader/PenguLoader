; Pengu Loader installer (Inno Setup 7, x64).
;
; Two payloads with deliberately different lifetimes:
;
;   {app}                      the loader and its native modules
;   {commonappdata}\.pengu     boot.dll, the machine-wide IFEO entry point
;
; The boot is separate because Windows' IFEO value points at a fixed path
; forever, and that path must be one an unprivileged process cannot write —
; otherwise an admin-gated registry key would resolve to user-controlled code.
; It therefore gets ProgramData's default ACL (users read + execute, no write)
; and is NOT given the everyone-full permissions v1.1.6 used on its data
; directory.
;
; Pointing IFEO at {app}\boot.dll instead would save this copy, and is exactly
; what the design avoids: the registry would then name a folder the user can
; move, rename or uninstall, and doing so would stop LeagueClientUx launching
; at all until someone edited HKLM. A fixed ProgramData path never moves, and a
; portable install that disappears just leaves a stale per-user pointer.
;
; This installer does NOT write the IFEO value. It only puts the boot where
; that value can point. Wiring it up happens on first activation in the hub,
; which the user asks for explicitly — installing Pengu should not silently
; insert it into another program's startup.
;
; Expects the payload directory to hold Pengu.exe, core.dll and boot.dll.
; WebView2Loader is statically linked into Pengu.exe, so no loader DLL ships.
; The core carries its Pengu signature inside its own .pengu
; section, so there is no manifest to ship beside it and nothing here has to
; care whether the build was signed — a release boot refuses an unsigned core
; on its own, and CI fails a release run that would produce one.
;
; Per-user state lives in %LOCALAPPDATA%\.pengu and is never touched here. The
; two directories share a name and nothing else: one is machine-wide and
; read-only to users, the other is per-account and holds config, plugins and
; the activation pointer.
;
; v1.1.6 created a world-writable {commonappdata}\Pengu Loader\plugins; that is
; gone, because plugins are code and a shared writable plugin folder let any
; account run code in every other account's client.

; Where the staged payload lives and where the installer is written. CI points
; these at the signed output directory.
#ifndef PayloadDir
  #define PayloadDir "..\bin"
#endif
#ifndef OutputDirectory
  #define OutputDirectory "..\bin"
#endif

#define MyAppName "Pengu Loader"
#define MyAppPublisher "Pengu Loader"
#define MyAppURL "https://pengu.lol"
#define MyAppExeName "Pengu.exe"
#define MyBootName "boot.dll"
#define MyBootDir "{commonappdata}\.pengu"
#define MyAppCopyright "Copyright (c) PenguLoader contributors"

; Version comes from the built exe, which the SDK stamps from the root
; package.json — same single source as everything else.
#define Major
#define Minor
#define Rev
#define Build
#define MyAppVersion GetVersionComponents(PayloadDir + "\" + MyAppExeName, Major, Minor, Rev, Build), Str(Major) + "." + Str(Minor) + "." + Str(Rev)

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
; Admin: writing the boot into ProgramData with a restrictive ACL, and the
; uninstaller removing the IFEO value, both need it.
PrivilegesRequired=admin
LicenseFile=..\LICENSE
OutputDir={#OutputDirectory}
OutputBaseFilename=pengu-v{#MyAppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
; x64-only: the core is injected into a 64-bit client and the host is
; published win-x64.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Build a 64-bit Setup so [Code] runs as a 64-bit process and sees the real
; registry rather than the WOW6432Node view. This is load-bearing, not tidiness:
; the uninstaller reads and deletes the IFEO Debugger value, which lives in the
; native view because LeagueClientUx.exe is 64-bit. From a 32-bit Setup the
; lookup silently finds nothing, the value survives the uninstall, and Windows
; keeps redirecting the client into a boot.dll we just deleted — the client
; stops launching at all. The [Code] below also names HKLM64 explicitly, so
; this stays correct even if the directive is ever dropped.
SetupArchitecture=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
; No explicit Permissions on the boot directory. Created by an elevated
; installer it inherits ProgramData's default ACL — Administrators and SYSTEM
; full, Users read + execute — which is exactly what we want and what an
; everyone-full override would destroy.
;
; uninsneveruninstall is NOT set: the boot must go when Pengu goes, or the IFEO
; value would outlive the thing it points at.
Name: "{#MyBootDir}"

[Files]
Source: "{#PayloadDir}\{#MyAppExeName}";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\core.dll";            DestDir: "{app}"; Flags: ignoreversion
; Shipped in {app} as well as installed to ProgramData, so a portable copy of
; the install folder can re-install or repair the boot on its own — that is
; what BootStubInstaller.ShippedPath looks for.
Source: "{#PayloadDir}\{#MyBootName}";       DestDir: "{app}"; Flags: ignoreversion
; restartreplace because rundll32 keeps the boot mapped for the whole client
; session; upgrading while League is open would otherwise fail outright.
Source: "{#PayloadDir}\{#MyBootName}";       DestDir: "{#MyBootDir}"; Flags: ignoreversion restartreplace uninsrestartdelete

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Icons]
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent runascurrentuser

[UninstallDelete]
; The boot itself is removed in code (ordered against the registry); this
; clears the directory once it's empty.
Type: dirifempty; Name: "{#MyBootDir}"

[Code]
const
  IfeoKey = 'SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\LeagueClientUx.exe';
  LegacyBootDir = '{commonappdata}\Pengu';

{ Is the IFEO Debugger value ours?

  Matched on the full installed path rather than on the file name. 'boot.dll'
  alone is common enough to appear in an unrelated tool's debugger string, and
  tearing down someone else's hook would be a worse bug than leaving ours. }
function DebuggerIsOurs(const Debugger: String): Boolean;
begin
  Result := Pos(LowerCase(ExpandConstant('{#MyBootDir}\{#MyBootName}')), LowerCase(Debugger)) > 0;
end;

{ Clear away the pre-release boot: an earlier build shipped pengu-boot.exe to
  %ProgramData%\Pengu, which Defender quarantines on sight when IFEO triggers
  it. That executable is why the boot is a DLL under rundll32 now. Any IFEO
  value still naming it points at a file that is gone or about to be, so it is
  removed too — otherwise LeagueClientUx would be redirected into nothing.

  Note for anyone editing these comments: Pascal brace comments do not nest, so
  an Inno constant written in braces silently ends the comment and turns the
  rest into code. Spell paths with %VARS% in here. }
procedure RemoveLegacyBoot();
var
  Debugger, Dir: String;
begin
  { A Pascal constant, not a preprocessor define, so it is passed by name. }
  Dir := ExpandConstant(LegacyBootDir);

  { HKLM64, never plain HKEY_LOCAL_MACHINE — see SetupArchitecture above. }
  if RegQueryStringValue(HKLM64, IfeoKey, 'Debugger', Debugger) then
    if Pos('pengu-boot.exe', LowerCase(Debugger)) > 0 then
      RegDeleteKeyIncludingSubkeys(HKLM64, IfeoKey);

  DeleteFile(Dir + '\pengu-boot.exe');
  RemoveDir(Dir);
end;

{ Remove the machine-wide boot, registry value first and DLL second.

  The order is the whole point. Deleting the boot while IFEO still names it
  leaves Windows redirecting LeagueClientUx.exe into a file that no longer
  exists, and the client stops launching at all — the exact failure this
  design was built to remove, arriving via our own uninstaller. Interrupted
  after the registry delete, the leftover DLL is inert.

  The delete can fail while a client is running, because rundll32 holds the
  boot mapped for the session. That is survivable and deliberately not retried:
  with the registry value gone the file does nothing, uninsrestartdelete clears
  it on the next reboot, and blocking an uninstall on "please close League"
  would be worse. }
procedure RemoveBoot();
var
  Debugger: String;
begin
  if RegQueryStringValue(HKLM64, IfeoKey, 'Debugger', Debugger) then
    if DebuggerIsOurs(Debugger) then
      RegDeleteKeyIncludingSubkeys(HKLM64, IfeoKey);

  DeleteFile(ExpandConstant('{#MyBootDir}\{#MyBootName}'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    RemoveLegacyBoot();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveBoot();
end;
