; Inno Setup 6 – Installer für Schneefall (Windows 10 1903+ / 11, 64 Bit)
; Bauen: iscc /DVersion=1.0.0 [/DQuelle=..\..\dist\Schneefall] pakete\windows\schneefall.iss  (siehe installer.sh)
; Installation pro Benutzer (kein Administrator): %LOCALAPPDATA%\Programs\Schneefall

#ifndef Version
  #error Version fehlt: iscc /DVersion=x.y.z (Quelle: Datei VERSION)
#endif
#ifndef Quelle
  #define Quelle "..\..\dist\Schneefall"
#endif

[Setup]
; feste ID: Updates erkennen die Vorversion
AppId={{6E1B7D0C-4C57-4F52-A3B2-5C0F8B1D9A41}
AppName=Schneefall
AppVersion={#Version}
AppVerName=Schneefall {#Version}
AppPublisher=Henry1598
AppPublisherURL=https://github.com/Henry1598/Schneefall
AppSupportURL=https://github.com/Henry1598/Schneefall/issues
AppUpdatesURL=https://github.com/Henry1598/Schneefall/releases
VersionInfoVersion={#Version}
VersionInfoProductName=Schneefall
VersionInfoDescription=Schneefall Setup
VersionInfoCompany=Henry1598
SetupIconFile=..\..\symbole\schneefall.ico
; kein Administrator: passt zur Pipe-Sicherheit (Steuerkanal gehört dem angemeldeten Benutzer)
PrivilegesRequired=lowest
DefaultDirName={autopf}\Schneefall
DefaultGroupName=Schneefall
DisableProgramGroupPage=yes
DisableDirPage=auto
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.18362
OutputDir=..\..\dist
OutputBaseFilename=Schneefall-{#Version}-windows-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\schneefall.exe
UninstallDisplayName=Schneefall
; Programm selbst beenden (--beenden), nicht den Neustart-Manager verwenden
CloseApplications=no
RestartApplications=no

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
german.TaskAutostart=Schneefall mit Windows starten
german.TaskDesktop=Symbol auf dem &Desktop anlegen
german.RunNow=Schneefall jetzt starten
german.AskSettings=Sollen auch die Einstellungen und die Statistik von Schneefall gelöscht werden?%n%n(%1)%n%nNein behält sie für eine spätere Neuinstallation.
english.TaskAutostart=Start Schneefall with Windows
english.TaskDesktop=Create a shortcut on the &desktop
english.RunNow=Start Schneefall now
english.AskSettings=Also delete the settings and statistics of Schneefall?%n%n(%1)%n%nChoose No to keep them for a later reinstall.

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; Flags: unchecked
Name: "autostart"; Description: "{cm:TaskAutostart}"; Flags: unchecked

[Files]
Source: "{#Quelle}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Schneefall"; Filename: "{app}\schneefall.exe"
Name: "{autoprograms}\Schneefall beenden"; Filename: "{app}\schneefall.exe"; Parameters: "--beenden"
Name: "{autodesktop}\Schneefall"; Filename: "{app}\schneefall.exe"; Tasks: desktopicon

[Registry]
; derselbe Wert wie im Programm (src/plattform/windows/autostart.cpp): HKCU\...\Run, „Schneefall“, Pfad in Anführungszeichen;
; so zeigt das Häkchen „Mit dem System starten“ im Steuerfenster denselben Zustand
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Schneefall"; \
    ValueData: """{app}\schneefall.exe"""; Tasks: autostart; Flags: uninsdeletevalue

[Run]
Filename: "{app}\schneefall.exe"; Description: "{cm:RunNow}"; Flags: nowait postinstall skipifsilent

[Code]
// laufende Instanz über den Steuerkanal beenden (Datei wäre sonst gesperrt)
procedure InstanzBeenden(const Programm: String);
var
  Ergebnis: Integer;
begin
  if FileExists(Programm) then
  begin
    Exec(Programm, '--beenden --still', '', SW_HIDE, ewWaitUntilTerminated, Ergebnis);
    Sleep(1500); // Prozess hat Zeit zum Aufräumen
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  InstanzBeenden(ExpandConstant('{app}\schneefall.exe'));
  Result := '';
end;

function InitializeUninstall(): Boolean;
begin
  InstanzBeenden(ExpandConstant('{app}\schneefall.exe'));
  Result := True;
end;

// Autostart-Wert entfernen, wenn er auf diese Installation zeigt (auch wenn das Programm ihn selbst gesetzt hat)
procedure AutostartEntfernen;
var
  Wert, Erwartet: String;
begin
  Erwartet := '"' + ExpandConstant('{app}\schneefall.exe') + '"';
  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'Schneefall', Wert) then
    if CompareText(Wert, Erwartet) = 0 then
      RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'Schneefall');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Einstellungen: String;
begin
  if CurUninstallStep = usUninstall then
    AutostartEntfernen;
  if CurUninstallStep = usPostUninstall then
  begin
    // Laufzeitdateien (Sperrdatei) gehören nicht dem Benutzer: immer weg
    DelTree(ExpandConstant('{localappdata}\Schneefall'), True, True, True);
    // Einstellungen und Statistik nur auf ausdrücklichen Wunsch; Standard und stiller Modus: behalten
    Einstellungen := ExpandConstant('{userappdata}\Schneefall');
    if DirExists(Einstellungen) and (not UninstallSilent) then
      if MsgBox(FmtMessage(CustomMessage('AskSettings'), [Einstellungen]), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(Einstellungen, True, True, True);
  end;
end;
