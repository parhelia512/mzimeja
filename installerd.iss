; Installer Settings for Inno Setup
; (Release Version)

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{2FCD745D-F3C2-4115-B537-D6AE6E066B82}
AppName=MZ-IME日本語入力
AppVerName=MZ-IME日本語入力 1.0.0.1 (デバッグ版)
AppPublisher=片山博文MZ
AppPublisherURL=https://katahiromz.fc2.page/
AppSupportURL=https://katahiromz.fc2.page/mzimeja
AppUpdatesURL=https://katahiromz.fc2.page/mzimeja
DefaultDirName={pf}\mzimeja
DefaultGroupName=MZ-IME日本語入力
OutputDir=.
OutputBaseFilename=mzimeja-1.0.0.1d-setup
Compression=lzma
SolidCompression=yes
VersionInfoVersion=1.0.0.1
VersionInfoTextVersion=1.0.0.1
AlwaysRestart=yes
UninstallRestartComputer=yes

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Files]
Source: "READMEJP.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "HISTORY.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\basic.dic"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\name.dic"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\kanji.dat"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\radical.dat"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\postal.dat"; DestDir: "{app}"; Flags: ignoreversion
Source: "build32\Debug\mzimeja.ime"; DestDir: "{app}\x86"; Flags: ignoreversion 32bit
Source: "build64\Debug\mzimeja.ime"; DestDir: "{app}\x64"; Flags: ignoreversion 64bit; Check: IsWin64
Source: "build32\Debug\ime_setup32.exe"; DestDir: "{app}"; Flags: ignoreversion 32bit
Source: "build64\Debug\ime_setup64.exe"; DestDir: "{app}"; Flags: ignoreversion 64bit; Check: IsWin64
Source: "build32\Debug\imepad.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build32\Debug\dict_compile.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build32\Debug\verinfo.exe"; DestDir: "{app}"; Flags: ignoreversion

; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\READMEJP.txt"; Filename: "{app}\READMEJP.txt"
Name: "{group}\LICENSE.txt"; Filename: "{app}\LICENSE.txt"
Name: "{group}\HISTORY.txt"; Filename: "{app}\HISTORY.txt"
Name: "{group}\MZ-IMEパッド"; Filename: "{app}\imepad.exe"
Name: "{group}\バージョン情報"; Filename: "{app}\verinfo.exe"
Name: "{group}\アンインストール"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\ime_setup32.exe"; Parameters: "/i"

[UninstallRun]
Filename: "{app}\ime_setup32.exe"; Parameters: "/u"
