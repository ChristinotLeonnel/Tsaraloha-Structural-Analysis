#pragma once

// Identité du produit TSA (Tsaraloha Structural Analysis).
//
// Les sources de src/ sont la base technique commune de TSA et de TSALab : tout ce qui distingue
// les deux logiciels (nom, version, extension, ProgID, signature de fichier, emplacements QSettings,
// icônes) est lu ici, jamais écrit en dur dans src/. Chaque produit fournit son propre
// product/ProductIdentity.h (même API) ; CMake ajoute le dossier product/ du produit compilé au
// chemin d'inclusion. Toujours inclure avec des chevrons : #include <ProductIdentity.h>.
//
// C++ pur (aucune dépendance Qt) : inclus aussi par la DLL Explorateur (ShellExtension).
// Les fonctions utilitaires Qt sont dans src/App/ProductInfo.h.

#include <cstdint>

namespace TSA::Product
{

// --- Application -----------------------------------------------------------------------------
inline constexpr char kName[] = "TSA";
inline constexpr char kLongName[] = "Tsaraloha Structural Analysis";
inline constexpr char kKind[] = "logiciel";                              // « … intégré à TSA, logiciel de modélisation »
inline constexpr char kVersion[] = "0.1.0";
inline constexpr char kReportVersion[] = "1.0.0";                        // version affichée dans la NDC
inline constexpr char kBadgeVersion[] = "CAD v1.0";                      // badge du viewport
inline constexpr char kMainWindowTitle[] = "TSA - 3D Structural Modeler";
inline constexpr char kConsoleBanner[] = "TSA Structural Analysis Modeler initialisé avec succès.";
inline constexpr char kStartCenterSubtitle[] = "Structural Analysis";
inline constexpr char kAboutIntroHtml[] = "";                             // paragraphe « À propos » propre au produit
inline constexpr char kPlatformLabel[] = "TSA - Plateforme de Conception & Calcul de Structures 3D";
// Documentation en ligne officielle (TSA Web, GitHub Pages) : seule destination autorisée des boutons
// Aide (src/Help/HelpTopics). Adresse reprise de la configuration du site (src/config/deployment.json
// du dépôt Tsaraloha-Web) ; HTTPS obligatoire, sans barre oblique finale.
inline constexpr char kDocsBaseUrl[] = "https://christinotleonnel.github.io/Tsaraloha-Web";
inline constexpr char kHttpUserAgent[] = "TSA-Structural-Analysis";
inline constexpr char kIfcOriginatingSystem[] = "TSA - Tsaraloha Structural Analysis";
inline constexpr char kPreviewBadge[] = "TSA 3D";
inline constexpr char kAiModelsSource[] = "TSA";                          // modèles IA téléchargés par ce produit
inline constexpr wchar_t kAppUserModelId[] = L"TSAEngineering.TSA.StructuralModeler.1.0";

// --- QSettings (organisation, application) ----------------------------------------------------
// TSA conserve ses emplacements historiques (paramètres existants des utilisateurs).
inline constexpr char kOrganizationName[] = "TSA Engineering";          // QCoreApplication + projets récents
inline constexpr char kOrganizationDomain[] = "";
inline constexpr char kSettingsApplication[] = "TSA";
inline constexpr char kOpenSeesSettingsOrganization[] = "TSA";
inline constexpr char kOpenSeesSettingsApplication[] = "TSA_StructuralAnalysis";
inline constexpr char kLayoutSettingsOrganization[] = "Tsaraloha";
inline constexpr char kLayoutSettingsApplication[] = "TSA";

// --- Projets ---------------------------------------------------------------------------------
inline constexpr char kProjectExtension[] = ".tsa";                       // format natif
inline constexpr char kLegacyExtension[] = "";                            // autre format ouvert en import (aucun)
inline constexpr char kLegacyProductName[] = "";
inline constexpr char kProgId[] = "TSA.Project";
inline constexpr char kProjectFriendlyName[] = "TSA Project File";
inline constexpr char kProjectContentType[] = "application/x-tsa-project";
inline constexpr char kDocumentsFolder[] = "TSA";                         // Documents/TSA
inline constexpr char kDefaultProjectName[] = "Projet TSA";
inline constexpr char kDefaultAuthor[] = "TSA User";

// Signatures de fichier (Little-Endian) : 'TSAF' = 0x46415354.
inline constexpr std::uint32_t kNativeFileMagic = 0x46415354;
inline constexpr std::uint32_t kLegacyFileMagic = 0;                      // 0 : aucun format importé

// --- Ressources (resources.qrc du produit) ----------------------------------------------------
inline constexpr char kIconIco[] = ":/icons/TSA.ico";
inline constexpr char kIconSvg[] = ":/icons/TSA.svg";
inline constexpr char kIconSvgOnDark[] = ":/icons/TSA_light.svg";        // logo sur fond sombre
inline constexpr char kGlyphSvg[] = ":/icons/TSA_glyph.svg";

// --- Extension Explorateur (DLL de miniatures) -------------------------------------------------
inline constexpr char kThumbnailProviderDll[] = "TSAThumbnailProvider.dll";

} // namespace TSA::Product
