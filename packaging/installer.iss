; =============================================================================
; Tsaraloha Structural Analysis (TSA) — Installateur Windows Professionnel
; Compilateur : Inno Setup 6 (ISCC)
; Architecture : Windows 10/11 64 bits (x64)
; Éditeur : Tsaraloha Software
; Site officiel : https://christinotleonnel.github.io/Tsaraloha-Web
; Documentation : https://christinotleonnel.github.io/Tsaraloha-Web/docs
; =============================================================================

#ifndef Product
  #error Product non défini (passer /DProduct=...)
#endif

[Setup]
AppId={{{#AppId}}
AppName={#Product}
AppVerName={#Product} {#Version}
AppVersion={#Version}
VersionInfoVersion={#NumericVersion}
AppPublisher=Tsaraloha Software
AppPublisherURL=https://christinotleonnel.github.io/Tsaraloha-Web
AppSupportURL=https://christinotleonnel.github.io/Tsaraloha-Web/support
AppUpdatesURL=https://christinotleonnel.github.io/Tsaraloha-Web/downloads/{#ProductSlug}

; Répertoire par défaut : %ProgramFiles%\Tsaraloha Software\TSA (si élévation)
; ou %LOCALAPPDATA%\Programs\Tsaraloha Software\TSA (utilisateur standard sans droits admin)
DefaultDirName={autopf}\Tsaraloha Software\{#Product}
DefaultGroupName=Tsaraloha Software\{#Product}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

SourceDir={#SourceDir}
OutputDir={#OutputDir}
OutputBaseFilename={#Product}-{#Version}-windows-x64-setup
SetupIconFile={#SetupIcon}
UninstallDisplayIcon={app}\{#Product}.exe
LicenseFile={#LicenseFile}

; Graphismes officiels TSA
WizardStyle=modern
#if FileExists(SourcePath + "\wizard_banner.bmp")
WizardImageFile={#SourcePath}\wizard_banner.bmp
#endif
#if FileExists(SourcePath + "\wizard_small.bmp")
WizardSmallImageFile={#SourcePath}\wizard_small.bmp
#endif

Compression=lzma2/max
SolidCompression=yes
ChangesAssociations=yes
CloseApplications=yes
ExtraDiskSpaceRequired=266453760

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Messages]
fr.WelcomeLabel1=Bienvenue dans l'assistant d'installation de %1
fr.WelcomeLabel2=Cet assistant va installer %1 version {#Version} sur votre ordinateur.%n%nLogiciel professionnel d'ingénierie civile pour la modélisation 3D, le maillage par éléments finis et le calcul de structures (OpenCASCADE, OpenSees, Gmsh).%n%nÉditeur : Tsaraloha Software%nSite officiel : https://christinotleonnel.github.io/Tsaraloha-Web%n%nIl est recommandé de fermer toutes les applications ouvertes avant de continuer.
fr.ClickNext=Cliquez sur Suivant pour continuer, ou sur Annuler pour quitter l'assistant.
fr.FinishedHeadingLabel=Installation de %1 terminée avec succès
fr.FinishedLabel=%1 a été installé avec succès sur votre ordinateur.%n%nPour consulter la documentation officielle :%nhttps://christinotleonnel.github.io/Tsaraloha-Web/docs

en.WelcomeLabel1=Welcome to the %1 Setup Wizard
en.WelcomeLabel2=This will install %1 version {#Version} on your computer.%n%nProfessional civil engineering desktop software for 3D structural modeling, finite element meshing, and analysis (OpenCASCADE, OpenSees, Gmsh).%n%nPublisher: Tsaraloha Software%nOfficial website: https://christinotleonnel.github.io/Tsaraloha-Web%n%nIt is recommended that you close all other applications before continuing.
en.ClickNext=Click Next to continue, or Cancel to exit Setup.
en.FinishedHeadingLabel=Completing the %1 Setup Wizard
en.FinishedLabel=%1 has been successfully installed on your computer.%n%nTo browse official documentation:%nhttps://christinotleonnel.github.io/Tsaraloha-Web/docs

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "association"; Description: "{cm:AssocTask,{#Extension}}"

[CustomMessages]
fr.AssocTask=Associer les projets %1 à {#Product} (aperçu dans l'Explorateur Windows compris)
en.AssocTask=Associate %1 projects with {#Product} (including Windows Explorer thumbnail preview)
fr.KeepData=Vos projets personnels .tsa et réglages sont conservés dans :%n%1%nLa désinstallation n'a supprimé aucun de vos projets.
en.KeepData=Your personal .tsa projects and configuration remain intact in:%n%1%nUninstall did not remove any of your projects.
fr.PrevVersionFound=Une installation existante de {#Product} (version %1) a été détectée.%nElle sera mise à jour proprement en conservant vos réglages et vos projets.
en.PrevVersionFound=An existing installation of {#Product} (version %1) was detected.%nIt will be upgraded cleanly while preserving your settings and projects.
fr.MissingExecutable=Erreur : le fichier principal %1 n'a pas été trouvé après la copie. L'installation a échoué.
en.MissingExecutable=Error: the main executable %1 was not found after file copy. Setup has failed.

[Files]
; Le paquet entier certifié par verify_package.py (MANIFEST.json compris)
Source: "*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#Product}"; Filename: "{app}\{#Product}.exe"; AppUserModelID: "{#AppUserModelId}"
Name: "{autodesktop}\{#Product}"; Filename: "{app}\{#Product}.exe"; AppUserModelID: "{#AppUserModelId}"; Tasks: desktopicon

[Registry]
; Enregistrement du type MIME et de l'extension de projet
Root: HKA; Subkey: "Software\Classes\{#Extension}"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#Extension}"; ValueType: string; ValueName: "Content Type"; ValueData: "{#ContentType}"; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#Extension}"; ValueType: string; ValueName: "PerceivedType"; ValueData: "document"; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#Extension}\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueName: ""; ValueData: "{#FriendlyName}"; Flags: uninsdeletekey; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "{#FriendlyName}"; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#ProgId}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#Product}.exe"",0"; Tasks: association
Root: HKA; Subkey: "Software\Classes\{#ProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#Product}.exe"" ""%1"""; Tasks: association

[Run]
; Fournisseur de vignettes dans l'Explorateur Windows
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\{#ThumbnailDll}"""; Flags: runhidden; Tasks: association
; Lancement facultatif de TSA à la fin de l'installation
Filename: "{app}\{#Product}.exe"; Description: "{cm:LaunchProgram,{#Product}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /u ""{app}\{#ThumbnailDll}"""; Flags: runhidden; RunOnceId: "UnregisterThumbnail"

[Code]
// Détection de version préalable et vérification d'intégrité
function InitializeSetup(): Boolean;
var
  PrevVersion: String;
begin
  Result := True;
  if RegQueryStringValue(HKA, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\' + ExpandConstant('{#AppId}') + '_is1', 'DisplayVersion', PrevVersion) then
  begin
    if not WizardSilent then
      Log(FmtMessage(CustomMessage('PrevVersionFound'), [PrevVersion]));
  end;
end;

// Vérification stricte que l'exécutable TSA.exe est réellement extrait avant de clôturer
procedure CurStepChanged(CurStep: TSetupStep);
var
  MainExe: String;
begin
  if CurStep = ssPostInstall then
  begin
    MainExe := ExpandConstant('{app}\{#Product}.exe');
    if not FileExists(MainExe) then
    begin
      RaiseException(FmtMessage(CustomMessage('MissingExecutable'), [MainExe]));
    end;
  end;
end;

// Message à la désinstallation rappelant la préservation des données utilisateur
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and not UninstallSilent then
    MsgBox(FmtMessage(CustomMessage('KeepData'), [ExpandConstant('{localappdata}\{#DataDir}')]), mbInformation, MB_OK);
end;
