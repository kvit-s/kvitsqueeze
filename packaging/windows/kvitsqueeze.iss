; KvitSqueeze installer — Inno Setup 6 (prd.md §13 Q5).
;
; Portable zip for the beta, this for the release. Built from the staged tree
; that win-deploy.bat produces, so the installer ships exactly what was tested
; running from that directory — the same executable, the same Qt DLLs.
;
; Two things here are obligations rather than choices:
;
;   * Qt is installed as separate DLLs, never statically linked, so a user can
;     relink against their own Qt build. That is the LGPLv3 obligation prd.md
;     §11.4 says to actually honour.
;   * The licences directory is installed, always.
;
; ── The audio engine is downloaded, not shipped
;
; squeezelite is GPLv3. KvitSqueeze does not distribute it: this installer
; fetches it from upstream during setup, verifies it against a pinned SHA-256,
; and puts it at {app}\engine\squeezelite.exe. Nothing in a KvitSqueeze artifact
; contains GPL-licensed code, so nothing in a KvitSqueeze release carries
; GPLv3's distribution duties. See THIRD-PARTY-NOTICES.md.
;
; The download URL is deliberately *not* compiled in here. Upstream keeps only
; a rolling window of builds on SourceForge and prunes the rest — the build
; this project pinned first is already gone — so a baked-in URL would 404
; within a year or two and every installer in the field would be permanently
; broken. Instead setup reads packaging/engine-manifest.txt over the network
; from this project's own repository; editing that file repairs installers
; that have already shipped.
;
; The download is never a gate. If it fails — no network, a corporate proxy, a
; pruned URL, an unchecked task — setup completes anyway and KvitSqueeze
; installs. The app reports a missing engine and names the repair script.

#define AppName "KvitSqueeze"
#define AppPublisher "KvitSqueeze"
#define AppExe "kvitsqueeze.exe"

; Passed in by win-package.bat; all three have a default so the script can also
; be opened and compiled by hand from the Inno Setup IDE.
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef StageDir
  #define StageDir "..\..\build-windows-msvc-release\Release"
#endif
; Overridable so a staging manifest, or a local one, can be pointed at without
; editing this file — which is also how the download path gets tested without
; publishing anything.
#ifndef EngineManifestUrl
  #define EngineManifestUrl "https://raw.githubusercontent.com/kvit-s/kvitsqueeze/main/packaging/engine-manifest.txt"
#endif

[Setup]
; A new AppId for the new name. Keeping the old one would have this installer
; treat a SqeezeAmp install as its own previous version and replace it in the
; Apps list, which is wrong: they are different products, and the old one still
; owns its own uninstaller. Uninstall SqeezeAmp separately.
AppId={{204278E3-C3B2-4438-858B-7E1CE5C65197}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=..\..\dist
OutputBaseFilename=KvitSqueeze-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; The engine arrives as a .zip, so the full extractor is needed rather than
; Inno's default 7z-only "enhanced" mode.
ArchiveExtraction=full
; Per-user by default: KvitSqueeze is a desktop music player, and nothing it does
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
; KvitSqueeze's own licence — MPL-2.0. This used to point at the GPLv3 text that
; travelled with the engine binary, which asked the user to accept a licence
; that was never KvitSqueeze's, and is not shipped at all any more.
LicenseFile={#StageDir}\licenses\LICENSE
; The setup program's own icon, and the one shown in Apps & Features. The
; shortcuts do not need it named: they point at kvitsqueeze.exe, which carries the
; same icon as a resource (packaging/windows/kvitsqueeze.rc). Relative paths here
; resolve against this script's directory.
SetupIconFile=kvitsqueeze.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "engine"; Description: "Download the audio engine (squeezelite) — required for playback"; GroupDescription: "Audio engine"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts"; Flags: unchecked
Name: "startup"; Description: "Start {#AppName} with Windows, minimised to the tray"; GroupDescription: "Startup"; Flags: unchecked

[Files]
; The whole staged tree: the executable, the Qt runtime windeployqt placed
; beside it, licenses\ and the engine repair script. Note there is no engine\ —
; see the header.
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; \
    Excludes: "*.pdb,*.ilk,*.exp,*.lib,make-appicon.exe,engine\*"
; Downloaded to {tmp} by the [Code] section below, then unpacked here. The
; executable inside is named for its build variant, so NormaliseEngineName
; renames it to the one ExternalEngine looks for.
Source: "{tmp}\sqz-engine.zip"; DestDir: "{app}\engine"; \
    Flags: external extractarchive ignoreversion; Check: EngineIsReady; \
    AfterInstall: NormaliseEngineName
; The manifest this installer was built with, compiled in and never installed.
; It is what setup falls back to when the published one cannot be reached, so
; an unreachable GitHub costs the repairability rather than the engine.
Source: "{#StageDir}\engine-manifest.txt"; Flags: dontcopy noencryption

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
; The engine was downloaded rather than installed from the package, and the
; executable was renamed after extraction, so Inno has tracked neither.
Type: filesandordirs; Name: "{app}\engine"
; The artwork cache and the logs are ours and are not user data. The player
; identity and the settings under HKCU\Software\KvitSqueeze are deliberately
; *not* removed: reinstalling should get the same player back, with its
; server-side queue and settings intact (prd.md FR-1.4).
Type: filesandordirs; Name: "{localappdata}\{#AppName}\{#AppName}\cache"
Type: filesandordirs; Name: "{localappdata}\{#AppName}\{#AppName}\logs"

[Code]
var
  DownloadPage: TDownloadWizardPage;
  EngineReady: Boolean;
  EngineAttempted: Boolean;
  EngineMember: String;

{ Reads one "key = value" line out of the manifest. A "#" begins a comment, and
  the key match is case-insensitive. Deliberately simple: the same file is
  parsed by fetch-engine.ps1, so the format has to be trivial in two languages. }
function ManifestValue(const Lines: TArrayOfString; const Key: String): String;
var
  I, P: Integer;
  Line: String;
begin
  Result := '';
  for I := 0 to GetArrayLength(Lines) - 1 do begin
    Line := Lines[I];
    P := Pos('#', Line);
    if P > 0 then
      Line := Copy(Line, 1, P - 1);
    P := Pos('=', Line);
    if P = 0 then
      Continue;
    if CompareText(Trim(Copy(Line, 1, P - 1)), Key) = 0 then begin
      Result := Trim(Copy(Line, P + 1, Length(Line)));
      Exit;
    end;
  end;
end;

function EngineIsReady: Boolean;
begin
  Result := EngineReady;
end;

{ The archive holds squeezelite-<variant>-x64.exe plus upstream's licence text.
  ExternalEngine opens engine\squeezelite.exe and nothing else, so the binary is
  renamed here rather than teaching the app about build variants. The licence
  text is kept beside it: a user who now has a GPLv3 program on disk should
  have its terms too. }
procedure NormaliseEngineName;
var
  Dir: String;
begin
  Dir := ExpandConstant('{app}\engine\');
  if (EngineMember <> '') and FileExists(Dir + EngineMember) then begin
    DeleteFile(Dir + 'squeezelite.exe');
    RenameFile(Dir + EngineMember, Dir + 'squeezelite.exe');
  end;
  if FileExists(Dir + 'LICENSE.txt') then begin
    DeleteFile(Dir + 'LICENSE.squeezelite.txt');
    RenameFile(Dir + 'LICENSE.txt', Dir + 'LICENSE.squeezelite.txt');
  end;
end;

procedure InitializeWizard;
begin
  EngineReady := False;
  EngineAttempted := False;
  EngineMember := '';
  DownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing),
                                     SetupMessage(msgPreparingDesc), nil);
  DownloadPage.ShowBaseNameInsteadOfUrl := True;
end;

{ The published manifest first, because it is the one that can be corrected
  after this installer has shipped. The copy compiled into the installer is the
  fallback: by definition it is the older pin, but it is the pin that was
  tested, and it works when GitHub does not. }
function LoadManifest(var Lines: TArrayOfString): Boolean;
begin
  Result := False;

  try
    DownloadPage.Clear;
    DownloadPage.Add('{#EngineManifestUrl}', 'published-manifest.txt', '');
    DownloadPage.Download;
    Result := LoadStringsFromFile(ExpandConstant('{tmp}\published-manifest.txt'), Lines);
    if Result then
      Log('engine: using the published manifest');
  except
    Log('engine: the published manifest is unreachable: ' + GetExceptionMessage);
  end;
  if Result then
    Exit;

  try
    ExtractTemporaryFile('engine-manifest.txt');
    Result := LoadStringsFromFile(ExpandConstant('{tmp}\engine-manifest.txt'), Lines);
    if Result then
      Log('engine: falling back to the manifest this installer was built with');
  except
    Log('engine: no usable manifest at all: ' + GetExceptionMessage);
  end;
end;

{ Manifest, then the archive it names. Every failure here is soft: setup
  continues without an engine and the app says so, which is a far better
  outcome than refusing to install a music player because SourceForge was
  unreachable. }
function TryDownloadEngine: Boolean;
var
  Lines: TArrayOfString;
  Url, Sha: String;
begin
  Result := False;

  if not LoadManifest(Lines) then
    Exit;

  Url := ManifestValue(Lines, 'url');
  Sha := ManifestValue(Lines, 'sha256');
  EngineMember := ManifestValue(Lines, 'member');
  if (Url = '') or (Sha = '') or (EngineMember = '') then begin
    Log('engine: the manifest is missing url, sha256 or member');
    Exit;
  end;
  Log('engine: ' + ManifestValue(Lines, 'version') + ' from ' + Url);

  { The checksum is Inno's job here — a mismatch raises, so a mirror that
    answers with an HTML error page fails before anything is unpacked. }
  DownloadPage.Clear;
  DownloadPage.Add(Url, 'sqz-engine.zip', Sha);
  DownloadPage.Download;
  Result := True;
end;

{ Runs exactly once, however setup was started. }
procedure EnsureEngine;
begin
  if EngineAttempted then
    Exit;
  EngineAttempted := True;

  if not WizardIsTaskSelected('engine') then begin
    Log('engine: not selected, skipping the download');
    Exit;
  end;

  if not WizardSilent then
    DownloadPage.Show;
  try
    try
      EngineReady := TryDownloadEngine;
    except
      EngineReady := False;
      if DownloadPage.AbortedByUser then
        Log('engine: download aborted by user')
      else
        Log('engine: download failed: ' + GetExceptionMessage);
    end;
  finally
    if not WizardSilent then
      DownloadPage.Hide;
  end;

  if not EngineReady then
    SuppressibleMsgBox(
      'The audio engine could not be downloaded, so KvitSqueeze will install without it.' + #13#10#13#10 +
      'Everything else works; playback will report that no engine is present. ' +
      'To finish later, run fetch-engine.ps1 from the installation folder once you have a connection.',
      mbInformation, MB_OK, IDOK);
end;

{ The interactive path: download on the way out of the Ready page, so the
  progress is visible and a user who cannot wait can cancel it. }
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpReady then
    EnsureEngine;
end;

{ The silent path. /SILENT and /VERYSILENT show no wizard pages at all, so
  NextButtonClick never fires and the download would simply not happen —
  a silent install would quietly produce an app that cannot play. This hook
  runs in both modes, and EngineAttempted keeps it from downloading twice. }
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  EnsureEngine;
end;
