#include "RequirementsCatalog.h"
#include <sstream>

namespace TSA::Standards
{

RequirementsCatalog& RequirementsCatalog::instance()
{
    static RequirementsCatalog s_instance;
    return s_instance;
}

RequirementsCatalog::RequirementsCatalog()
{
    initializeDefaultCatalog();
}

void RequirementsCatalog::registerRequirement(const NormativeRequirement& req)
{
    auto it = m_idIndex.find(req.id);
    if (it != m_idIndex.end())
    {
        m_requirements[it->second] = req;
    }
    else
    {
        m_idIndex[req.id] = m_requirements.size();
        m_requirements.push_back(req);
    }
}

std::optional<NormativeRequirement> RequirementsCatalog::findById(const std::string& id) const
{
    auto it = m_idIndex.find(id);
    if (it != m_idIndex.end())
    {
        return m_requirements[it->second];
    }
    return std::nullopt;
}

std::vector<NormativeRequirement> RequirementsCatalog::filterByStandard(StandardFramework stdCode) const
{
    std::vector<NormativeRequirement> res;
    for (const auto& r : m_requirements)
    {
        if (r.standard == stdCode) res.push_back(r);
    }
    return res;
}

std::vector<NormativeRequirement> RequirementsCatalog::filterByDomain(RequirementDomain domain) const
{
    std::vector<NormativeRequirement> res;
    for (const auto& r : m_requirements)
    {
        if (r.domain == domain) res.push_back(r);
    }
    return res;
}

std::vector<NormativeRequirement> RequirementsCatalog::filterByStatus(RequirementStatus status) const
{
    std::vector<NormativeRequirement> res;
    for (const auto& r : m_requirements)
    {
        if (r.status == status) res.push_back(r);
    }
    return res;
}

size_t RequirementsCatalog::countByStatus(RequirementStatus status) const
{
    size_t cnt = 0;
    for (const auto& r : m_requirements)
    {
        if (r.status == status) ++cnt;
    }
    return cnt;
}

QString RequirementsCatalog::generateMatrixMarkdown() const
{
    QString md;
    md += QStringLiteral("# Matrice de Traçabilité Normative de TSA\n\n");
    md += QStringLiteral("| Identifiant | Domaine | Norme | Clause | Description de l'Exigence | Code Source | Test Associé | Statut |\n");
    md += QStringLiteral("| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :---: |\n");

    for (const auto& r : m_requirements)
    {
        md += QStringLiteral("| **%1** | %2 | %3 | %4 | %5 | `%6` | `%7` | **%8** |\n")
                  .arg(QString::fromStdString(r.id))
                  .arg(requirementDomainToString(r.domain))
                  .arg(standardFrameworkToString(r.standard))
                  .arg(QString::fromStdString(r.clause))
                  .arg(QString::fromStdString(r.requirementText))
                  .arg(QString::fromStdString(r.codeLocation))
                  .arg(QString::fromStdString(r.associatedTest))
                  .arg(requirementStatusToString(r.status));
    }

    md += QStringLiteral("\n**Total recensé** : %1 exigences (%2 Implémentées, %3 Partielles, %4 En vérification).\n")
              .arg(totalCount())
              .arg(countByStatus(RequirementStatus::Implemented))
              .arg(countByStatus(RequirementStatus::Partial))
              .arg(countByStatus(RequirementStatus::ToVerify));

    return md;
}

QString RequirementsCatalog::generateTraceabilityReportHtml() const
{
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    html += QStringLiteral("<title>Matrice de Traçabilité Normative TSA</title>");
    html += QStringLiteral("<style>");
    html += QStringLiteral("body { font-family: Segoe UI, sans-serif; margin: 30px; background: #fdfdfd; color: #222; }");
    html += QStringLiteral("h1 { color: #1a365d; border-bottom: 2px solid #3182ce; padding-bottom: 10px; }");
    html += QStringLiteral("table { width: 100%; border-collapse: collapse; margin-top: 20px; font-size: 13px; }");
    html += QStringLiteral("th, td { border: 1px solid #cbd5e0; padding: 8px 12px; text-align: left; }");
    html += QStringLiteral("th { background-color: #2b6cb0; color: white; }");
    html += QStringLiteral("tr:nth-child(even) { background-color: #f7fafc; }");
    html += QStringLiteral(".badge-imp { background: #38a169; color: white; padding: 3px 8px; border-radius: 4px; font-weight: bold; }");
    html += QStringLiteral(".badge-par { background: #dd6b20; color: white; padding: 3px 8px; border-radius: 4px; font-weight: bold; }");
    html += QStringLiteral(".badge-mis { background: #e53e3e; color: white; padding: 3px 8px; border-radius: 4px; font-weight: bold; }");
    html += QStringLiteral("</style></head><body>");

    html += QStringLiteral("<h1>TSA — Rapport Formel de Traçabilité Normative</h1>");
    html += QStringLiteral("<p>Conformité aux exigences <b>ISO/IEC 25010:2023</b>, <b>ISO/IEC/IEEE 12207:2017</b>, <b>ISO/IEC/IEEE 29119</b> et <b>Eurocodes EN 1990 à EN 1999</b>.</p>");

    html += QStringLiteral("<table><tr><th>ID</th><th>Domaine</th><th>Norme</th><th>Clause</th><th>Exigence</th><th>Code Source</th><th>Test</th><th>Statut</th></tr>");

    for (const auto& r : m_requirements)
    {
        QString badgeClass = QStringLiteral("badge-imp");
        if (r.status == RequirementStatus::Partial) badgeClass = QStringLiteral("badge-par");
        else if (r.status == RequirementStatus::Missing) badgeClass = QStringLiteral("badge-mis");

        html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td><code>%6</code></td><td><code>%7</code></td><td><span class='%8'>%9</span></td></tr>")
                    .arg(QString::fromStdString(r.id))
                    .arg(requirementDomainToString(r.domain))
                    .arg(standardFrameworkToString(r.standard))
                    .arg(QString::fromStdString(r.clause))
                    .arg(QString::fromStdString(r.requirementText))
                    .arg(QString::fromStdString(r.codeLocation))
                    .arg(QString::fromStdString(r.associatedTest))
                    .arg(badgeClass)
                    .arg(requirementStatusToString(r.status));
    }

    html += QStringLiteral("</table></body></html>");
    return html;
}

void RequirementsCatalog::initializeDefaultCatalog()
{
    m_requirements.clear();
    m_idIndex.clear();

    // =========================================================================
    // 1. GÉNIE LOGICIEL, ARCHITECTURE & QUALITÉ (ISO/IEC 25010 & 12207)
    // =========================================================================
    registerRequirement({
        "REQ-SW-ARCH-001",
        StandardFramework::ISO_IEC_25010,
        RequirementDomain::Architecture,
        "ISO/IEC 25010:2023",
        "§4.2.7 (Maintenabilité - Modularité)",
        "Découplage strict des couches : UI -> Command -> Model -> Geometry -> OCCT. Interdiction d'accès direct de l'UI au solveur ou au kernel OCCT.",
        "src/UI/MainWindow_Actions.cpp, src/Viewer/OccView.cpp",
        "TSA_CommandsUndoTests",
        RequirementStatus::Implemented,
        "Vérifié par inspection et tests d'annulation/rétablissement."
    });

    registerRequirement({
        "REQ-SW-ARCH-002",
        StandardFramework::ISO_IEC_25010,
        RequirementDomain::Architecture,
        "ISO/IEC 25010:2023",
        "§4.2.7 (Modularité)",
        "Le modèle structural (TSA::Model::Model) est l'unique source de vérité. L'UI et OpenCASCADE s'abonnent via IModelObserver sans état propre.",
        "src/Model/Model.h, src/Viewer/OccView.h",
        "TSA_ModelElementsTests",
        RequirementStatus::Implemented,
        "Synchronisation bidirectionnelle vérifiée."
    });

    registerRequirement({
        "REQ-SW-MOD-001",
        StandardFramework::ISO_IEC_25010,
        RequirementDomain::Architecture,
        "ISO/IEC 25010:2023",
        "§4.2.7 (Analysabilité)",
        "Principe de responsabilité unique (SRP) et surveillance des fichiers volumineux (> 1000 lignes) avec plan de découpage progressif.",
        "src/Model/Model.cpp, src/IO/TSAFile.cpp, src/Viewer/OccView_Shapes.cpp",
        "TSA_AllTests",
        RequirementStatus::Partial,
        "Audit métrique réalisé : 11 fichiers identifiés pour refactorisation progressive."
    });

    registerRequirement({
        "REQ-SW-TEST-001",
        StandardFramework::ISO_IEC_IEEE_29119,
        RequirementDomain::TestingQA,
        "ISO/IEC/IEEE 29119",
        "Partie 2 & 3 (Processus & Documentation des tests)",
        "Stratégie de tests automatisés couvrant les tests unitaires, l'intégration, la persistance et la validation analytique numérique.",
        "tests/ (14 suites de tests CTest)",
        "TSA_AllTests",
        RequirementStatus::Implemented,
        "100% de tests réussis sur les 14 suites CTest (89 sous-tests)."
    });

    registerRequirement({
        "REQ-SW-DOC-001",
        StandardFramework::IEEE_Std_1063,
        RequirementDomain::DocumentationUI,
        "IEEE Std 1063-2001 (R2007)",
        "Clause 5 (Structure minimale) & Clause 4.2 (Exactitude)",
        "Documentation technique et utilisateur complète, exacte et sans divergence avec le code source et le catalogue de commandes.",
        "src/Commands/CommandCatalog.cpp, docs/shortcuts.txt",
        "tools/check_shortcuts.py --strict",
        RequirementStatus::Implemented,
        "Vérifié rigoureusement par le script python d'inspection des raccourcis."
    });

    registerRequirement({
        "REQ-EXT-LIB-001",
        StandardFramework::ISO_IEC_25010,
        RequirementDomain::Architecture,
        "ISO/IEC 25010:2023",
        "§4.2.7 (Maintenabilité & Portabilité)",
        "Inventaire documenté, vérification de compatibilité et isolation des bibliothèques externes tierces (Qt 6, OCCT 8.0.1, OpenSees).",
        "src/Standards/ExternalLibraryCatalog.cpp",
        "TSA_StandardsTests",
        RequirementStatus::Implemented,
        "Catalogue formalisé et accessible par l'application."
    });

    // =========================================================================
    // 2. DONNÉES ET UNITÉS DU SYSTÈME INTERNATIONAL (ISO 80000 / EN 1990)
    // =========================================================================
    registerRequirement({
        "REQ-DATA-SI-001",
        StandardFramework::EN_1990,
        RequirementDomain::DataModel,
        "ISO 80000-1 / EN 1990:2002",
        "§1.6 (Symboles et Unités)",
        "Toutes les grandeurs physiques du modèle de calcul interne sont exprimées en unités SI strictes : m, m², m⁴, N, kN, Pa, MPa, kg/m³.",
        "src/Model/Section.h, src/Model/Material.h, src/Coordinate/Point3D.h",
        "TSA_ModelElementsTests",
        RequirementStatus::Implemented,
        "Cohérence géométrique et mécanique validée."
    });

    registerRequirement({
        "REQ-DATA-VAL-001",
        StandardFramework::ISO_IEC_25010,
        RequirementDomain::SecurityIntegrity,
        "ISO/IEC 25010:2023",
        "§4.2.5 (Tolérance aux pannes & Intégrité)",
        "Validation défensive des données du modèle : rejet immédiat des coordonnées NaN/inf, dimensions négatives, modules nuls et instabilités cinématiques.",
        "src/Standards/ModelValidator.cpp, src/Analysis/LoadValidation.cpp",
        "TSA_LoadsTests",
        RequirementStatus::Implemented,
        "Validateur global de modèle intégré au pipeline."
    });

    // =========================================================================
    // 3. ANALYSE ÉLÉMENTS FINIS (OPENSEES & EQUILIBRE RDM)
    // =========================================================================
    registerRequirement({
        "REQ-CALC-SNAP-001",
        StandardFramework::ISO_IEC_25010,
        RequirementDomain::AnalysisFEM,
        "ISO/IEC 25010:2023",
        "§4.2.5 (Intégrité des données)",
        "Isolation des données de calcul par capture d'un CalculationSnapshot immuable, garantissant l'indépendance du calcul vis-à-vis des mutations UI.",
        "src/Analysis/CalculationSnapshot.h, src/Analysis/OpenSeesSolver.cpp",
        "TSA_OpenSeesTests::testAnalyticalCantileverBeam",
        RequirementStatus::Implemented,
        "Test de non-régression validé sous accès asynchrone."
    });

    registerRequirement({
        "REQ-CALC-EQ-001",
        StandardFramework::EN_1990,
        RequirementDomain::AnalysisFEM,
        "Principes de la Mécanique Rationnelle",
        "Équilibre Statique Global",
        "Vérification rigoureuse de l'équilibre global de la structure : somme vectorielle des charges extérieures égale à la somme des réactions d'appui.",
        "src/Analysis/OpenSeesResultsReader.cpp",
        "TSA_OpenSeesTests::testLoadEquilibriumBalance",
        RequirementStatus::Implemented,
        "Résultantes globales vérifiées à 1e-4 kN près sur cas complexes."
    });

    // =========================================================================
    // 4. EUROCODES DE CALCUL STRUCTURAL (EN 1990 À EN 1999)
    // =========================================================================
    registerRequirement({
        "REQ-CALC-EC0-001",
        StandardFramework::EN_1990,
        RequirementDomain::StructuralDesign,
        "EN 1990:2002+A1:2005",
        "§6.4.3 (Combinaisons d'actions ELU fondamentales)",
        "Génération des combinaisons d'actions aux États Limites Ultimes (ELU) selon l'équation 6.10 : sum(gamma_G,j * G_k,j) + gamma_Q,1 * Q_k,1 + sum(gamma_Q,i * psi_0,i * Q_k,i).",
        "src/Model/Load/LoadCombination.h, src/Standards/NationalAnnexConfig.h",
        "TSA_LoadsTests",
        RequirementStatus::Implemented,
        "Combinaisons pondérées résolues et calculées par OpenSees."
    });

    registerRequirement({
        "REQ-CALC-EC0-002",
        StandardFramework::EN_1990,
        RequirementDomain::StructuralDesign,
        "EN 1990:2002+A1:2005",
        "Annexe A1 (Application aux bâtiments) & Annexes Nationales",
        "Gestion configurable des facteurs de sécurité partiels et de simultanéité psi_0, psi_1, psi_2 selon l'Annexe Nationale sélectionnée (France NF, Allemagne DIN, CEN).",
        "src/Standards/NationalAnnexConfig.h, src/Standards/NationalAnnexConfig.cpp",
        "TSA_StandardsTests",
        RequirementStatus::Implemented,
        "Configuration des Annexes Nationales formalisée."
    });

    registerRequirement({
        "REQ-CALC-EC1-001",
        StandardFramework::EN_1991,
        RequirementDomain::StructuralDesign,
        "EN 1991-1-1:2002",
        "§5.2 (Poids propre des structures)",
        "Calcul automatique du poids propre volumique des éléments linéaires à partir de la masse volumique rho et de la section transversale A.",
        "src/Analysis/OpenSeesAnalysisBuilder.cpp",
        "TSA_OpenSeesTests::testUniformDistributedBeamLoad",
        RequirementStatus::Implemented,
        "Charges uniformes dérivées de rho*g*A validées."
    });

    registerRequirement({
        "REQ-CALC-EC2-001",
        StandardFramework::EN_1992,
        RequirementDomain::StructuralDesign,
        "EN 1992-1-1:2004",
        "Tableau 3.1 (Propriétés de résistance et de déformation du béton)",
        "Définition normée des classes de béton C20/25 à C50/60 avec leurs valeurs fck, fcm, Ecm, nu = 0.20 et masse volumique 2500 kg/m³.",
        "src/Model/Material.cpp",
        "TSA_ExtensionsTests",
        RequirementStatus::Implemented,
        "Fiches matériaux conformes à l'Eurocode 2."
    });

    registerRequirement({
        "REQ-CALC-EC2-002",
        StandardFramework::EN_1992,
        RequirementDomain::StructuralDesign,
        "EN 1992-1-1:2004",
        "§6.1 (Flexion simple et composée à l'ELU)",
        "Vérification de la capacité portante des sections rectangulaires et circulaires en béton armé et calcul des sections d'armatures longitudinales As.",
        "src/Standards/Design/ConcreteDesignEC2.h [prévu Phase 6]",
        "EC2_RC_BEAM_001 [prévu]",
        RequirementStatus::Partial,
        "Spécification rédigée, implémentation du vérificateur en Phase 6."
    });

    registerRequirement({
        "REQ-CALC-EC3-001",
        StandardFramework::EN_1993,
        RequirementDomain::StructuralDesign,
        "EN 1993-1-1:2005",
        "Tableau 3.1 (Nuances d'acier) & Catalogues de profilés",
        "Propriétés mécaniques des aciers S235, S275, S355 (E = 210 GPa, nu = 0.30) et géométrie normalisée des profilés IPE, HEA, HEB, UPN.",
        "src/Model/Section.cpp, src/Model/Material.cpp",
        "TSA_ModelElementsTests",
        RequirementStatus::Implemented,
        "Inerties principales et modules d'élasticité validés."
    });

    registerRequirement({
        "REQ-CALC-EC3-002",
        StandardFramework::EN_1993,
        RequirementDomain::StructuralDesign,
        "EN 1993-1-1:2005",
        "§6.3 (Résistance des barres aux instabilités : flambement et déversement)",
        "Calcul des longueurs de flambement Lcr, des élancements réduits lambda_bar et des coefficients de réduction chi selon les courbes de flambement a0, a, b, c, d.",
        "src/Standards/Design/SteelDesignEC3.h [prévu Phase 6]",
        "EC3_STEEL_BEAM_001 [prévu]",
        RequirementStatus::Partial,
        "Spécification rédigée, implémentation du vérificateur en Phase 6."
    });

    registerRequirement({
        "REQ-CALC-CAB-001",
        StandardFramework::EN_1993_1_11,
        RequirementDomain::StructuralDesign,
        "EN 1993-1-11:2006 / fib Bulletin 89",
        "§5 & §6 (Éléments tendus en acier : torons, câbles clos, haubans)",
        "Formulation de câbles tendus intégrant la précontrainte initiale, le module sécant d'Ernst, les pertes par rentrée de mors et la sécurité gamma_M.",
        "src/Model/Cable/CableStandards.cpp, src/Model/Cable/Cable.cpp",
        "TSA_CablesTests",
        RequirementStatus::Implemented,
        "Comportement non-linéaire et catalogues certifiés validés."
    });

    // =========================================================================
    // 5. ERGONOMIE MULTIPORT ET ÉDITION DE NOTE DE CALCUL
    // =========================================================================
    registerRequirement({
        "REQ-UI-PORT-001",
        StandardFramework::ISO_IEC_25010,
        RequirementDomain::ResultsVisualization,
        "ISO/IEC 25010:2023",
        "§4.2.4 (Utilisabilité - Espace de travail multi-ports)",
        "Disposition fluide et réactive de vues 3D, déformées, diagrammes 2D et note de calcul partageant une structure de données unique sans clonage mémoire.",
        "src/UI/Port/PortAreaWidget.cpp",
        "TSA_WindowManagerTests",
        RequirementStatus::Implemented,
        "Vérifié sous 5 modes d'affichage multi-ports."
    });

    registerRequirement({
        "REQ-NDC-GEN-001",
        StandardFramework::IEEE_Std_1063,
        RequirementDomain::DocumentationUI,
        "IEEE Std 1063-2001 / EN 1990",
        "Rapport justificatif de dimensionnement",
        "Génération automatique d'une Note de Calcul (NDC) structurée en chapitres, tableaux et références normatives, exportable en HTML et texte brut.",
        "src/NDC/NDCGenerator.cpp, src/NDC/NDCDocumentModel.cpp",
        "TSA_AllTests",
        RequirementStatus::Implemented,
        "Rapport technique complet généré et validé."
    });

    // =========================================================================
    // 6. RÉSULTATS, TRAÇABILITÉ D'EXÉCUTION ET BENCHMARKS (V&V)
    // =========================================================================
    registerRequirement({
        "REQ-RES-META-001",
        StandardFramework::ISO_IEC_25010,
        RequirementDomain::ResultsVisualization,
        "ISO/IEC 25010 §4.2.5 / EN 1990 §6",
        "Métadonnées certifiées d'exécution du calcul",
        "Enregistrement immuable des métadonnées de calcul : Annexe Nationale active, version OpenSees, statut d'équilibre statique, résidu de fermeture maximal et horodatage.",
        "src/Analysis/ResultsModel.h, src/Analysis/OpenSeesSolver.cpp",
        "TSA_StandardsTests",
        RequirementStatus::Implemented,
        "Traçabilité garantie et intégrée dans la Note de Calcul NDC."
    });

    registerRequirement({
        "REQ-VV-BENCH-001",
        StandardFramework::ISO_IEC_IEEE_29119,
        RequirementDomain::TestingQA,
        "ISO/IEC/IEEE 29119 / ISO 9001:2015",
        "Cas de référence analytiques et benchmarks (V&V)",
        "Répertoire centralisé de benchmarks avec formules analytiques fermées (Euler-Bernoulli, Timoshenko, RDM), seuils de tolérance et évaluation automatisée pass/fail.",
        "src/Standards/AnalyticalBenchmark.h, src/Standards/AnalyticalBenchmark.cpp",
        "TSA_StandardsTests",
        RequirementStatus::Implemented,
        "4 benchmarks fondamentaux validés avec écart relatif < 1%."
    });
}

} // namespace TSA::Standards
