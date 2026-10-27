; Installateur Windows : standalone + VST3, VB-Cable en option (coche par defaut)
; ISCC /DAppVersion=x.y.z installer\gragarda-voice.iss  (attend pkg\ rempli comme en CI)
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{6F1C2B9E-3D47-4E8A-9B1F-5A2C7D4E8F10}
AppName=Gragarda Voice
AppVersion={#AppVersion}
AppPublisher=Gragarda
AppPublisherURL=https://voice.gragarda.fr
DefaultDirName={autopf}\Gragarda Voice
DefaultGroupName=Gragarda Voice
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
LicenseFile=..\LICENSE
OutputDir=..
OutputBaseFilename=gragarda-voice-standalone
WizardStyle=modern

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
fr.VBCable=Installer le cable virtuel VB-Cable (micro "CABLE Output" pour Discord, OBS, Teams...)
en.VBCable=Install the VB-Cable virtual cable ("CABLE Output" mic for Discord, OBS, Teams...)
fr.Shortcut=Raccourci sur le bureau
en.Shortcut=Desktop shortcut

[Tasks]
Name: "vbcable"; Description: "{cm:VBCable}"
Name: "desktopicon"; Description: "{cm:Shortcut}"

[Files]
Source: "..\pkg\Gragarda Voice Standalone\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
Source: "..\pkg\Gragarda Voice.vst3\*"; DestDir: "{commoncf64}\VST3\Gragarda Voice.vst3"; Flags: recursesubdirs ignoreversion
Source: "vbcable.ps1"; DestDir: "{tmp}"; Flags: deleteafterinstall; Tasks: vbcable

[Icons]
Name: "{group}\Gragarda Voice"; Filename: "{app}\Gragarda Voice.exe"
Name: "{autodesktop}\Gragarda Voice"; Filename: "{app}\Gragarda Voice.exe"; Tasks: desktopicon

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Gragarda Voice.vst3"

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{tmp}\vbcable.ps1"""; StatusMsg: "VB-Cable..."; Flags: waituntilterminated; Tasks: vbcable
Filename: "{app}\Gragarda Voice.exe"; Flags: nowait postinstall skipifsilent
