// Suite « appearance » : apparence des éléments par matériau (tests 220-221).
// Texture propre à chaque matériau, partagée entre objets ; couleur choisie par l'utilisateur
// teintant le grain du matériau ; panneau Propriétés n'imposant plus de couleur par défaut.

#include "test_common.h"
#include <QMessageBox>

#include "App/ProductInfo.h"
#include "UI/Properties/BeamPropertiesView.h"
#include "UI/Properties/ColumnPropertiesView.h"
#include "UI/Properties/SlabPropertiesView.h"
#include "UI/Properties/TrussMemberPropertiesView.h"
#include "UI/Properties/WallPropertiesView.h"
#include "Viewer/MaterialVisual.h"
#include "Viewer/TextureManager.h"

#include "UI/Theme/ThemeManager.h"
#include "UI/Dialogs/DialogStandardHelper.h"
#include "UI/Widgets/StandardDialogHeader.h"
#include "UI/Dialogs/NewNodeDialog.h"
#include "UI/Dialogs/SectionCustomizationDialog.h"
#include "UI/Dialogs/LoadCaseDialog.h"
#include "UI/Dialogs/LevelDialog.h"
#include "UI/Properties/PropertyPanel.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QInputDialog>
#include <QTableWidget>
#include <QTimer>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
occ::handle<Graphic3d_TextureMap> textureOf(const Handle(AIS_Shape)& shape)
{
    if (!shape->Attributes()->HasOwnShadingAspect()) return {};
    const auto& fill = shape->Attributes()->ShadingAspect()->Aspect();
    return fill->ToMapTexture() ? fill->TextureMap() : occ::handle<Graphic3d_TextureMap>();
}
} // namespace

bool runSuite_Appearance(int& passed)
{
    using TSA::Viewer::MaterialVisual;
    using TSA::Viewer::RenderDisplayMode;

    // -------------------------------------------------------------------------
    // TEST 220 : texture propre à chaque matériau, partagée ; couleur utilisateur ; modes sans texture
    // -------------------------------------------------------------------------
    {
        TSA::Viewer::TextureManager::instance().addSearchPath(TSA::Product::sourceDirectory() + "/Extensions/TSALib/Textures");
        auto& mv = MaterialVisual::instance();
        mv.clearCache();
        const Material concrete = Material::concreteC25_30();
        const Material steel = Material::steelS235();
        TEST_CHECK(mv.hasTexture(concrete) && mv.hasTexture(steel), "Test 220: textures béton et acier trouvées");

        const TopoDS_Shape box = BRepPrimAPI_MakeBox(0.3, 0.3, 6.0).Shape();
        Handle(AIS_Shape) a = new AIS_Shape(box), b = new AIS_Shape(box), c = new AIS_Shape(box), d = new AIS_Shape(box);
        mv.applyToShape(a, concrete);
        mv.applyToShape(b, concrete);
        mv.applyToShape(c, steel);
        const auto ta = textureOf(a), tb = textureOf(b), tc = textureOf(c);
        TEST_CHECK(!ta.IsNull() && !tc.IsNull(), "Test 220: texture appliquée par défaut (mode Matériaux)");
        TEST_CHECK(ta->GetId() == tb->GetId(), "Test 220: une seule ressource graphique par texture (béton partagé)");
        TEST_CHECK(ta->GetId() != tc->GetId(), "Test 220: béton et acier ont des textures différentes");
        TEST_CHECK(a->TextureRepeatUV().Y() >= 5.9, "Test 220: motif répété le long de la barre (≈ 1 par mètre)");

        // Acier : couleur de base bleue. Sans couleur propre, l'objet reste neutre (gris clair) pour que
        // la texture garde ses propres couleurs au lieu d'être assombrie par un second bleu.
        Quantity_Color colorC;
        c->Color(colorC);
        TEST_CHECK(std::abs(colorC.Red() - colorC.Blue()) < 1e-6 && std::abs(colorC.Green() - colorC.Blue()) < 1e-6 && colorC.Red() > 0.7,
                   "Test 220: sans couleur propre, la texture garde ses couleurs (" << colorC.Red() << ", " << colorC.Green() << ", " << colorC.Blue() << ")");

        mv.applyToShape(d, concrete, "#FF0000");
        const auto td = textureOf(d);
        Quantity_Color colorD;
        d->Color(colorD);
        TEST_CHECK(!td.IsNull() && td->GetId() != ta->GetId(), "Test 220: couleur utilisateur → grain neutre du même matériau");
        TEST_CHECK(colorD.Red() > 0.5 && colorD.Green() < 0.05 && colorD.Blue() < 0.05, "Test 220: la couleur choisie teinte l'élément");

        mv.applyToShape(b, concrete, "", RenderDisplayMode::Structure);
        TEST_CHECK(textureOf(b).IsNull(), "Test 220: pas de texture hors du mode Matériaux");
        std::cout << "[PASS] Test 220: Texture par matériau, partagée, teintée par la couleur utilisateur" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 221 : panneau Propriétés — aucune couleur imposée, bouton « Matériau »
    // -------------------------------------------------------------------------
    {
        Model m;
        const int n1 = m.addNode(0, 0, 0);
        const int n2 = m.addNode(0, 0, 3);
        const int col = m.addColumn(n1, n2, Section::circular(0.3), Material::concreteC25_30());
        TEST_CHECK(m.getColumn(col) && m.getColumn(col)->color().empty(), "Test 221: poteau créé sans couleur propre");

        TSA::UI::ColumnPropertiesView view(&m);
        view.setElementId(col);
        QPushButton* colorButton = nullptr;
        for (auto* btn : view.findChildren<QPushButton*>())
            if (btn->property("shownColor").isValid()) colorButton = btn;
        TEST_CHECK(colorButton != nullptr, "Test 221: bouton couleur trouvé");
        const QString concreteHex = QColor(QString::fromStdString(Material::concreteC25_30().visual.baseColor)).name();
        TEST_CHECK(colorButton->property("shownColor").toString() == concreteHex, "Test 221: le bouton montre la couleur du matériau");

        view.applyChanges();
        TEST_CHECK(m.getColumn(col)->color().empty(), "Test 221: valider le panneau n'impose plus de couleur (orange auparavant)");

        m.getColumn(col)->setColor("#ff0000");
        view.setElementId(col);
        TEST_CHECK(colorButton->property("shownColor").toString() == QStringLiteral("#ff0000"), "Test 221: couleur propre affichée");
        auto* useMaterial = view.findChild<QToolButton*>(QStringLiteral("useMaterialColorButton"));
        TEST_CHECK(useMaterial != nullptr, "Test 221: bouton « Matériau » présent");
        useMaterial->click();
        TEST_CHECK(m.getColumn(col)->color().empty(), "Test 221: « Matériau » rend l'apparence du matériau");
        std::cout << "[PASS] Test 221: Panneau Propriétés : couleur du matériau par défaut, « Matériau »" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 493 : panneau Propriétés — modifier un champ ne réécrit pas la section, le matériau, ni les valeurs que
    // les champs ne savent pas représenter. Auparavant, chaque validation (nom, couleur…) reconstruisait la section
    // depuis les champs : IPE 300 → « Profilé I » sans congés, UPN / HEA / cornière → rectangle ou rond, matériau
    // hors liste → matériau précédent de la liste, rotation et hauteur arrondies aux décimales affichées.
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 4, 0), d = m.addNode(0, 4, 0);
        const int e = m.addNode(0, 0, 3), f = m.addNode(6, 0, 3);
        auto sameSection = [](const Section& x, const Section& y) {
            return x.name == y.name && x.shape == y.shape && x.area() == y.area() && x.iy() == y.iy() && x.iz() == y.iz() && x.it() == y.it();
        };

        const int ipe = m.addBar(a, b, Section::ipe(300), Material::steelS355(), BarRole::Beam, 12.345);
        const int upn = m.addBar(b, c, Section::upn(200), Material::steelS235());
        Material special = Material::steelS355();
        special.id = 987654;
        special.name = "S355 spécial (test 493)";
        special.fk = 4.12e8;
        const int custom = m.addBar(c, d, Section::rectangular(0.20, 0.40), special);
        const Section ipeSec = m.getBeam(ipe)->section(), upnSec = m.getBeam(upn)->section();
        for (const int id : { ipe, upn, custom })
        {
            TSA::UI::BeamPropertiesView view(&m);
            view.setElementId(id);
            view.applyChanges();   // comme après une modification du nom ou de la couleur
        }
        TEST_CHECK(sameSection(m.getBeam(ipe)->section(), ipeSec), "Test 493: barre IPE 300 conservée (nom, A, Iy, Iz, It)");
        TEST_CHECK(m.getBeam(ipe)->rotation() == 12.345, "Test 493: rotation non arrondie aux décimales affichées");
        TEST_CHECK(sameSection(m.getBeam(upn)->section(), upnSec), "Test 493: barre UPN 200 conservée");
        TEST_CHECK(m.getBeam(custom)->material().name == special.name && m.getBeam(custom)->material().fk == special.fk,
                   "Test 493: matériau absent de la liste conservé");

        const int col = m.addColumn(a, e);
        m.getColumn(col)->setSection(Section::hea(200));
        m.getColumn(col)->setMaterial(Material::steelS355());
        const Section heaSec = m.getColumn(col)->section();
        {
            TSA::UI::ColumnPropertiesView view(&m);
            view.setElementId(col);
            view.applyChanges();
        }
        TEST_CHECK(sameSection(m.getColumn(col)->section(), heaSec), "Test 493: poteau HEA 200 conservé (non remplacé par un rectangle)");
        TEST_CHECK(m.getColumn(col)->material().name == Material::steelS355().name, "Test 493: matériau du poteau conservé");

        const int truss = m.addTrussMember(a, f);
        m.getTrussMember(truss)->setSection(Section::angle(0.06, 0.06, 0.006, "L 60x60x6"));
        const Section angleSec = m.getTrussMember(truss)->section();
        {
            TSA::UI::TrussMemberPropertiesView view(&m);
            view.setElementId(truss);
            view.applyChanges();
        }
        TEST_CHECK(sameSection(m.getTrussMember(truss)->section(), angleSec), "Test 493: treillis en cornière conservé (non remplacé par un rond)");

        const int wall = m.addWall(e, f, 3.125, 0.175);
        {
            TSA::UI::WallPropertiesView view(&m);
            view.setElementId(wall);
            view.applyChanges();
        }
        TEST_CHECK(m.getWall(wall) && m.getWall(wall)->height() == 3.125 && m.getWall(wall)->thickness() == 0.175,
                   "Test 493: hauteur du voile non arrondie (3,125 m)");

        // Contrôle positif : une modification réelle d'un champ reste appliquée.
        const int slab = m.addSlab({ a, b, c, d }, 0.175);
        {
            TSA::UI::SlabPropertiesView view(&m);
            view.setElementId(slab);
            view.applyChanges();
            TEST_CHECK(m.getSlab(slab) && m.getSlab(slab)->thickness() == 0.175, "Test 493: épaisseur de dalle conservée");
            QDoubleSpinBox* thickness = nullptr;
            for (auto* spin : view.findChildren<QDoubleSpinBox*>())
                if (spin->minimum() == 0.01 && spin->maximum() == 10.0) thickness = spin;
            TEST_CHECK(thickness != nullptr, "Test 493: champ épaisseur trouvé");
            thickness->setValue(0.25);
            TEST_CHECK(m.getSlab(slab)->thickness() == 0.25, "Test 493: une épaisseur modifiée par l'utilisateur est appliquée");
        }
        std::cout << "[PASS] Test 493: Panneau Propriétés : seuls les champs modifiés sont réécrits (sections, matériaux, valeurs)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 508 : panneau Propriétés d'une barre en I (BUG-110, décision D1) — modifier une dimension conserve le rayon de
    // congé ; un profil normalisé qui ne correspond plus à la norme est signalé et sa conversion en section personnalisée
    // proposée (une seule fois si elle est refusée) ; le rayon se modifie explicitement.
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(12, 0, 0);
        const int bar1 = m.addBar(a, b, Section::ipe(300), Material::steelS355(), BarRole::Beam);
        const int bar2 = m.addBar(b, c, Section::ipe(300), Material::steelS355(), BarRole::Beam);
        QMessageBox::StandardButton reply = QMessageBox::Yes;
        int prompts = 0;
        QTimer answer;
        QObject::connect(&answer, &QTimer::timeout, [&] {
            for (auto* w : QApplication::topLevelWidgets())
                if (auto* box = qobject_cast<QMessageBox*>(w); box && box->isVisible() && box->button(reply))
                {
                    ++prompts;
                    box->button(reply)->click();
                }
        });
        answer.start(20);
        auto spin = [](QWidget& v, double lo, double hi) -> QDoubleSpinBox* {
            for (auto* s : v.findChildren<QDoubleSpinBox*>())
                if (s->minimum() == lo && s->maximum() == hi && !s->isHidden()) return s;
            return nullptr;
        };
        {
            TSA::UI::BeamPropertiesView view(&m);
            view.setElementId(bar1);
            QDoubleSpinBox* height = spin(view, 0.01, 10.0);
            QDoubleSpinBox* fillet = spin(view, 0.0, 0.1);
            TEST_CHECK(height && fillet && fillet->value() == 0.015, "Test 508: champ « rayon de congé » affiché (15 mm)");
            height->setValue(0.31);
            const auto& s = m.getBeam(bar1)->section();
            TEST_CHECK(prompts == 1 && s.rootRadius() == 0.015 && s.height == 0.31 && s.name.rfind("Profilé I 310", 0) == 0,
                       "Test 508: hauteur modifiée : rayon conservé, conversion proposée et acceptée (« " << s.name << " »)");
        }
        reply = QMessageBox::No;
        {
            TSA::UI::BeamPropertiesView view(&m);
            view.setElementId(bar2);
            QDoubleSpinBox* height = spin(view, 0.01, 10.0);
            height->setValue(0.31);
            TEST_CHECK(prompts == 2 && m.getBeam(bar2)->section().name == "IPE 300" && m.getBeam(bar2)->section().rootRadius() == 0.015,
                       "Test 508: conversion refusée : désignation et rayon conservés");
            height->setValue(0.32);
            TEST_CHECK(prompts == 2 && m.getBeam(bar2)->section().height == 0.32, "Test 508: refus mémorisé : la question n'est pas reposée");
            spin(view, 0.0, 0.1)->setValue(0.02);
            TEST_CHECK(m.getBeam(bar2)->section().rootRadius() == 0.02 && prompts == 2, "Test 508: rayon de congé modifié explicitement");
        }
        answer.stop();
        std::cout << "[PASS] Test 508: Barre en I : rayon conservé, conversion proposée, rayon modifiable" << std::endl;
        ++passed;
    }

    // TEST 498 : dialogue des cas de charge — ajouter un cas, ajouter / supprimer une combinaison prévient les vues
    // (arbre du modèle, vue 3D) une seule fois. Auparavant : entrée Annuler créée mais aucune notification.
    // -------------------------------------------------------------------------
    {
        Model m;
        struct Observer : TSA::Model::IModelObserver
        {
            int loadCaseChanges = 0;
            void onLoadCaseChanged(int) override { ++loadCaseChanges; }
        } obs;
        m.addObserver(&obs);
        TSA::UI::LoadCaseDialog dlg(&m);
        // Valide automatiquement les boîtes de saisie modales (nom, catégorie) ouvertes par le dialogue.
        QTimer autoAccept;
        QObject::connect(&autoAccept, &QTimer::timeout, [] {
            for (auto* w : QApplication::topLevelWidgets())
                if (auto* input = qobject_cast<QInputDialog*>(w); input && input->isVisible()) input->accept();
        });
        autoAccept.start(20);
        const auto casesBefore = m.loadManager().loadCases().size();
        QMetaObject::invokeMethod(&dlg, "onAddLoadCase", Qt::DirectConnection);
        TEST_CHECK(m.loadManager().loadCases().size() == casesBefore + 1 && obs.loadCaseChanges == 1, "Test 498: cas ajouté, une notification");
        const auto combosBefore = m.loadManager().combinations().size();
        QMetaObject::invokeMethod(&dlg, "onAddCombination", Qt::DirectConnection);
        TEST_CHECK(m.loadManager().combinations().size() == combosBefore + 1 && obs.loadCaseChanges == 2, "Test 498: combinaison ajoutée, une notification");
        autoAccept.stop();
        // Suppression : sélectionner la ligne de la combinaison ajoutée dans le tableau des combinaisons
        QTableWidget* combos = nullptr;
        for (auto* t : dlg.findChildren<QTableWidget*>())
            for (int r = 0; r < t->rowCount(); ++r)
                for (int c = 0; c < t->columnCount(); ++c)
                    if (t->item(r, c) && t->item(r, c)->text() == QStringLiteral("1.35G + 1.5Q")) combos = t, t->setCurrentCell(r, 0);
        TEST_CHECK(combos != nullptr, "Test 498: combinaison affichée dans le tableau");
        QMetaObject::invokeMethod(&dlg, "onRemoveCombination", Qt::DirectConnection);
        TEST_CHECK(m.loadManager().combinations().size() == combosBefore && obs.loadCaseChanges == 3, "Test 498: combinaison supprimée, une notification");
        m.removeObserver(&obs);
        std::cout << "[PASS] Test 498: Cas de charge et combinaisons : vues prévenues une fois par modification" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 222 : Conformité au Standard UI/UX TSA (TSA_UI_DESIGN_STANDARD)
    // -------------------------------------------------------------------------
    {
        // 1. Couleurs sémantiques ThemeManager
        auto& tm = TSA::UI::ThemeManager::instance();
        TEST_CHECK(tm.primaryColor().isValid() && tm.primaryColor().name().startsWith('#'), "Test 222: primaryColor valide");
        TEST_CHECK(tm.windowColor().isValid() && tm.windowColor().name().startsWith('#'), "Test 222: windowColor valide");
        TEST_CHECK(tm.surfaceColor().isValid() && tm.surfaceColor().name().startsWith('#'), "Test 222: surfaceColor valide");
        TEST_CHECK(tm.borderColor().isValid() && tm.borderColor().name().startsWith('#'), "Test 222: borderColor valide");
        TEST_CHECK(tm.textColor().isValid() && tm.textColor().name().startsWith('#'), "Test 222: textColor valide");
        TEST_CHECK(tm.textMutedColor().isValid() && tm.textMutedColor().name().startsWith('#'), "Test 222: textMutedColor valide");
        TEST_CHECK(tm.successColor().isValid() && tm.successColor().name().startsWith('#'), "Test 222: successColor valide");
        TEST_CHECK(tm.warningColor().isValid() && tm.warningColor().name().startsWith('#'), "Test 222: warningColor valide");
        TEST_CHECK(tm.destructiveColor().isValid() && tm.destructiveColor().name().startsWith('#'), "Test 222: destructiveColor valide");
        TEST_CHECK(tm.infoColor().isValid() && tm.infoColor().name().startsWith('#'), "Test 222: infoColor valide");

        // 2. Catégories de tailles standardisées (DialogStandardHelper)
        using namespace TSA::UI;
        QDialog testDlg;
        applyStandardDialogSizing(&testDlg, DialogSizeCategory::Compact);
        TEST_CHECK(testDlg.minimumWidth() >= 420 && testDlg.minimumHeight() >= 300, "Test 222: Compact min sizing");

        applyStandardDialogSizing(&testDlg, DialogSizeCategory::Standard);
        TEST_CHECK(testDlg.minimumWidth() >= 520 && testDlg.minimumHeight() >= 480, "Test 222: Standard min sizing");

        applyStandardDialogSizing(&testDlg, DialogSizeCategory::Large);
        TEST_CHECK(testDlg.minimumWidth() >= 740 && testDlg.minimumHeight() >= 540, "Test 222: Large min sizing");

        applyStandardDialogSizing(&testDlg, DialogSizeCategory::Studio);
        TEST_CHECK(testDlg.minimumWidth() >= 960 && testDlg.minimumHeight() >= 620, "Test 222: Studio min sizing");

        // 3. En-tête standard (StandardDialogHeader)
        auto* dummyLayout = new QVBoxLayout(&testDlg);
        auto* header = addStandardDialogHeader(dummyLayout, QStringLiteral("Titre Test"), QStringLiteral("Sous-titre explicatif"));
        TEST_CHECK(header != nullptr, "Test 222: header créé");
        TEST_CHECK(header->title() == QStringLiteral("Titre Test"), "Test 222: titre header correct");
        TEST_CHECK(header->subtitle() == QStringLiteral("Sous-titre explicatif"), "Test 222: sous-titre header correct");

        // 4. Ligne de boutons standardisée
        auto* okBtn = new QPushButton(QStringLiteral("Action"), &testDlg);
        auto* cancelBtn = new QPushButton(QStringLiteral("Annuler"), &testDlg);
        auto* btnRow = createStandardButtonRow(&testDlg, okBtn, cancelBtn, QStringLiteral("test.help"));
        TEST_CHECK(btnRow != nullptr, "Test 222: ligne de boutons créée");
        TEST_CHECK(okBtn->property("primary").toBool() == true, "Test 222: bouton principal marqué [primary=true]");

        // 5. Instanciation des dialogues réels et vérification des dimensions minimales
        Model m;
        NewNodeDialog nodeDlg(&m);
        TEST_CHECK(nodeDlg.minimumWidth() >= 420 && nodeDlg.minimumHeight() >= 300, "Test 222: NewNodeDialog conforme Compact");

        SectionCustomizationDialog secDlg;
        TEST_CHECK(secDlg.minimumWidth() >= 520 && secDlg.minimumHeight() >= 480, "Test 222: SectionCustomizationDialog conforme Standard");

        LoadCaseDialog lcDlg(&m);
        TEST_CHECK(lcDlg.minimumWidth() >= 740 && lcDlg.minimumHeight() >= 540, "Test 222: LoadCaseDialog conforme Large");

        LevelDialog lvlDlg(m.levelManager());
        TEST_CHECK(lvlDlg.minimumWidth() >= 740 && lvlDlg.minimumHeight() >= 540, "Test 222: LevelDialog conforme Large");

        PropertyPanel propPanel(&m);
        auto* headerLabel = propPanel.findChild<QLabel*>();
        TEST_CHECK(headerLabel != nullptr && headerLabel->property("panelHeader").toBool() == true, "Test 222: PropertyPanel stylise avec panelHeader");

        std::cout << "[PASS] Test 222: Conformité globale au Standard UI/UX TSA (Tailles, En-têtes, Sémantique)" << std::endl;
        ++passed;
    }
    return true;
}
