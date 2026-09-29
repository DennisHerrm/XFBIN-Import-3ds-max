; ============================================================
;  XFBIN Import for 3ds Max - Inno Setup script
;
;  Normally built by ERSTELLE_RELEASE.bat, which stages the
;  package and passes the version:
;
;    ISCC.exe /DAppVersion=2.1.6 /DPackageDir=..\dist\stage\XfbinImport XfbinImport.iss
;
;  The installer only copies the Autodesk application package
;  into ApplicationPlugins - 3ds Max finds it there by itself.
;  No registry keys besides Inno's own uninstall entry, no
;  services, no admin rights unless "all users" is chosen.
;
;  Code signing: pass /DSignToolName=<name> and configure a sign
;  tool of that name in the Inno Setup IDE (Tools -> Configure
;  Sign Tools), or with /S on the command line. Without it the
;  installer is built unsigned.
; ============================================================

#ifndef AppVersion
  #error Pass the version: ISCC /DAppVersion=x.y.z
#endif
#ifndef PackageDir
  #define PackageDir "..\dist\stage\XfbinImport"
#endif

#define AppName      "XFBIN Import for 3ds Max"
#define AppPublisher "DennisH"
#define AppURL       "https://github.com/DennisHerrm/XFBIN-Import-3ds-max"

[Setup]
; AppId identifies the installation for upgrades and uninstall.
; It must NEVER change between versions.
AppId={{6E7F7540-80A3-4835-9E9B-14FFC650A134}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
AppCopyright=Copyright (c) 2026 {#AppPublisher}

; Autodesk fixes the location - there is nothing to choose.
;   only for me : %APPDATA%\Autodesk\ApplicationPlugins
;   all users   : %ProgramData%\Autodesk\ApplicationPlugins
DefaultDirName={autoappdata}\Autodesk\ApplicationPlugins\XfbinImport
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UsedUserAreasWarning=no

; Per-user by default, so no UAC prompt. The dialog lets the
; user switch to an all-users install.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

; Keep the uninstaller out of the package root so the folder
; stays exactly what Autodesk expects.
UninstallFilesDir={app}\Uninstall
UninstallDisplayName={#AppName}
UninstallDisplayIcon={uninstallexe}

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

LicenseFile=..\LICENSE
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no

OutputDir=..\dist
OutputBaseFilename=XfbinImport-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes

; Shown under Properties -> Details of the setup file.
VersionInfoVersion={#AppVersion}
VersionInfoProductVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Setup
VersionInfoCopyright=Copyright (c) 2026 {#AppPublisher}

#ifdef SignToolName
SignTool={#SignToolName}
SignedUninstaller=yes
#endif

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[Messages]
en.FinishedLabelNoIcons=Setup has finished installing [name].%n%nStart 3ds Max and open the importer from the main menu:%n    DH Tools > XFBIN Import%n%nOn 3ds Max 2025 and newer the menu may only appear after a second restart. It is always available under Customize > Customize User Interface > Category "DH Tools".
de.FinishedLabelNoIcons=[name] wurde installiert.%n%n3ds Max starten und den Importer im Hauptmenue oeffnen:%n    DH Tools > XFBIN Import%n%nAb 3ds Max 2025 erscheint das Menue unter Umstaenden erst nach einem zweiten Neustart. Unter Customize > Customize User Interface > Kategorie "DH Tools" ist es immer zu finden.

[CustomMessages]
en.MaxRunning=3ds Max is running.%n%nThe plugin file is locked while 3ds Max is open. Please save your work, close every 3ds Max window and click Retry.
de.MaxRunning=3ds Max laeuft noch.%n%nSolange 3ds Max offen ist, ist die Plugin-Datei gesperrt. Bitte die Arbeit speichern, alle 3ds-Max-Fenster schliessen und auf Wiederholen klicken.

[InstallDelete]
; Remove files left by an older version (or a manual install)
; so no stale plugin build stays behind.
Type: filesandordirs; Name: "{app}\Contents"

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Code]
{ Is a process with this executable name running?
  WMI answers for processes of every user and session. If the
  query itself fails, assume "not running" - Inno's Restart
  Manager check (CloseApplications) still catches a locked file. }
function IsProcessRunning(const ExeName: String): Boolean;
var
  Locator, Service, Found: Variant;
begin
  Result := False;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Service := Locator.ConnectServer('.', 'root\CIMV2');
    Found := Service.ExecQuery(Format('SELECT ProcessId FROM Win32_Process WHERE Name = "%s"', [ExeName]));
    Result := Found.Count > 0;
  except
  end;
end;

{ Ask until 3ds Max is closed or the user gives up. }
function WaitForMaxClosed(): Boolean;
begin
  Result := True;
  while IsProcessRunning('3dsmax.exe') do
  begin
    if SuppressibleMsgBox(CustomMessage('MaxRunning'), mbError, MB_RETRYCANCEL, IDCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

function InitializeSetup(): Boolean;
begin
  Result := WaitForMaxClosed();
end;

function InitializeUninstall(): Boolean;
begin
  Result := WaitForMaxClosed();
end;
