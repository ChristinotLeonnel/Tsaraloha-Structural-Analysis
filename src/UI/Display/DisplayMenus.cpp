#include "DisplayMenus.h"

#include "../../Geometry/LoadDisplayScale.h"
#include "../../Model/Model.h"
#include "../../Viewer/AnnotationStyle.h"
#include "../../Viewer/AxisDisplayStyle.h"
#include "../../Viewer/DisplayProfile.h"
#include "../../Viewer/OccView.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>
#include <vector>

namespace TSA::UI
{

DisplayMenus::DisplayMenus(QWidget* dialogParent, Context context)
    : QObject(dialogParent)
    , m_parent(dialogParent)
    , m_context(std::move(context))
{
    buildAnnotationMenu();
    buildAxisMenu();
    buildLoadMenu();
    m_profilesMenu = new QMenu(tr("Profils d'affichage"), m_parent);
    m_profilesMenu->setToolTipsVisible(true);
    connect(m_profilesMenu, &QMenu::aboutToShow, this, &DisplayMenus::rebuildProfilesMenu);
    rebuildProfilesMenu();
    // Diagnostic d'affichage (développeur) : objets de la scène et échelles graphiques de la vue du document
    m_diagnostic = new QAction(tr("Diagnostic d'affichage…"), this);
    connect(m_diagnostic, &QAction::triggered, this, [this] {
        const QString text = diagnosticText();
        if (text.isEmpty()) return;
        emit message(text, QStringLiteral("INFO"));
        QMessageBox box(QMessageBox::Information, tr("Diagnostic d'affichage"), text, QMessageBox::Ok, m_parent);
        box.setTextInteractionFlags(Qt::TextSelectableByMouse);
        box.exec();
    });
}

void DisplayMenus::addTo(QMenu* menu)
{
    if (!menu) return;
    menu->addMenu(m_annotationMenu);
    menu->addMenu(m_axisMenu);
    menu->addMenu(m_loadMenu);
    menu->addMenu(m_profilesMenu);
    menu->addAction(m_diagnostic);
}

QString DisplayMenus::diagnosticText() const
{
    OccView* v = view();
    return v ? v->displayDiagnosticsText() : QString();
}

void DisplayMenus::buildAnnotationMenu()
{
    using TSA::Viewer::AnnotationSizeMode;
    using TSA::Viewer::AnnotationStyle;
    auto& style = AnnotationStyle::instance();

    // Échelle des annotations : taille des textes indépendante de la géométrie (aucune valeur modifiée)
    m_annotationMenu = new QMenu(tr("Échelle des annotations"), m_parent);
    m_annotationMenu->setToolTipsVisible(true);
    auto* modes = new QActionGroup(m_annotationMenu);
    auto* screen = m_annotationMenu->addAction(tr("Taille constante à l'écran"));
    auto* model = m_annotationMenu->addAction(tr("Taille liée au modèle"));
    screen->setToolTip(tr("Textes lisibles quel que soit le zoom (hauteur en pixels, facteur d'écran compris)"));
    model->setToolTip(tr("Textes en mètres du modèle : ils grandissent et rétrécissent avec le zoom"));
    for (QAction* a : { screen, model }) a->setCheckable(true), modes->addAction(a);
    auto syncModes = [screen, model, &style] {
        screen->setChecked(style.settings().mode == AnnotationSizeMode::ScreenConstant);
        model->setChecked(style.settings().mode == AnnotationSizeMode::ModelLinked);
    };
    syncModes();
    connect(screen, &QAction::triggered, this, [&style] { style.setMode(AnnotationSizeMode::ScreenConstant); });
    connect(model, &QAction::triggered, this, [&style] { style.setMode(AnnotationSizeMode::ModelLinked); });
    connect(&style, &AnnotationStyle::changed, this, syncModes);
    m_annotationMenu->addSeparator();
    connect(m_annotationMenu->addAction(tr("Taille du texte…")), &QAction::triggered, this, [this, &style] {
        bool ok = false;
        const int px = QInputDialog::getInt(m_parent, tr("Taille des annotations"), tr("Hauteur des textes (pixels logiques, 8 à 32) :"),
                                            int(std::lround(style.settings().basePixels)), 8, 32, 1, &ok);
        if (ok) style.setBasePixels(px);
    });
    connect(m_annotationMenu->addAction(tr("Hauteur liée au modèle…")), &QAction::triggered, this, [this, &style] {
        bool ok = false;
        const double h = QInputDialog::getDouble(m_parent, tr("Taille liée au modèle"), tr("Hauteur d'un texte (m) :"), style.settings().modelHeight, 0.01, 10.0, 2, &ok);
        if (ok) style.setModelHeight(h);
    });
    connect(m_annotationMenu->addAction(tr("Adapter automatiquement")), &QAction::triggered, this, [this, &style] {
        double span = 10.0;
        const TSA::Model::Model* m = m_context.model ? m_context.model() : nullptr;
        if (m && !m->nodes().empty())
        {
            double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
            for (const auto& [id, n] : m->nodes())
            {
                const double c[3] = { n.x(), n.y(), n.z() };
                for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], c[k]), hi[k] = std::max(hi[k], c[k]);
            }
            span = std::max({ hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1.0 });
        }
        OccView* v = view();
        style.adaptTo(v ? v->height() : 800, span);
    });
    connect(m_annotationMenu->addAction(tr("Réinitialiser les tailles")), &QAction::triggered, this, [&style] { style.reset(); });
}

void DisplayMenus::buildAxisMenu()
{
    using TSA::Viewer::AxisDisplayStyle;
    using TSA::Viewer::LocalAxisAnchor;
    auto& style = AxisDisplayStyle::instance();

    m_axisMenu = new QMenu(tr("Repères"), m_parent);
    m_axisMenu->setToolTipsVisible(true);
    auto addCheck = [this](const QString& text, const QString& tip) {
        QAction* a = m_axisMenu->addAction(text);
        a->setCheckable(true);
        a->setToolTip(tip);
        return a;
    };
    QAction* global = addCheck(tr("Repère global"), tr("Trièdre X, Y, Z à l'origine du modèle."));
    QAction* corner = addCheck(tr("Indicateur d'angle"), tr("Trièdre fixe dans le coin de la vue."));
    QAction* local = addCheck(tr("Repères locaux"), tr("Trièdre x, y, z des éléments sélectionnés, tel que le calcul le définit."));
    QAction* letters = addCheck(tr("Lettres des axes"), tr("X, Y, Z pour le repère global ; x, y, z pour les repères locaux."));
    connect(global, &QAction::toggled, &style, &AxisDisplayStyle::setGlobalAxes);
    connect(corner, &QAction::toggled, &style, &AxisDisplayStyle::setCornerIndicator);
    connect(local, &QAction::toggled, &style, &AxisDisplayStyle::setLocalAxes);
    connect(letters, &QAction::toggled, &style, &AxisDisplayStyle::setLetters);

    m_axisMenu->addSeparator();
    auto* anchors = new QActionGroup(m_axisMenu);
    QAction* atCenter = addCheck(tr("Repère local au centre de l'élément"), QString());
    QAction* atPoint = addCheck(tr("Repère local au point cliqué"), tr("Sinon au centre : élément choisi dans l'arborescence, sélection multiple."));
    anchors->addAction(atCenter);
    anchors->addAction(atPoint);
    connect(atCenter, &QAction::triggered, &style, [&style] { style.setAnchor(LocalAxisAnchor::ElementCenter); });
    connect(atPoint, &QAction::triggered, &style, [&style] { style.setAnchor(LocalAxisAnchor::SelectedLocation); });

    m_axisMenu->addSeparator();
    connect(m_axisMenu->addAction(tr("Taille des axes…")), &QAction::triggered, this, [this, &style] {
        bool ok = false;
        const int px = QInputDialog::getInt(m_parent, tr("Taille des axes"), tr("Longueur des axes (pixels logiques, 30 à 200) :"),
                                            int(std::lround(style.settings().axisPixels)), 30, 200, 5, &ok);
        if (ok) style.setAxisPixels(px);
    });
    connect(m_axisMenu->addAction(tr("Taille des lettres…")), &QAction::triggered, this, [this, &style] {
        bool ok = false;
        const int px = QInputDialog::getInt(m_parent, tr("Taille des lettres"), tr("Hauteur des lettres (pixels logiques, 8 à 32) :"),
                                            int(std::lround(style.settings().labelPixels)), 8, 32, 1, &ok);
        if (ok) style.setLabelPixels(px);
    });
    connect(m_axisMenu->addAction(tr("Réinitialiser les repères")), &QAction::triggered, this, [&style] { style.reset(); });

    auto sync = [=, &style] {
        const auto& s = style.settings();
        for (auto [action, on] : { std::pair { global, s.globalAxes }, std::pair { corner, s.cornerIndicator }, std::pair { local, s.localAxes },
                                   std::pair { letters, s.letters } })
        {
            const QSignalBlocker block(action);
            action->setChecked(on);
        }
        (s.anchor == LocalAxisAnchor::SelectedLocation ? atPoint : atCenter)->setChecked(true);
    };
    sync();
    connect(&style, &AxisDisplayStyle::changed, this, sync);
}

void DisplayMenus::buildLoadMenu()
{
    // Échelle des charges : longueur des flèches seulement (les charges transmises au calcul sont inchangées)
    using TSA::Geometry::LoadScaleMode;
    using TSA::Geometry::LoadScaleSettings;
    m_loadMenu = new QMenu(tr("Échelle des charges"), m_parent);
    m_loadMenu->setToolTipsVisible(true);
    auto* lmodes = new QActionGroup(m_loadMenu);
    struct ModeItem
    {
        QString text, tip;
        LoadScaleMode mode;
    };
    const ModeItem items[] = {
        { tr("Proportionnelle"), tr("Longueur proportionnelle à l'intensité dans chaque famille (kN, kN/m, kN/m², kN·m)"), LoadScaleMode::Proportional },
        { tr("Automatique (minimum garanti)"), tr("Proportionnelle, avec une longueur minimale pour que les charges faibles restent visibles"), LoadScaleMode::Automatic },
        { tr("Normalisée"), tr("Toutes les flèches de même longueur ; l'intensité est donnée par la valeur affichée"), LoadScaleMode::Normalized },
    };
    auto modeActions = std::make_shared<std::vector<std::pair<QAction*, LoadScaleMode>>>();
    for (const auto& it : items)
    {
        QAction* a = m_loadMenu->addAction(it.text);
        a->setToolTip(it.tip);
        a->setCheckable(true);
        modeActions->emplace_back(a, it.mode);
        lmodes->addAction(a);
        const LoadScaleMode mode = it.mode;
        connect(a, &QAction::triggered, this, [this, mode] {
            OccView* v = view();
            if (!v) return;
            auto s = v->loadScaleSettings();
            s.mode = mode;
            v->setLoadScaleSettings(s);
        });
    }
    m_loadMenu->addSeparator();
    connect(m_loadMenu->addAction(tr("Facteur d'affichage…")), &QAction::triggered, this, [this] {
        OccView* v = view();
        if (!v) return;
        bool ok = false;
        const double f = QInputDialog::getDouble(m_parent, tr("Échelle des charges"), tr("Facteur global d'affichage des flèches :"), v->loadScale(), 0.05, 20.0, 2, &ok);
        if (ok) v->setLoadScale(f);
    });
    connect(m_loadMenu->addAction(tr("Longueur minimale…")), &QAction::triggered, this, [this] {
        OccView* v = view();
        if (!v) return;
        bool ok = false;
        auto s = v->loadScaleSettings();
        const double r = QInputDialog::getDouble(m_parent, tr("Échelle des charges"), tr("Longueur minimale (mode automatique), en % de la flèche maximale :"),
                                                 s.minRatio * 100.0, 0.0, 100.0, 0, &ok);
        if (!ok) return;
        s.minRatio = r / 100.0;
        v->setLoadScaleSettings(s);
    });
    auto* values = m_loadMenu->addAction(tr("Afficher les valeurs et unités"));
    auto* legend = m_loadMenu->addAction(tr("Afficher la légende des charges"));
    for (QAction* a : { values, legend }) a->setCheckable(true), a->setChecked(true);
    connect(values, &QAction::triggered, this, [this](bool on) {
        OccView* v = view();
        if (!v) return;
        auto s = v->loadScaleSettings();
        s.showValues = s.showUnits = on;
        v->setLoadScaleSettings(s);
    });
    connect(legend, &QAction::triggered, this, [this](bool on) {
        OccView* v = view();
        if (!v) return;
        auto s = v->loadScaleSettings();
        s.showLegend = on;
        v->setLoadScaleSettings(s);
    });
    connect(m_loadMenu->addAction(tr("Réinitialiser l'échelle des charges")), &QAction::triggered, this, [this] {
        OccView* v = view();
        if (!v) return;
        v->setLoadScale(1.0);
        v->setLoadScaleSettings(LoadScaleSettings {});
    });
    // Coches relues dans la vue du document à chaque ouverture du menu (la vue peut avoir changé : onglet, profil)
    connect(m_loadMenu, &QMenu::aboutToShow, this, [this, modeActions, values, legend] {
        OccView* v = view();
        const LoadScaleSettings s = v ? v->loadScaleSettings() : LoadScaleSettings {};
        for (const auto& [a, mode] : *modeActions) a->setChecked(mode == s.mode);
        values->setChecked(s.showValues);
        legend->setChecked(s.showLegend);
    });
}

void DisplayMenus::rebuildProfilesMenu()
{
    m_profilesMenu->clear();
    TSA::Viewer::DisplayProfileStore store;
    auto* group = new QActionGroup(m_profilesMenu);
    for (const auto& p : store.profiles())
    {
        // Nom = clé du profil ; profils fournis traduits à l'affichage seulement (noms personnels inchangés)
        QAction* a = m_profilesMenu->addAction(QCoreApplication::translate("TSA::Viewer::DisplayProfile", p.name.toUtf8().constData()));
        a->setCheckable(true);
        a->setChecked(p.name == m_activeProfile);
        a->setToolTip(tr("Applique les réglages graphiques du profil (aucune donnée du modèle ni des résultats n'est modifiée)"));
        group->addAction(a);
        const QString name = p.name;
        connect(a, &QAction::triggered, this, [this, name] { applyProfile(name); });
    }
    m_profilesMenu->addSeparator();
    connect(m_profilesMenu->addAction(tr("Enregistrer l'affichage actuel comme profil…")), &QAction::triggered, this, [this] {
        OccView* v = view();
        if (!v) return;
        bool ok = false;
        const QString name = QInputDialog::getText(m_parent, tr("Profil d'affichage"), tr("Nom du profil :"), QLineEdit::Normal,
                                                   m_activeProfile.isEmpty() ? tr("Mon profil") : m_activeProfile, &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        TSA::Viewer::DisplayProfileStore store;
        if (store.contains(name) &&
            QMessageBox::question(m_parent, tr("Profil d'affichage"), tr("Le profil « %1 » existe : le remplacer par l'affichage actuel ?").arg(name)) != QMessageBox::Yes)
            return;
        QString error;
        if (!store.save({ name, TSA::Viewer::captureDisplaySettings(*v) }, &error))
        {
            QMessageBox::warning(m_parent, tr("Profil d'affichage"), error);
            return;
        }
        m_activeProfile = name;
        if (m_context.documentPath) store.setDocumentProfile(m_context.documentPath(), name);
        emit message(tr("Profil d'affichage « %1 » enregistré").arg(name), QStringLiteral("INFO"));
    });
    auto pick = [this](const QString& title) {
        TSA::Viewer::DisplayProfileStore store;
        QStringList names;
        for (const auto& p : store.profiles()) names << p.name;
        bool ok = false;
        const QString n = QInputDialog::getItem(m_parent, title, tr("Profil :"), names, 0, false, &ok);
        return ok ? n : QString();
    };
    connect(m_profilesMenu->addAction(tr("Renommer un profil…")), &QAction::triggered, this, [this, pick] {
        const QString from = pick(tr("Renommer un profil"));
        if (from.isEmpty()) return;
        bool ok = false;
        const QString to = QInputDialog::getText(m_parent, tr("Renommer un profil"), tr("Nouveau nom :"), QLineEdit::Normal, from, &ok).trimmed();
        if (!ok || to == from) return;
        QString error;
        TSA::Viewer::DisplayProfileStore store;
        if (!store.rename(from, to, &error)) QMessageBox::warning(m_parent, tr("Renommer un profil"), error);
        else if (m_activeProfile == from) m_activeProfile = to;
    });
    connect(m_profilesMenu->addAction(tr("Supprimer un profil…")), &QAction::triggered, this, [this, pick] {
        const QString name = pick(tr("Supprimer un profil"));
        if (name.isEmpty()) return;
        if (QMessageBox::question(m_parent, tr("Supprimer un profil"), tr("Supprimer le profil « %1 » ?").arg(name)) != QMessageBox::Yes) return;
        TSA::Viewer::DisplayProfileStore().remove(name);
        if (m_activeProfile == name) m_activeProfile.clear();
    });
    connect(m_profilesMenu->addAction(tr("Rétablir les profils fournis")), &QAction::triggered, this, [] {
        TSA::Viewer::DisplayProfileStore().restoreDefaults();
    });
}

bool DisplayMenus::applyProfile(const QString& name)
{
    OccView* v = view();
    if (!v) return false;
    TSA::Viewer::DisplayProfileStore store;
    if (!store.contains(name)) return false;
    const QStringList unknown = TSA::Viewer::applyDisplaySettings(*v, store.profile(name).settings);
    if (!unknown.isEmpty()) emit message(tr("Profil « %1 » : réglages inconnus ignorés (%2)").arg(name, unknown.join(", ")), QStringLiteral("WARN"));
    m_activeProfile = name;
    if (m_context.documentPath) store.setDocumentProfile(m_context.documentPath(), name);
    emit profileApplied(name);
    emit message(tr("Profil d'affichage « %1 » appliqué").arg(name), QStringLiteral("INFO"));
    return true;
}

} // namespace TSA::UI
