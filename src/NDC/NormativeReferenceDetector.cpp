#include "NormativeReferenceDetector.h"
#include "../Model/Model.h"
#include "../Model/Material.h"
#include "../Model/Section.h"
#include "../Model/Load/LoadManager.h"
#include "../Model/Load/LoadCase.h"
#include "../Analysis/ResultsModel.h"

#include <QDate>
#include <algorithm>

namespace TSA::NDC
{

std::vector<StandardReference> NormativeReferenceDetector::detectApplicableStandards(const TSA::Model::Model& model)
{
    std::vector<StandardReference> standards;

    // 1. EN 1990 (Eurocode 0) : Toujours applicable pour la formulation des états limites et des combinaisons
    standards.push_back({
        QStringLiteral("EN 1990"),
        QStringLiteral("Eurocode 0 — Bases de calcul des structures (EN 1990:2002 + A1:2005)"),
        QStringLiteral("Bases de calcul"),
        QStringLiteral("Principes généraux de sécurité structurale, exigences d'aptitude au service et combinaisons d'actions (ELU / ELS)."),
        QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-0")
    });

    // 2. EN 1991 (Eurocode 1) : Actions sur les structures si des charges ou le poids propre existent
    bool hasLoads = false;
    const auto& lm = model.loadManager();
    if (!lm.loadCases().empty() || !lm.nodalLoads().empty() || !lm.memberLoads().empty())
    {
        hasLoads = true;
    }
    for (const auto& [id, lc] : lm.loadCases())
    {
        if (lc.isSelfWeightIncluded()) hasLoads = true;
    }

    if (hasLoads)
    {
        standards.push_back({
            QStringLiteral("EN 1991-1-1"),
            QStringLiteral("Eurocode 1 — Actions sur les structures — Partie 1-1 : Actions générales — Poids volumiques, poids propres, charges d'exploitation pour les bâtiments"),
            QStringLiteral("Actions"),
            QStringLiteral("Modélisation des charges gravitaires permanentes (G) et variables d'exploitation (Q)."),
            QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-1")
        });
    }

    // 3. Détection des matériaux et sections
    bool hasConcrete = false;
    bool hasSteel = false;
    bool hasTimber = false;

    std::map<std::string, TSA::Model::Material> uniqueMaterials;
    for (const auto& [id, b] : model.beams()) uniqueMaterials[b.material().name] = b.material();
    for (const auto& [id, c] : model.columns()) uniqueMaterials[c.material().name] = c.material();
    for (const auto& [id, tr] : model.trussMembers()) uniqueMaterials[tr.material().name] = tr.material();
    for (const auto& [id, cb] : model.cables()) uniqueMaterials[cb.material().name] = cb.material();
    for (const auto& [id, sl] : model.slabs()) uniqueMaterials[sl.material().name] = sl.material();
    for (const auto& [id, w] : model.walls()) uniqueMaterials[w.material().name] = w.material();

    for (const auto& [name, mat] : uniqueMaterials)
    {
        if (mat.type == TSA::Model::MaterialType::Concrete ||
            mat.type == TSA::Model::MaterialType::ReinforcedConcrete)
        {
            hasConcrete = true;
        }
        else if (mat.type == TSA::Model::MaterialType::Steel ||
                 mat.type == TSA::Model::MaterialType::RebarSteel ||
                 mat.type == TSA::Model::MaterialType::GalvanizedSteel)
        {
            hasSteel = true;
        }
        else if (mat.type == TSA::Model::MaterialType::Timber)
        {
            hasTimber = true;
        }
        
        QString matName = QString::fromStdString(mat.name).toLower();
        if (matName.contains("béton") || matName.contains("concrete") || matName.contains("c25") || matName.contains("c30"))
        {
            hasConcrete = true;
        }
        if (matName.contains("acier") || matName.contains("steel") || matName.contains("s235") || matName.contains("s275") || matName.contains("s355"))
        {
            hasSteel = true;
        }
    }

    // Si les sections barres suggèrent de l'acier (profilés I, UPN, tubes)
    for (const auto& [id, beam] : model.beams())
    {
        auto shape = beam.section().shape;
        if (shape == TSA::Model::SectionShape::IShape ||
            shape == TSA::Model::SectionShape::UPN ||
            shape == TSA::Model::SectionShape::Angle ||
            shape == TSA::Model::SectionShape::Pipe ||
            shape == TSA::Model::SectionShape::BoxHollow)
        {
            hasSteel = true;
        }
    }

    // EN 1992-1-1 (Eurocode 2 - Béton)
    if (hasConcrete || !model.slabs().empty() || !model.walls().empty())
    {
        standards.push_back({
            QStringLiteral("EN 1992-1-1"),
            QStringLiteral("Eurocode 2 — Calcul des structures en béton — Partie 1-1 : Règles générales et règles pour les bâtiments (EN 1992-1-1:2004)"),
            QStringLiteral("Béton armé"),
            QStringLiteral("Dimensionnement et vérification des armatures en flexion et effort tranchant pour éléments en béton."),
            QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-2")
        });
    }

    // EN 1993-1-1 (Eurocode 3 - Acier)
    if (hasSteel)
    {
        standards.push_back({
            QStringLiteral("EN 1993-1-1"),
            QStringLiteral("Eurocode 3 — Calcul des structures en acier — Partie 1-1 : Règles générales et règles pour les bâtiments (EN 1993-1-1:2005)"),
            QStringLiteral("Acier"),
            QStringLiteral("Vérification des sections et de l'instabilité par flambement / déversement des éléments métalliques."),
            QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-3")
        });
    }

    // EN 1993-1-11 (Câbles)
    if (!model.cables().empty())
    {
        standards.push_back({
            QStringLiteral("EN 1993-1-11"),
            QStringLiteral("Eurocode 3 — Calcul des structures en acier — Partie 1-11 : Calcul des structures à câbles ou éléments tendus (EN 1993-1-11:2006)"),
            QStringLiteral("Câbles et éléments tendus"),
            QStringLiteral("Vérification des tirants, haubans et éléments câbles soumis à précontrainte et traction pure."),
            QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-3")
        });
    }

    // EN 1997-1 (Eurocode 7 - Géotechnique / Fondations)
    if (!model.foundations().empty())
    {
        standards.push_back({
            QStringLiteral("EN 1997-1"),
            QStringLiteral("Eurocode 7 — Calcul géotechnique — Partie 1 : Règles générales (EN 1997-1:2004)"),
            QStringLiteral("Géotechnique"),
            QStringLiteral("Justification de la capacité portante des semelles de fondation et des appuis au sol."),
            QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-7")
        });
    }

    // EN 1998-1 (Eurocode 8 - Séisme)
    bool hasSeismic = false;
    for (const auto& [id, lc] : lm.loadCases())
    {
        if (lc.category() == TSA::Model::LoadCaseCategory::Seismic)
        {
            hasSeismic = true;
            break;
        }
    }

    if (hasSeismic)
    {
        standards.push_back({
            QStringLiteral("EN 1998-1"),
            QStringLiteral("Eurocode 8 — Calcul des structures pour leur résistance aux séismes — Partie 1 : Règles générales, actions sismiques et règles pour les bâtiments (EN 1998-1:2004)"),
            QStringLiteral("Séisme"),
            QStringLiteral("Actions sismiques appliquées comme cas de charge statiques (forces latérales équivalentes) et combinaison sismique."),
            QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-8")
        });
    }

    return standards;
}

std::vector<BibliographicReference> NormativeReferenceDetector::generateBibliography(const TSA::Model::Model& model)
{
    std::vector<BibliographicReference> refs;
    QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");

    // Eurocodes JRC
    refs.push_back({
        QStringLiteral("[CEN-EN1990]"),
        QStringLiteral("CEN (Comité Européen de Normalisation)"),
        QStringLiteral("NF EN 1990:2002/A1:2005 — Eurocode 0 : Bases de calcul des structures"),
        QStringLiteral("Bruxelles : CEN / Commission Européenne JRC"),
        2005,
        QStringLiteral("Norme européenne homologuée"),
        QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-0"),
        currentDate,
        QStringLiteral("Principes fondamentaux de sécurité, états limites et combinaisons d'actions.")
    });

    auto applicableStds = detectApplicableStandards(model);
    for (const auto& stdItem : applicableStds)
    {
        if (stdItem.code == "EN 1992-1-1")
        {
            refs.push_back({
                QStringLiteral("[CEN-EN1992]"),
                QStringLiteral("CEN"),
                QStringLiteral("NF EN 1992-1-1:2004 — Eurocode 2 : Calcul des structures en béton — Partie 1-1"),
                QStringLiteral("Bruxelles : CEN"),
                2004,
                QStringLiteral("Eurocode 2"),
                QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-2"),
                currentDate,
                QStringLiteral("Dimensionnement des armatures longitudinales et transversales.")
            });
        }
        else if (stdItem.code == "EN 1993-1-1")
        {
            refs.push_back({
                QStringLiteral("[CEN-EN1993]"),
                QStringLiteral("CEN"),
                QStringLiteral("NF EN 1993-1-1:2005 — Eurocode 3 : Calcul des structures en acier — Partie 1-1"),
                QStringLiteral("Bruxelles : CEN"),
                2005,
                QStringLiteral("Eurocode 3"),
                QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-3"),
                currentDate,
                QStringLiteral("Vérification des sections de classe 1 à 4 et instabilités globales.")
            });
        }
        else if (stdItem.code == "EN 1998-1")
        {
            refs.push_back({
                QStringLiteral("[CEN-EN1998]"),
                QStringLiteral("CEN"),
                QStringLiteral("NF EN 1998-1:2004 — Eurocode 8 : Calcul des structures pour leur résistance aux séismes"),
                QStringLiteral("Bruxelles : CEN"),
                2004,
                QStringLiteral("Eurocode 8"),
                QStringLiteral("https://eurocodes.jrc.ec.europa.eu/EN-Eurocodes/eurocode-8"),
                currentDate,
                QStringLiteral("Spectres de réponse élastique et comportement parasismique.")
            });
        }
    }

    // OpenSees Framework officiel
    refs.push_back({
        QStringLiteral("[OpenSees-Recorders]"),
        QStringLiteral("Mazzoni, S., McKenna, F., Scott, M. H., & Fenves, G. L."),
        QStringLiteral("OpenSees Command Language Manual — Element & Envelope Element Recorders"),
        QStringLiteral("Pacific Earthquake Engineering Research Center (PEER), University of California, Berkeley"),
        2006,
        QStringLiteral("OpenSees Release Documentation"),
        QStringLiteral("https://opensees.github.io/OpenSeesDocumentation/user/manual/output/recorder.html"),
        currentDate,
        QStringLiteral("Enregistrement précis des forces (force, localForce, globalForce), déformations et extrema (envelopeElement).")
    });

    refs.push_back({
        QStringLiteral("[OpenSeesPy-Framework]"),
        QStringLiteral("Zhu, M., McKenna, F., & Scott, M. H."),
        QStringLiteral("OpenSeesPy: Python library for the OpenSees framework"),
        QStringLiteral("SoftwareX, Elsevier, Vol. 7, pp. 6-11"),
        2018,
        QStringLiteral("Peer-reviewed article"),
        QStringLiteral("https://openseespydoc.readthedocs.io/"),
        currentDate,
        QStringLiteral("Architecture d'interface de calcul et extraction numérique haute performance.")
    });

    // Ouvrages de référence
    refs.push_back({
        QStringLiteral("[Bathe-1996]"),
        QStringLiteral("Bathe, Klaus-Jürgen"),
        QStringLiteral("Finite Element Procedures"),
        QStringLiteral("Prentice Hall, Upper Saddle River, NJ"),
        1996,
        QStringLiteral("2nd Edition"),
        QStringLiteral("http://web.mit.edu/kjb/www/Books/FEP_2nd_Edition_4th_Printing.pdf"),
        currentDate,
        QStringLiteral("Théorie des éléments finis, résolution d'équations et intégration temporelle.")
    });

    refs.push_back({
        QStringLiteral("[Calgaro-Virlogeux]"),
        QStringLiteral("Calgaro, Jean-Armand & Virlogeux, Michel"),
        QStringLiteral("Projet et calcul des ponts — Tome 1 & 2"),
        QStringLiteral("Paris : Presses de l'École Nationale des Ponts et Chaussées"),
        1988,
        QStringLiteral("Collection scientifique de l'ENPC"),
        QStringLiteral("https://catalogue.enpc.fr/"),
        currentDate,
        QStringLiteral("Conception structurale, lignes d'influence et comportement des ossatures.")
    });

    return refs;
}

} // namespace TSA::NDC
