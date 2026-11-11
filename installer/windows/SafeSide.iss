; Inno Setup script for BOBA SAFE-SIDE (Windows, VST3, 64-bit).
; Built in CI:  ISCC /DAppVersion=2.0.0 /DSourceDir=<dir with SAFE-SIDE.vst3> /DOutDir=dist installer\windows\SafeSide.iss

#ifndef AppVersion
  #define AppVersion "2.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\build\SafeSide_artefacts\Release\VST3"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif
#ifndef OutName
  #define OutName "BOBA-SafeSide-" + AppVersion + "-Windows-Setup"
#endif

[Setup]
AppId={{4B0BA5AF-E51D-4E2A-9C61-2D0B0BA5A7E2}
AppName=SAFE-SIDE
AppVersion={#AppVersion}
AppVerName=BOBA SAFE-SIDE {#AppVersion}
AppPublisher=BOBA
AppSupportURL=https://github.com/Niklauren/SAFE-SIDE
AppUpdatesURL=https://github.com/Niklauren/SAFE-SIDE/releases
DefaultDirName={commoncf64}\VST3
DisableDirPage=no
DirExistsWarning=no
AppendDefaultDirName=no
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallDisplayName=BOBA SAFE-SIDE {#AppVersion}
UninstallFilesDir={commonpf64}\BOBA\SAFE-SIDE
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutDir}
OutputBaseFilename={#OutName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=auto
WizardImageFile=wizard.bmp,wizard@2x.bmp
WizardSmallImageFile=wizard-small.bmp,wizard-small@2x.bmp
WizardImageStretch=yes
CloseApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoCompany=BOBA
VersionInfoDescription=BOBA SAFE-SIDE installer

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
ru.WelcomeExtra=Стереорасширитель, который не крадёт моно: шесть характеров ширины (AIR, PRISM, TWIN, SHINE, PHANTOM, SPACE), моно на выходе равно моно на входе.%n%nПлагин VST3 будет установлен в стандартную папку VST3. Перед установкой закрой DAW.
en.WelcomeExtra=Stereo widener that never steals the mono: six width characters, the mono sum stays exactly the original.%n%nThe VST3 plug-in goes into the standard VST3 folder. Please close your DAW before installing.
ru.DirHint=Папка для VST3. Оставь как есть, если не знаешь, что менять: её находят все DAW.
en.DirHint=VST3 folder. Keep the default unless you know otherwise: every DAW scans it.
ru.Done=Готово. Открой DAW и пересканируй плагины (Ableton: Settings → Plug-ins → Use VST3 Plug-in System Folders → Rescan). Плагин появится у производителя BOBA.
en.Done=Done. Open your DAW and rescan plug-ins (Ableton: Settings → Plug-ins → Use VST3 Plug-in System Folders → Rescan). The plug-in is listed under BOBA.

[Messages]
ru.WelcomeLabel2=Будет установлен [name/ver].
en.WelcomeLabel2=This will install [name/ver] on your computer.

[Files]
Source: "{#SourceDir}\SAFE-SIDE.vst3\*"; DestDir: "{app}\SAFE-SIDE.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; remove an older copy first so no stale files remain inside the bundle
Type: filesandordirs; Name: "{app}\SAFE-SIDE.vst3"

[UninstallDelete]
Type: filesandordirs; Name: "{app}\SAFE-SIDE.vst3"

[Code]
procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel2.Caption := WizardForm.WelcomeLabel2.Caption + #13#10#13#10 + CustomMessage('WelcomeExtra');
  WizardForm.SelectDirLabel.Caption := CustomMessage('DirHint');
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
    WizardForm.FinishedLabel.Caption := CustomMessage('Done');
end;
