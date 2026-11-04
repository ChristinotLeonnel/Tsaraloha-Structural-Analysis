// Suite « appearance » : apparence des éléments par matériau (tests 220-221).
// Texture propre à chaque matériau, partagée entre objets ; couleur choisie par l'utilisateur
// teintant le grain du matériau ; panneau Propriétés n'imposant plus de couleur par défaut.

#include "test_common.h"
#include <QFormLayout>
#include <QComboBox>
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
#include "UI/Theme/ThemedIcon.h"
#include <ProductIdentity.h>
#include <QFile>
#include "UI/Dialogs/DialogStandardHelper.h"
#include "UI/Widgets/StandardDialogHeader.h"
#include "UI/Dialogs/NewNodeDialog.h"
#include "UI/Dialogs/SectionCustomizationDialog.h"
#include "UI/Dialogs/LoadCaseDialog.h"
#include "UI/Dialogs/LevelDialog.h"
#include "UI/Properties/PropertyPanel.h"
#include "UI/Dialogs/GridDialog.h"
#include "UI/Dialogs/HelpDialog.h"
#include "UI/Dock/LogConsoleDock.h"

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
#include <QGridLayout>
#include <QCheckBox>
#include <QDir>
#include <QTreeWidget>
#include <QRegularExpression>
#include <QTextEdit>

#include <cmath>

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
        {
            // Forme absente de la liste : aucun type affiché à tort, même après une barre en I (essai manuel 12)
            TSA::UI::BeamPropertiesView view(&m);
            view.setElementId(ipe);
            view.setElementId(upn);
            QComboBox* shape = nullptr;
            for (auto* combo : view.findChildren<QComboBox*>())
                if (combo->findData(static_cast<int>(SectionShape::IShape)) >= 0) shape = combo;
            TEST_CHECK(shape && shape->currentIndex() == -1 && shape->placeholderText() == QString::fromStdString(upnSec.name) && !shape->isEnabled(),
                       "Test 493: UPN : liste des types désactivée, aucun type affiché, désignation « " << upnSec.name << " »");
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

    // -------------------------------------------------------------------------
    // TEST 596 (BUG-115, BUG-121 (2)) : poteau — « Tube » produit un vrai tube (épaisseur de paroi), un tube existant est
    // modifiable et conservé ; les lignes masquées le sont entières (libellé compris)
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3);
        const int col = m.addColumn(a, b, Section::rectangular(0.3, 0.4), Material::steelS235());
        TSA::UI::ColumnPropertiesView view(&m);
        view.setElementId(col);
        auto* combo = view.findChildren<QComboBox*>().front();
        for (auto* c : view.findChildren<QComboBox*>())
            if (c->findData(static_cast<int>(SectionShape::BoxHollow)) >= 0) combo = c;
        combo->setCurrentIndex(combo->findData(static_cast<int>(SectionShape::BoxHollow)));
        view.applyChanges();
        const Section tube = m.getColumn(col)->section();
        TEST_CHECK(tube.shape == SectionShape::BoxHollow && tube.tw > 0.0 && tube.area() < 0.3 * 0.4 - 1e-6,
                   "Test 596: « Tube » → tube creux (épaisseur " << tube.tw << " m, A = " << tube.area() << " m² < aire pleine)");
        // Tube existant : relu, conservé à la validation (n'est plus remplacé par un rectangle plein)
        {
            TSA::UI::ColumnPropertiesView again(&m);
            again.setElementId(col);
            again.applyChanges();
        }
        const Section kept = m.getColumn(col)->section();
        TEST_CHECK(kept.shape == SectionShape::BoxHollow && std::abs(kept.tw - tube.tw) < 1e-12 && std::abs(kept.area() - tube.area()) < 1e-12,
                   "Test 596: tube existant conservé à la validation du panneau");
        // Lignes masquées : aucun libellé visible sans son champ (section rectangulaire : pas de « Rayon du cercle » ni d'épaisseur)
        combo->setCurrentIndex(combo->findData(static_cast<int>(SectionShape::Rectangular)));
        view.show();
        QApplication::processEvents();
        int orphanLabels = 0;
        for (auto* form : view.findChildren<QFormLayout*>())
            for (int r = 0; r < form->rowCount(); ++r)
            {
                QLayoutItem* label = form->itemAt(r, QFormLayout::LabelRole);
                QLayoutItem* field = form->itemAt(r, QFormLayout::FieldRole);
                if (label && label->widget() && field && field->widget() && label->widget()->isVisible() && !field->widget()->isVisible()) ++orphanLabels;
            }
        TEST_CHECK(orphanLabels == 0, "Test 596: aucun libellé affiché sans son champ (" << orphanLabels << ")");
        view.hide();
        std::cout << "[PASS] Test 596: Poteau en tube creux (épaisseur), lignes masquées entières" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 611 : système de conception (docs/UI/TSA_UI_DESIGN_STANDARD.md § 2) — jetons des deux thèmes, contrastes
    // WCAG, feuille unique sans jeton non résolu, bascule de thème sans redémarrage, surfaces migrées sans couleur locale
    // -------------------------------------------------------------------------
    {
        using TSA::UI::ThemeManager;
        using TSA::UI::ThemeTokens;
        auto& tm = ThemeManager::instance();
        const bool wasDark = tm.isDarkMode();
        const QString previousStyleSheet = qApp->styleSheet();   // restaurés à la fin : les suites suivantes en dépendent
        const QPalette previousPalette = qApp->palette();
        const auto luminance = [](const QColor& col) {
            const auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
            return 0.2126 * lin(col.redF()) + 0.7152 * lin(col.greenF()) + 0.0722 * lin(col.blueF());
        };
        const auto contrast = [&](const QColor& a, const QColor& b) {
            const double la = luminance(a), lb = luminance(b);
            return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
        };
        const QRegularExpression unresolved(QStringLiteral("@[A-Za-z]"));
        for (const bool dark : { true, false })
        {
            const ThemeTokens& t = dark ? ThemeManager::darkTokens() : ThemeManager::lightTokens();
            const char* themeName = dark ? "sombre" : "clair";
            int invalid = 0;
            for (const QString& name : ThemeManager::tokenNames())
                if (!ThemeManager::tokenColor(t, name).isValid()) ++invalid;
            TEST_CHECK(invalid == 0 && ThemeManager::tokenNames().size() >= 50, "Test 611: jetons du thème " << themeName << " tous définis (" << invalid << " invalides)");
            // Texte lisible (≥ 4,5:1) sur les trois fonds où il apparaît
            for (const auto& [fgName, fg] : { std::pair { "text", t.text }, std::pair { "textSecondary", t.textSecondary }, std::pair { "textMuted", t.textMuted },
                                               std::pair { "accentText", t.accentText }, std::pair { "successText", t.successText },
                                               std::pair { "warningText", t.warningText }, std::pair { "dangerText", t.dangerText } })
                for (const auto& [bgName, bg] : { std::pair { "panel", t.panel }, std::pair { "surface", t.surface }, std::pair { "chrome", t.chrome } })
                    TEST_CHECK(contrast(fg, bg) >= 4.5, "Test 611: thème " << themeName << " — " << fgName << " sur " << bgName << " : " << contrast(fg, bg) << ":1");
            TEST_CHECK(contrast(t.selectionText, t.selection) >= 4.5 && contrast(t.textOnAccent, t.accent) >= 4.5,
                       "Test 611: thème " << themeName << " — sélection et bouton principal lisibles");
            for (const QColor& log : { t.logTime, t.logInfo, t.logCommand, t.logSystem, t.logWarning, t.logError, t.logDebug, t.consoleText })
                TEST_CHECK(contrast(log, t.console) >= 4.5, "Test 611: thème " << themeName << " — couleur de journal " << log.name().toStdString() << " lisible sur la console");
            const QString qss = ThemeManager::resolve(QStringLiteral("a { color: @text; b: @textMuted; c: @accentSubtle; }"), t);
            TEST_CHECK(qss.contains(t.text.name()) && qss.contains(t.textMuted.name()) && qss.contains(t.accentSubtle.name()) && !qss.contains('@'),
                       "Test 611: resolve() remplace chaque jeton par sa couleur (noms longs avant courts) : " << qss.toStdString());
        }
        // Bascule : la feuille et la palette de l'application suivent le thème, sans jeton non résolu
        for (const bool dark : { false, true })
        {
            tm.setDarkMode(dark, true);
            const ThemeTokens& t = tm.tokens();
            TEST_CHECK(!qApp->styleSheet().contains(unresolved) && qApp->styleSheet().contains(t.canvas.name()) && qApp->palette().color(QPalette::Base) == t.surface,
                       "Test 611: thème " << (dark ? "sombre" : "clair") << " appliqué à l'application (feuille résolue, palette)");
        }
        // Propriété dynamique : la valeur change et le widget est repoli
        QPushButton probe;
        TSA::UI::setStyleProperty(&probe, "active", true);
        TEST_CHECK(probe.property("active").toBool(), "Test 611: setStyleProperty pose la propriété");
        // Surfaces migrées : toute couleur d'une feuille de style locale est un jeton du thème actif (ThemeManager::resolve),
        // jamais une valeur écrite en dur
        const QRegularExpression hex(QStringLiteral("#[0-9A-Fa-f]{6}\\b"));
        const auto localColors = [&](QWidget* root) {
            std::set<QString> tokenHex;
            for (const QString& name : ThemeManager::tokenNames()) tokenHex.insert(ThemeManager::tokenColor(tm.tokens(), name).name());
            int n = 0;
            auto count = [&](const QString& sheet) {
                for (auto it = hex.globalMatch(sheet); it.hasNext();)
                    if (!tokenHex.count(it.next().captured(0).toLower())) ++n;
            };
            count(root->styleSheet());
            for (auto* w : root->findChildren<QWidget*>()) count(w->styleSheet());
            return n;
        };
        TSA::UI::GridDialog grid;
        TSA::UI::LogConsoleDock console;
        TSA::UI::HelpDialog help;
        TEST_CHECK(localColors(&grid) == 0 && localColors(&console) == 0 && localColors(&help) == 0,
                   "Test 611: GridDialog, console et aide sans couleur locale (" << localColors(&grid) << ", " << localColors(&console) << ", " << localColors(&help) << ")");
        console.appendLog(QStringLiteral("essai"), QStringLiteral("ERR"), QStringLiteral("Test"));
        auto* output = console.findChild<QTextEdit*>();
        tm.setDarkMode(false, true);
        const bool lightLog = output && output->toHtml().contains(ThemeManager::lightTokens().logError.name(), Qt::CaseInsensitive);
        tm.setDarkMode(true, true);
        const bool darkLog = output && output->toHtml().contains(ThemeManager::darkTokens().logError.name(), Qt::CaseInsensitive);
        TEST_CHECK(lightLog && darkLog, "Test 611: journal de la console recoloré au changement de thème (" << lightLog << ", " << darkLog << ")");
        // Contrôle visuel : TSA_TEST_CAPTURE=<dossier> enregistre, pour chaque thème, une planche du système de conception
        // (contrôles dans leurs états) et les surfaces migrées, rendues avec la vraie feuille de style de l'application.
        if (const QString folder = qEnvironmentVariable("TSA_TEST_CAPTURE"); !folder.isEmpty())
        {
            QWidget board;
            auto* boardLayout = new QGridLayout(&board);
            boardLayout->setContentsMargins(16, 16, 16, 16);
            boardLayout->setHorizontalSpacing(12);
            boardLayout->setVerticalSpacing(10);
            auto* header = new QLabel(QStringLiteral("Propriétés — Poutre B12"), &board);
            header->setProperty("panelHeader", true);
            boardLayout->addWidget(header, 0, 0, 1, 4);
            auto button = [&](const QString& text, const char* prop, const QVariant& value, bool enabled = true) {
                auto* b = new QPushButton(text, &board);
                if (prop) b->setProperty(prop, value);
                b->setEnabled(enabled);
                return b;
            };
            boardLayout->addWidget(button(QStringLiteral("Créer"), "primary", true), 1, 0);
            boardLayout->addWidget(button(QStringLiteral("Annuler"), nullptr, {}), 1, 1);
            boardLayout->addWidget(button(QStringLiteral("Supprimer"), "destructive", true), 1, 2);
            boardLayout->addWidget(button(QStringLiteral("Appliquer"), "accentOutline", true), 1, 3);
            boardLayout->addWidget(button(QStringLiteral("Z actif"), "active", true), 2, 0);
            boardLayout->addWidget(button(QStringLiteral("X"), "active", false), 2, 1);
            boardLayout->addWidget(button(QStringLiteral("Valider EC3"), "success", true), 2, 2);
            boardLayout->addWidget(button(QStringLiteral("Désactivé"), "primary", true, false), 2, 3);
            auto edit = [&](const QString& text, const char* prop, bool readOnly = false, bool enabled = true) {
                auto* e = new QLineEdit(text, &board);
                if (prop) e->setProperty(prop, true);
                e->setReadOnly(readOnly);
                e->setEnabled(enabled);
                return e;
            };
            boardLayout->addWidget(edit(QStringLiteral("6.000 m"), nullptr), 3, 0);
            boardLayout->addWidget(edit(QStringLiteral("IPE 300 (lecture seule)"), nullptr, true), 3, 1);
            boardLayout->addWidget(edit(QStringLiteral("-4,2 kN (invalide)"), "invalid"), 3, 2);
            boardLayout->addWidget(edit(QStringLiteral("Désactivé"), nullptr, false, false), 3, 3);
            auto* spin = new QDoubleSpinBox(&board);
            spin->setSuffix(QStringLiteral(" kN/m"));
            spin->setValue(12.5);
            spin->setProperty("emphasis", true);
            boardLayout->addWidget(spin, 4, 0);
            auto* combo = new QComboBox(&board);
            combo->addItems({ QStringLiteral("S235"), QStringLiteral("S355") });
            boardLayout->addWidget(combo, 4, 1);
            auto* check = new QCheckBox(QStringLiteral("Synchronisation"), &board);
            check->setChecked(true);
            check->setProperty("emphasis", true);
            boardLayout->addWidget(check, 4, 2);
            auto* chip = new QCheckBox(QStringLiteral("2D"), &board);
            chip->setProperty("chip", true);
            chip->setChecked(true);
            boardLayout->addWidget(chip, 4, 3);
            int col = 0;
            for (const char* status : { "info", "success", "warning", "error" })
            {
                auto* l = new QLabel(QStringLiteral("État : %1").arg(QString::fromLatin1(status)), &board);
                l->setProperty("status", status);
                boardLayout->addWidget(l, 5, col++);
            }
            auto* hint = new QLabel(QStringLiteral("Aide : unités SI (m, kN)"), &board);
            hint->setProperty("hint", true);
            boardLayout->addWidget(hint, 6, 0, 1, 2);
            auto* badge = new QLabel(QStringLiteral("Mode 2D : plan XZ"), &board);
            badge->setProperty("badge", "accent");
            boardLayout->addWidget(badge, 6, 2);
            auto* table = new QTableWidget(4, 3, &board);
            table->setHorizontalHeaderLabels({ QStringLiteral("Barre"), QStringLiteral("Section"), QStringLiteral("L (m)") });
            table->setAlternatingRowColors(true);
            for (int r = 0; r < 4; ++r)
                for (int cIdx = 0; cIdx < 3; ++cIdx) table->setItem(r, cIdx, new QTableWidgetItem(QStringLiteral("B%1-%2").arg(r + 1).arg(cIdx)));
            table->selectRow(1);
            boardLayout->addWidget(table, 7, 0, 1, 2);
            auto* tree = new QTreeWidget(&board);
            tree->setHeaderLabel(QStringLiteral("Navigateur de projet"));
            auto* beams = new QTreeWidgetItem(tree, { QStringLiteral("Poutres (12)") });
            new QTreeWidgetItem(beams, { QStringLiteral("B1 — IPE 300") });
            auto* selected = new QTreeWidgetItem(beams, { QStringLiteral("B2 — IPE 300") });
            new QTreeWidgetItem(tree, { QStringLiteral("Poteaux (8)") });
            tree->expandAll();
            tree->setCurrentItem(selected);
            boardLayout->addWidget(tree, 7, 2, 1, 2);
            for (const bool dark : { true, false })
            {
                tm.setDarkMode(dark, true);
                const QString suffix = dark ? QStringLiteral("_dark") : QStringLiteral("_light");
                board.resize(760, 520);
                board.ensurePolished();
                board.grab().save(QDir(folder).filePath(QStringLiteral("design_system") + suffix + QStringLiteral(".png")));
                grid.resize(480, 600);
                grid.grab().save(QDir(folder).filePath(QStringLiteral("grid_dialog") + suffix + QStringLiteral(".png")));
                console.resize(900, 175);
                console.appendLog(QStringLiteral("Calcul OpenSees terminé"), QStringLiteral("SYS"), QStringLiteral("Analysis"));
                console.appendLog(QStringLiteral("Barre B12 : élancement élevé"), QStringLiteral("WARN"), QStringLiteral("Standards"));
                console.grab().save(QDir(folder).filePath(QStringLiteral("console") + suffix + QStringLiteral(".png")));
                help.resize(900, 600);
                help.grab().save(QDir(folder).filePath(QStringLiteral("help") + suffix + QStringLiteral(".png")));
            }
        }
        tm.setDarkMode(wasDark, true);
        qApp->setStyleSheet(previousStyleSheet);
        qApp->setPalette(previousPalette);
        std::cout << "[PASS] Test 611: Système de conception — jetons, contrastes WCAG, feuille unique, bascule de thème" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 612 : logo du produit lisible en thème clair (barre de titre, accueil, barre d'état). Avant : carré sombre et T gris
    // fin (#D3D7DA) peu visibles sur fond clair ; le thème sombre garde l'icône officielle.
    // -------------------------------------------------------------------------
    {
        using TSA::UI::ThemeManager;
        QFile official(QString::fromLatin1(TSA::Product::kIconSvg));
        TEST_CHECK(official.open(QIODevice::ReadOnly) && TSA::UI::productLogoSvg(true) == official.readAll(),
                   "Test 612: thème sombre — icône officielle inchangée");
        const QString light = QString::fromUtf8(TSA::UI::productLogoSvg(false));
        const QString ink = ThemeManager::lightTokens().text.name();
        TEST_CHECK(light.contains(ink, Qt::CaseInsensitive) && !light.contains(QStringLiteral("#D3D7DA"), Qt::CaseInsensitive),
                   "Test 612: thème clair — glyphe en aplat de la couleur du texte (" << ink.toStdString() << ")");
        // Rendu à la taille de la barre de titre, posé sur le fond des barres claires : traits opaques contrastés
        for (const int side : { 16, 22, 40 })
        {
            const QImage img = TSA::UI::productLogoIcon(false).pixmap(side, side).toImage().convertToFormat(QImage::Format_ARGB32);
            const QColor bg = ThemeManager::lightTokens().chrome;
            int opaque = 0;
            double minContrast = 100.0;
            const auto lum = [](const QColor& col) {
                const auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
                return 0.2126 * lin(col.redF()) + 0.7152 * lin(col.greenF()) + 0.0722 * lin(col.blueF());
            };
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                {
                    const QColor px = QColor::fromRgba(img.pixel(x, y));
                    if (px.alpha() < 230) continue;
                    ++opaque;
                    const double a = lum(px), b = lum(bg);
                    minContrast = std::min(minContrast, (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05));
                }
            TEST_CHECK(opaque >= side * side / 20 && minContrast >= 7.0,
                       "Test 612: logo clair " << side << " px — " << opaque << " pixels opaques, contraste minimal " << minContrast << ":1");
        }
        std::cout << "[PASS] Test 612: Logo du produit lisible en thème clair, icône officielle en thème sombre" << std::endl;
        ++passed;
    }

    return true;
}
