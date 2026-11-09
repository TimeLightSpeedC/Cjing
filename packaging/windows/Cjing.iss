; 由 scripts/package-windows.ps1 调用；GitHub Actions 也可复用
; 生成安装程序：dist\Cjing-<版本>-windows-setup.exe

#define MyAppName "井字棋"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "TimeLightSpeedC"
#define MyAppExeName "CjingApp.exe"

[Setup]
AppId={{6F0B6D1E-3C55-4E2A-9B7E-5D1C2A8F4B31}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Cjing
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=Cjing-{#MyAppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
UninstallDisplayName={#MyAppName}

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务："

[Files]
Source: "stage\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "运行{#MyAppName}"; Flags: nowait postinstall skipifsilent
