; 基本信息
[Setup]
AppName=FocusLock
AppVersion=1.0
DefaultDirName={pf}\FocusLock
DefaultGroupName=FocusLock
OutputDir=G:\InstallerOutput
OutputBaseFilename=FocusLockSetup
Compression=lzma
SolidCompression=yes

; 安装文件
[Files]
Source: "G:\C++\FocusLock\FocusLock\build\Desktop_Qt_6_10_1_MinGW_64_bit-Release\CountdownLock.exe"; DestDir: "{app}"; Flags: ignoreversion

; 如果有 DLL 或资源文件，也要加上类似的条目
; Source: "G:\InstallerSource\CountdownLock\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

; 快捷方式
[Icons]
Name: "{group}\FocusLock"; Filename: "{app}\CountdownLock.exe"
Name: "{commondesktop}\FocusLock"; Filename: "{app}\CountdownLock.exe"

; 卸载时删除残留文件
[UninstallDelete]
Type: filesandordirs; Name: "{app}"
