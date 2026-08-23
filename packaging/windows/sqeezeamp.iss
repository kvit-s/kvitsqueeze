; SqeezeAmp installer — Inno Setup 6 (prd.md §13 Q5).
;
; Portable zip for the beta, this for the release. Built from the staged tree
; that win-deploy.bat produces, so the installer ships exactly what was tested
; running from that directory — the same executable, the same Qt DLLs, the same
; audio engine.
;
; Two things here are obligations rather than choices:
;
;   * The licences directory is installed, always. Shipping the GPLv3
;     squeezelite binary requires its licence text to travel with it, and its
;     source to be attached to the same release (prd.md §11.2).
;   * Qt is installed as separate DLLs, never statically linked, so a user can
;     relink against their own Qt build. That is the LGPLv3 obligation prd.md
;     §11.4 says to actually honour.

#define AppName "SqeezeAmp"
#define AppPublisher "SqeezeAmp"
#define AppExe "sqeezeamp.exe"

; Passed in by win-package.bat; both have a default so the script can also be
; opened and compiled by hand from the Inno Setup IDE.
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef StageDir
  #define StageDir "..\..\build-windows-msvc-release\Release"
#endif

[Setup]
AppId={{7C1B5A64-3E8F-4C2D-9B77-1F4E2A6D0C31}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=..\..\dist
OutputBaseFilename=SqeezeAmp-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; Per-user by default: SqeezeAmp is a desktop music player, and nothing it does
; needs administrator rights. Start-with-Windows writes to HKCU (prd.md FR-7.6).
;
; PrivilegesRequired must say so explicitly — Inno defaults it to `admin`, which
; would elevate, install into Program Files, and still write the Run key and the
; cache paths under whichever account happened to accept the UAC prompt. That
; mismatch is exactly what the compiler's UsedUserAreasWarning is about. With
; `lowest`, {autopf} resolves to the per-user programs directory and every
; per-user area below belongs to the user who is installing.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#StageDir}\licenses\LICENSE.squeezelite
; The setup program's own icon, and the one shown in Apps & Features. The
; shortcuts do not need it named: they point at sqeezeamp.exe, which carries the
; same icon as a resource (packaging/windows/sqeezeamp.rc). Relative paths here
; resolve against this script's directory.
SetupIconFile=sqeezeamp.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts"; Flags: unchecked
Name: "startup"; Description: "Start {#AppName} with Windows, minimised to the tray"; GroupDescription: "Startup"; Flags: unchecked

[Files]
; The whole staged tree: the executable, the Qt runtime windeployqt placed
; beside it, engine\squeezelite.exe and licenses\.
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; \
    Excludes: "*.pdb,*.ilk,*.exp,*.lib"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; The same key and the same --minimized flag the in-app setting writes, so the
; two agree instead of producing two entries (prd.md FR-7.6).
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "{#AppName}"; \
    ValueData: """{app}\{#AppExe}"" --minimized"; \
    Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "{app}\{#AppExe}"; Description: "Start {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The artwork cache and the logs are ours and are not user data. The player
; identity and the settings under HKCU\Software\SqeezeAmp are deliberately
; *not* removed: reinstalling should get the same player back, with its
; server-side queue and settings intact (prd.md FR-1.4).
Type: filesandordirs; Name: "{localappdata}\{#AppName}\{#AppName}\cache"
Type: filesandordirs; Name: "{localappdata}\{#AppName}\{#AppName}\logs"
