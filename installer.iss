; ExtOnly インストーラー定義（Inno Setup 6）
; 使い方: build-installer.ps1 を実行する（または ISCC.exe installer.iss）
;
; 注意: CLSID は 64bit のレジストリビューに書く必要がある。
;       そのため ArchitecturesInstallIn64BitMode を指定している。

#define AppName "ExtOnly"
#define AppVersion "1.0.0"
#define AppPublisher "ExtOnly"
#define AppFileName "ExtOnly.dll"

[Setup]
AppId={{86C28C61-1E17-4016-BD15-42AF0F0D9E2F}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=dist
OutputBaseFilename=ExtOnlySetup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppFileName}
VersionInfoVersion={#AppVersion}.0
VersionInfoDescription={#AppName} Setup

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Files]
Source: "build\ExtOnly.dll"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
; 「ファイルを右クリック → 選択」を追加（自分専用・管理者権限不要）
Root: HKCU; Subkey: "Software\Classes\*\shellex\ContextMenuHandlers\ExtOnly"; ValueType: string; ValueName: ""; ValueData: "{{B06D4875-833C-4F8E-85A7-8811748382EE}"; Flags: uninsdeletekey
; シェル拡張本体（COM としての登録）
Root: HKCU; Subkey: "Software\Classes\CLSID\{{B06D4875-833C-4F8E-85A7-8811748382EE}"; ValueType: string; ValueName: ""; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\CLSID\{{B06D4875-833C-4F8E-85A7-8811748382EE}\InprocServer32"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppFileName}"
Root: HKCU; Subkey: "Software\Classes\CLSID\{{B06D4875-833C-4F8E-85A7-8811748382EE}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"

[Code]
procedure SHChangeNotify(wEventId: Longint; uFlags: Cardinal; dwItem1, dwItem2: Cardinal);
  external 'SHChangeNotify@shell32.dll stdcall';

function UninstallKeyName(): String;
begin
  Result := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{86C28C61-1E17-4016-BD15-42AF0F0D9E2F}_is1';
end;

function InitializeSetup(): Boolean;
var
  UninstallString: String;
  ResultCode: Integer;
begin
  Result := True;

  // 既にインストール済みなら、インストーラーではなくアンインストーラーとして動く
  if (not WizardSilent) and RegKeyExists(HKCU, UninstallKeyName()) then
  begin
    if MsgBox('ExtOnly は既にインストールされています。' + #13#10 + #13#10 +
              'アンインストールしますか？', mbConfirmation, MB_YESNO) = IDYES then
    begin
      if RegQueryStringValue(HKCU, UninstallKeyName(), 'UninstallString', UninstallString) then
        Exec(RemoveQuotes(UninstallString), '', '', SW_SHOWNORMAL, ewNoWait, ResultCode)
      else
        MsgBox('アンインストーラーが見つかりませんでした。', mbError, MB_OK);
    end;

    // このインストーラーではインストールしない
    Result := False;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    SHChangeNotify($08000000, 0, 0, 0);  // SHCNE_ASSOCCHANGED: シェルに変更を通知
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
  begin
    WizardForm.FinishedLabel.Caption := WizardForm.FinishedLabel.Caption + #13#10#13#10 +
      '※ うまく反映されない場合は、エクスプローラーを再起動するか、Windows を再起動してください。';
  end;
end;
