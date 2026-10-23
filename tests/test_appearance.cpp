// Suite « appearance » : apparence des éléments par matériau (tests 220-221).
// Texture propre à chaque matériau, partagée entre objets ; couleur choisie par l'utilisateur
// teintant le grain du matériau ; panneau Propriétés n'imposant plus de couleur par défaut.

#include "test_common.h"

#include "App/ProductInfo.h"
#include "UI/Properties/ColumnPropertiesView.h"
#include "Viewer/MaterialVisual.h"
#include "Viewer/TextureManager.h"

#include "UI/Theme/ThemeManager.h"
#include "UI/Dialogs/DialogStandardHelper.h"
#include "UI/Widgets/StandardDialogHeader.h"
#include "UI/Dialogs/NewNodeDialog.h"
#include "UI/Dialogs/SectionCustomizationDialog.h"
#include "UI/Dialogs/LoadCaseDialog.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Prs3d_ShadingAspect.hxx>
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

        std::cout << "[PASS] Test 222: Conformité globale au Standard UI/UX TSA (Tailles, En-têtes, Sémantique)" << std::endl;
        ++passed;
    }
    return true;
}
