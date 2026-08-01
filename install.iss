; T9GamepadIME 安装脚本（Inno Setup 6）
; 编译：ISCC.exe install.iss
; 输出：installer\T9GamepadIME-Setup.exe

#define MyAppName "T9GamepadIME"
#define MyAppVersion "1.0.0"
#define MyAppExeName "t9ime.exe"

[Setup]
AppId={{A1B2C3D4-5E6F-4A8B-9C0D-1E2F3A4B5C6D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=T9GamepadIME
AppComments=T9 手柄中文拼音输入法
DefaultDirName={localappdata}\Programs\{#MyAppName}
DisableProgramGroupPage=yes
OutputDir=installer
OutputBaseFilename=T9GamepadIME-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 用户级安装：无需管理员，且程序可写安装目录（日志/用户词典）
PrivilegesRequired=lowest
SetupIconFile=assets\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "build_test\Release\t9ime.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "data\dict_pinyin.dat"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "config.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "NOTICE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

; 卸载时删除整个应用目录（含程序运行时生成的用户词典/日志）
[UninstallDelete]
Type: filesandordirs; Name: "{app}"
