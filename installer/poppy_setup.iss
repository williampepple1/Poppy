#define MyAppName "Poppy"
#ifndef MyAppVersion
#define MyAppVersion "1.4.1"
#endif
#define MyAppPublisher "William Pepple"
#define MyAppURL "https://github.com/williampepple1/Poppy"
#define MyAppExeName "poppy.exe"

[Setup]
AppId={{D37E84C1-87F9-459E-906D-1E76B46274D8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=..\LICENSE
OutputDir=..\release-installer
OutputBaseFilename=Poppy-windows-x64-setup
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associatebru"; Description: "Associate .bru (Bruno collection) files with Poppy"; GroupDescription: "File associations:"

[Files]
Source: "..\dist\Poppy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
Root: HKA; Subkey: "Software\Classes\.bru"; ValueType: string; ValueName: ""; ValueData: "Poppy.Collection"; Flags: uninsdeletevalue; Tasks: associatebru
Root: HKA; Subkey: "Software\Classes\Poppy.Collection"; ValueType: string; ValueName: ""; ValueData: "Poppy Collection File"; Flags: uninsdeletekey; Tasks: associatebru
Root: HKA; Subkey: "Software\Classes\Poppy.Collection\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: associatebru
Root: HKA; Subkey: "Software\Classes\Poppy.Collection\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: associatebru

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{#MyAppName} CLI"; Filename: "{app}\poppy-cli.exe"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
