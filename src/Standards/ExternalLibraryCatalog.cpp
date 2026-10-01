#include "ExternalLibraryCatalog.h"

namespace TSA::Standards
{

ExternalLibraryCatalog& ExternalLibraryCatalog::instance()
{
    static ExternalLibraryCatalog s_instance;
    return s_instance;
}

ExternalLibraryCatalog::ExternalLibraryCatalog()
{
    initializeCatalog();
}

std::optional<ExternalLibraryInfo> ExternalLibraryCatalog::findByName(const std::string& name) const
{
    for (const auto& lib : m_libraries)
    {
        if (lib.name == name) return lib;
    }
    return std::nullopt;
}

QString ExternalLibraryCatalog::generateDocumentationMarkdown() const
{
    QString md;
    md += QStringLiteral("# Répertoire des Bibliothèques Externes Tierces (TSA)\n\n");
    md += QStringLiteral("| Bibliothèque | Version | Licence | Rôle Principal | Couche d'Adaptation | Statut Test |\n");
    md += QStringLiteral("| :--- | :---: | :--- | :--- | :--- | :--- |\n");

    for (const auto& lib : m_libraries)
    {
        md += QStringLiteral("| **%1** | %2 | %3 | %4 | `%5` | `%6` |\n")
                  .arg(QString::fromStdString(lib.name))
                  .arg(QString::fromStdString(lib.version))
                  .arg(QString::fromStdString(lib.license))
                  .arg(QString::fromStdString(lib.purpose))
                  .arg(QString::fromStdString(lib.interfaceUsed))
                  .arg(QString::fromStdString(lib.verificationTest));
    }

    md += QStringLiteral("\n## Détail des Contraintes d'Isolation\n\n");
    for (const auto& lib : m_libraries)
    {
        md += QStringLiteral("### %1 (v%2)\n").arg(QString::fromStdString(lib.name), QString::fromStdString(lib.version));
        md += QStringLiteral("- **Mission** : %1\n").arg(QString::fromStdString(lib.purpose));
        md += QStringLiteral("- **Responsabilité** : %1\n").arg(QString::fromStdString(lib.responsibility));
        md += QStringLiteral("- **Contraintes d'architecture** : %1\n").arg(QString::fromStdString(lib.constraints));
        md += QStringLiteral("- **Compatibilité** : %1\n\n").arg(QString::fromStdString(lib.compatibilityNotes));
    }

    return md;
}

void ExternalLibraryCatalog::initializeCatalog()
{
    m_libraries.clear();

    // 1. Qt 6
    m_libraries.push_back({
        "Qt",
        "6.2+",
        "LGPLv3 / Commercial",
        "Framework applicatif, interface graphique utilisateur (Widgets, Ribbon, Docks, Dialogs), signaux/slots et gestion des processus asynchrones (QProcess).",
        "src/UI/, src/App/",
        "Gère exclusivement l'IHM, les événements utilisateurs et la communication inter-composants. Le code UI ne doit jamais implémenter directement des calculs structuraux ni manipuler OpenSees sans passer par les commandes.",
        "Strictement interdit d'inclure des en-têtes Qt Widgets dans src/Model/. TSA_Core ne dépend que de QtCore/QtGui/QtWidgets pour la persistance et les abstractions de base.",
        "MSVC 2022/2026 (x64), MinGW-w64 (GCC 11+), Clang 15+. Déploiement automatique via windeployqt.",
        "TSA_WindowManagerTests, TSA_ViewerTests"
    });

    // 2. OpenCASCADE Technology (OCCT)
    m_libraries.push_back({
        "OpenCASCADE",
        "8.0.1",
        "LGPLv2.1 avec exception",
        "Moteur géométrique B-Rep, topologie 3D solide/filaire, et visualiseur 3D OpenGL interactif (V3d_Viewer, AIS_InteractiveContext).",
        "src/Viewer/OccView, src/Geometry/*Geometry",
        "Construit la représentation visuelle 3D et gère l'affichage accéléré par le GPU. OCCT ne détient aucune vérité métier : les TopoDS_Shape et AIS_Shape sont esclaves des données de TSA::Model.",
        "Le modèle structural (src/Model/) ne doit jamais stocker de TopoDS_Shape ni de handle AIS_Shape. Toutes les géométries sont reconstruites à la volée par les constructeurs de src/Geometry/.",
        "Compilé en x64 vc14-64 avec en-têtes déclarés SYSTEM dans CMake pour éliminer les avertissements /W4.",
        "TSA_ViewerTests, TSA_CoordinatesTests"
    });

    // 3. OpenSees
    m_libraries.push_back({
        "OpenSees",
        "3.4.0 à 3.8.0+",
        "UC Berkeley Open-Source License",
        "Solveur par éléments finis (FEM) non-linéaires statiques, dynamiques, modaux et transitoires pour l'ingénierie des structures.",
        "src/Analysis/OpenSeesAdapter, src/Analysis/OpenSeesAnalysisBuilder, src/Analysis/OpenSeesSolver",
        "Calcul numérique des déplacements nodaux, réactions aux appuis et efforts internes le long des éléments structuraux.",
        "OpenSees est isolé dans un processus externe non bloquant (QProcess). Il communique via des scripts Tcl générés et des fichiers de sortie recorder. Aucun composant applicatif autre que src/Analysis/ ne doit dépendre d'OpenSees.",
        "Compatible tout binaire OpenSees.exe officiel Windows x64. Détection et téléchargement automatique intégrés dans OpenSeesManager.",
        "TSA_OpenSeesTests (10 cas de validation analytique)"
    });

    // 4. FreeType (dépendance OCCT)
    m_libraries.push_back({
        "FreeType",
        "2.13.x",
        "FreeType License (FTL) / GPLv2",
        "Rendu et vectorisation des polices de caractères pour les étiquettes de cotation et les textes 3D dans le viewport.",
        "Utilisé en interne par OpenCASCADE TKService",
        "Affichage des labels de nœuds, des valeurs d'efforts sur les diagrammes 3D et des axes de coordonnées.",
        "Géré automatiquement par les DLLs 3rdparty fournies avec OCCT.",
        "Lien statique / dynamique vc14-64.",
        "TSA_ViewerTests"
    });

    // 5. Intel Threading Building Blocks (TBB)
    m_libraries.push_back({
        "Intel TBB",
        "2021.x",
        "Apache 2.0",
        "Bibliothèque de parallélisme mémoire partagée multi-cœurs pour l'accélération du maillage géométrique et du rendu.",
        "Utilisé en interne par OpenCASCADE TKMesh",
        "Accélère la tessellation polygonale des surfaces et solides OCCT.",
        "Aucune utilisation directe dans le code TSA pour préserver la portabilité.",
        "DLL tbb12.dll déployée automatiquement avec l'exécutable.",
        "TSA_ViewerTests"
    });
}

} // namespace TSA::Standards
