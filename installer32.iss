; Installer Settings for Inno Setup
; (Release Version)

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{2FCD745D-F3C2-4115-B537-D6AE6E066B82}
AppName=MZ-IME日本語入力
AppVerName=MZ-IME日本語入力 1.0.0.3
AppPublisher=片山博文MZ
AppPublisherURL=https://katahiromz.fc2.page/
AppSupportURL=https://katahiromz.fc2.page/mzimeja
AppUpdatesURL=https://katahiromz.fc2.page/mzimeja
DefaultDirName={pf}\mzimeja
DefaultGroupName=MZ-IME日本語入力
OutputDir=.
OutputBaseFilename=mzimeja32-1.0.0.3-setup
Compression=lzma
SolidCompression=yes
VersionInfoVersion=1.0.0.3
VersionInfoTextVersion=1.0.0.3
AlwaysRestart=yes
UninstallRestartComputer=yes
ArchitecturesAllowed=x86
DisableDirPage=yes

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Files]
Source: "READMEJP.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "ChangeLog.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\basic.dic"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\name.dic"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\kanji.dat"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\radical.dat"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\postal.dat"; DestDir: "{app}"; Flags: ignoreversion
Source: "build32\Release\mzimeja.ime"; DestDir: "{app}\x86"; Flags: ignoreversion
Source: "build32\Release\ime_setup32.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build32\Release\imepad.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build32\Release\dict_compile.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build32\Release\verinfo.exe"; DestDir: "{app}"; Flags: ignoreversion

; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\READMEJP.txt"; Filename: "{app}\READMEJP.txt"
Name: "{group}\LICENSE.txt"; Filename: "{app}\LICENSE.txt"
Name: "{group}\ChangeLog.txt"; Filename: "{app}\ChangeLog.txt"
Name: "{group}\MZ-IMEパッド"; Filename: "{app}\imepad.exe"
Name: "{group}\バージョン情報"; Filename: "{app}\verinfo.exe"
Name: "{group}\アンインストール"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\ime_setup32.exe"; Parameters: "/i"
Filename: "{app}\ime_setup64.exe"; Parameters: "/i"; Check: IsWin64

[UninstallRun]
Filename: "{app}\ime_setup32.exe"; Parameters: "/u"
Filename: "{app}\ime_setup64.exe"; Parameters: "/u"; Check: IsWin64
