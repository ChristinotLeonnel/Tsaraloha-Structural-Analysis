; Installateur Windows (Inno Setup 6) commun à TSA et TSALab, construit à partir du paquet vérifié
; <build>/dist/<produit> (cible package_<produit>, tools/verify_package.py). Cible installer_<produit> :
; les valeurs ci-dessous sont passées par CMake (/D...), aucune n'a de valeur par défaut.
;
; Installation par utilisateur (%LOCALAPPDATA%\Programs\<produit>), sans droits administrateur, comme
; l'association de fichiers et l'aperçu (HKCU) que le logiciel enregistre déjà lui-même.

#ifndef Product
  #error Product non défini (passer /DProduct=...)
#endif

[Setup]
AppId={{{#AppId}}
AppName={#Product}
AppVerName={#Product} {#Version}
AppVersion={#Version}
VersionInfoVersion={#NumericVersion}
AppPublisher=Christinot Leonnel Tsaraloha
AppPublisherURL=https://christinotleonnel.github.io/Tsaraloha-Web
AppSupportURL=https://christinotleonnel.github.io/Tsaraloha-Web/support
AppUpdatesURL=https://christinotleonnel.github.io/Tsaraloha-Web/downloads/{#ProductSlug}
DefaultDirName={autopf}\{#Product}
DefaultGroupName={#Product}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
SourceDir={#SourceDir}
OutputDir={#OutputDir}
OutputBaseFilename={#Product}-{#Version}-windows-x64-setup
SetupIconFile={#SetupIcon}
UninstallDisplayIcon={app}\{#Product}.exe
LicenseFile={#LicenseFile}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "association"; Description: "{cm:AssocTask,{#Extension}}"

[CustomMessages]
fr.AssocTask=Ouvrir les fichiers %1 avec {#Product} (aperçu dans l'Explorateur compris)
en.AssocTask=Open %1 files with {#Product} (including Explorer thumbnails)
fr.KeepData=Vos réglages restent dans %1 ; supprimez ce dossier pour les effacer.
en.KeepData=Your settings remain in %1; delete that folder to erase them.

[Files]
; Le paquet entier, tel que vérifié (MANIFEST.json compris) : rien n'est ajouté ni retiré ici.
Source: "*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#Product}"; Filename: "{app}\{#Product}.exe"; AppUserModelID: "{#AppUserModelId}"
Name: "{autodesktop}\{#Product}"; Filename: "{app}\{#Product}.exe"; AppUserModelID: "{#AppUserModelId}"; Tasks: desktopicon

[Registry]
; Mêmes clés que WindowsAssociation.cpp, posées dès l'installation et retirées à la désinstallation.
; La clé de l'extension est partagée avec d'autres logiciels : seules nos valeurs sont supprimées.
Root: HKA; Subkey: "Software\Classes\{#Extension}"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#Extension}"; ValueType: string; ValueName: "Content Type"; ValueData: "{#ContentType}"; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#Extension}"; ValueType: string; ValueName: "PerceivedType"; ValueData: "document"; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#Extension}\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueName: ""; ValueData: "{#FriendlyName}"; Flags: uninsdeletekey; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "{#FriendlyName}"; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#ProgId}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#Product}.exe"",0"; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#ProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#Product}.exe"" ""%1"""; Tasks: association

[Run]
; Aperçu des fichiers dans l'Explorateur : DllRegisterServer de la DLL (HKCU, sans droits administrateur).
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\{#ThumbnailDll}"""; Flags: runhidden; Tasks: association
Filename: "{app}\{#Product}.exe"; Description: "{cm:LaunchProgram,{#Product}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /u ""{app}\{#ThumbnailDll}"""; Flags: runhidden; RunOnceId: "UnregisterThumbnail"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and not UninstallSilent then
    MsgBox(FmtMessage(CustomMessage('KeepData'), [ExpandConstant('{localappdata}\{#DataDir}')]), mbInformation, MB_OK);
end;
