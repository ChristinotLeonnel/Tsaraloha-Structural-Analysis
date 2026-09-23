#include "HelpDialog.h"
#include "../Theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QIcon>
#include <QSplitter>

namespace TSA::UI
{

HelpDialog::HelpDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    updateTheme();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &HelpDialog::updateTheme);
}

void HelpDialog::setupUi()
{
    setWindowTitle(tr("Aide & Documentation - TSA Structural Modeler"));
    resize(860, 620);
    setMinimumSize(700, 500);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // Barre d'en-tête et recherche
    auto* topHeader = new QHBoxLayout();
    topHeader->setSpacing(10);

    auto* titleLabel = new QLabel(tr("📖 <b>Centre d'Aide & Documentation TSA</b>"), this);
    titleLabel->setStyleSheet("font-size: 14px;");
    topHeader->addWidget(titleLabel);

    topHeader->addStretch();

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("🔍 Rechercher un outil, raccourci, commande..."));
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setFixedWidth(260);
    connect(m_searchBox, &QLineEdit::textChanged, this, &HelpDialog::onSearchChanged);
    topHeader->addWidget(m_searchBox);

    mainLayout->addLayout(topHeader);

    // Séparateur horizontal
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Liste des rubriques à gauche
    m_topicsList = new QListWidget(splitter);
    m_topicsList->setFixedWidth(230);
    m_topicsList->setIconSize(QSize(20, 20));

    auto addTopic = [this](const QString& text) {
        auto* item = new QListWidgetItem(text, m_topicsList);
        item->setSizeHint(QSize(200, 36));
    };

    addTopic(tr("🚀 Démarrage & Workflow"));
    addTopic(tr("✏️ Modélisation 3D"));
    addTopic(tr("📐 Grilles 3D & Niveaux"));
    addTopic(tr("🎥 Vues, Caméra & Coupes"));
    addTopic(tr("⌨️ Raccourcis & Console"));
    addTopic(tr("⚙️ Analyse & Résultats EF"));
    addTopic(tr("ℹ️ À Propos de TSA"));

    connect(m_topicsList, &QListWidget::currentRowChanged, this, &HelpDialog::onTopicChanged);

    // Navigateur de texte à droite
    m_browser = new QTextBrowser(splitter);
    m_browser->setOpenExternalLinks(true);

    splitter->addWidget(m_topicsList);
    splitter->addWidget(m_browser);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter, 1);

    // Boutons inférieurs
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);

    auto* tipLabel = new QLabel(tr("Astuce : Vous pouvez aussi taper <b>HELP</b> ou <b>?</b> dans la console de commande."), this);
    tipLabel->setStyleSheet("color: #8B949E; font-size: 11px;");
    bottomLayout->addWidget(tipLabel);

    bottomLayout->addStretch();

    auto* btnClose = new QPushButton(tr("Fermer"), this);
    btnClose->setFixedWidth(90);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(btnClose);

    mainLayout->addLayout(bottomLayout);

    m_topicsList->setCurrentRow(0);
}

void HelpDialog::selectTopic(int index)
{
    if (m_topicsList && index >= 0 && index < m_topicsList->count())
    {
        m_topicsList->setCurrentRow(index);
    }
}

void HelpDialog::onTopicChanged(int row)
{
    if (m_browser && row >= 0)
    {
        m_browser->setHtml(getHelpContent(row));
    }
}

void HelpDialog::onSearchChanged(const QString& filter)
{
    if (filter.trimmed().isEmpty())
    {
        for (int i = 0; i < m_topicsList->count(); ++i)
        {
            m_topicsList->item(i)->setHidden(false);
        }
        return;
    }

    QString lower = filter.toLower();
    for (int i = 0; i < m_topicsList->count(); ++i)
    {
        bool match = m_topicsList->item(i)->text().toLower().contains(lower) ||
                     getHelpContent(i).toLower().contains(lower);
        m_topicsList->item(i)->setHidden(!match);
    }
}

void HelpDialog::updateTheme()
{
    const bool dark = ThemeManager::instance().isDarkMode();

    if (dark)
    {
        setStyleSheet(
            "QDialog { background-color: #1E2328; color: #E6EDF3; font-family: 'Segoe UI', sans-serif; }"
            "QListWidget {"
            "   background-color: #161B22;"
            "   color: #C9D1D9;"
            "   border: 1px solid #30363D;"
            "   border-radius: 4px;"
            "   font-size: 11px;"
            "}"
            "QListWidget::item {"
            "   padding: 4px 8px;"
            "   border-bottom: 1px solid #212830;"
            "}"
            "QListWidget::item:selected {"
            "   background-color: #1F3A5A;"
            "   color: #58A6FF;"
            "   font-weight: bold;"
            "}"
            "QTextBrowser {"
            "   background-color: #161B22;"
            "   color: #E6EDF3;"
            "   border: 1px solid #30363D;"
            "   border-radius: 4px;"
            "   padding: 10px;"
            "   font-family: 'Segoe UI', sans-serif;"
            "}"
            "QLineEdit {"
            "   background-color: #161B22;"
            "   color: #E6EDF3;"
            "   border: 1px solid #30363D;"
            "   border-radius: 3px;"
            "   padding: 3px 6px;"
            "}"
            "QPushButton {"
            "   background-color: #212830;"
            "   color: #E6EDF3;"
            "   border: 1px solid #30363D;"
            "   border-radius: 3px;"
            "   padding: 5px 12px;"
            "}"
            "QPushButton:hover { background-color: #30363D; border-color: #58A6FF; color: #58A6FF; }"
        );
    }
    else
    {
        setStyleSheet(
            "QDialog { background-color: #F6F8FA; color: #24292F; font-family: 'Segoe UI', sans-serif; }"
            "QListWidget {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   border-radius: 4px;"
            "   font-size: 11px;"
            "}"
            "QListWidget::item {"
            "   padding: 4px 8px;"
            "   border-bottom: 1px solid #F0F2F5;"
            "}"
            "QListWidget::item:selected {"
            "   background-color: #DDF4FF;"
            "   color: #0969DA;"
            "   font-weight: bold;"
            "}"
            "QTextBrowser {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   border-radius: 4px;"
            "   padding: 10px;"
            "   font-family: 'Segoe UI', sans-serif;"
            "}"
            "QLineEdit {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   border-radius: 3px;"
            "   padding: 3px 6px;"
            "}"
            "QPushButton {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   border-radius: 3px;"
            "   padding: 5px 12px;"
            "}"
            "QPushButton:hover { background-color: #EAEEF2; border-color: #0969DA; color: #0969DA; }"
        );
    }

    if (m_browser && m_topicsList)
    {
        m_browser->setHtml(getHelpContent(m_topicsList->currentRow()));
    }
}

QString HelpDialog::getHelpContent(int topicIndex) const
{
    const bool dark = ThemeManager::instance().isDarkMode();
    QString bodyColor = dark ? "#E6EDF3" : "#24292F";
    QString headerColor = dark ? "#58A6FF" : "#0969DA";
    QString codeBg = dark ? "#212830" : "#F6F8FA";
    QString borderColor = dark ? "#30363D" : "#D0D7DE";
    QString badgeBg = dark ? "#1F3A5A" : "#DDF4FF";
    QString badgeFg = dark ? "#58A6FF" : "#0969DA";
    QString cardBg = dark ? "#1E2328" : "#F6F8FA";

    QString style = QString(
        "<style>"
        "body { font-family: 'Segoe UI', -apple-system, sans-serif; color: %1; line-height: 1.5; font-size: 13px; }"
        "h2 { color: %2; border-bottom: 2px solid %4; padding-bottom: 4px; margin-top: 5px; }"
        "h3 { color: %2; margin-top: 14px; margin-bottom: 6px; }"
        "table { width: 100%; border-collapse: collapse; margin-top: 8px; margin-bottom: 12px; }"
        "th { background-color: %4; color: %1; text-align: left; padding: 6px 10px; border: 1px solid %4; font-size: 11px; }"
        "td { padding: 6px 10px; border: 1px solid %4; font-size: 11px; }"
        ".badge { background-color: %5; color: %6; padding: 2px 6px; border-radius: 3px; font-weight: bold; font-family: Consolas, monospace; }"
        ".card { background-color: %7; border: 1px solid %4; border-radius: 5px; padding: 10px; margin-bottom: 10px; }"
        "code { background-color: %3; border: 1px solid %4; border-radius: 3px; padding: 1px 4px; font-family: Consolas, monospace; }"
        "</style>"
    ).arg(bodyColor, headerColor, codeBg, borderColor, badgeBg, badgeFg, cardBg);

    switch (topicIndex)
    {
    case 0: // Démarrage & Workflow
        return style +
            "<h2>🚀 Démarrage & Workflow de Modélisation</h2>"
            "<p>Bienvenue dans <b>TSA (Tsaraloha Structural Analysis)</b>, le logiciel moderne de modélisation 3D et d'analyse structurelle par éléments finis basé sur OpenCASCADE et Qt 6.</p>"
            "<div class='card'>"
            "<b>Flux de travail standard pour un projet :</b>"
            "<ol>"
            "<li><b>Grilles & Niveaux d'Étages :</b> Définissez le repère altimétrique (étages RDC, R+1, etc.) et les axes orthonormés de votre bâtiment.</li>"
            "<li><b>Modélisation Géométrique :</b> Créez les nœuds structuraux, puis reliez-les par des poutres, poteaux et dalles. Vous pouvez également utiliser le générateur de <i>Cube Structurel 3D</i>.</li>"
            "<li><b>Profilés & Matériaux :</b> Définissez et assignez des sections transversales (IPE, HEA, béton armé) et les matériaux (Béton C25/30, Acier S355).</li>"
            "<li><b>Appuis & Liaisons :</b> Fixez les conditions aux limites (encastrements, articulations, appuis simples).</li>"
            "<li><b>Cas de Charges :</b> Appliquez des charges nodales, linéiques réparties, surfaciques ou moments.</li>"
            "<li><b>Analyse Éléments Finis :</b> Lancez le solveur statique ou modal pour inspecter les déformées, contraintes et diagrammes d'efforts (M, N, V).</li>"
            "</ol>"
            "</div>";

    case 1: // Modélisation 3D
        return style +
            "<h2>✏️ Outils de Modélisation 3D</h2>"
            "<p>TSA intègre des outils de dessin interactif 3D accélérés par OpenCASCADE :</p>"
            "<table>"
            "<tr><th>Outil</th><th>Raccourci</th><th>Description</th></tr>"
            "<tr><td><b>Sélection</b></td><td><span class='badge'>Échap</span></td><td>Permet d'inspecter les entités ou d'effectuer des sélections de zone :<br>"
            "• <i>Gauche vers Droite (Bleu)</i> : Sélection par inclusion totale (fenêtre englobante).<br>"
            "• <i>Droite vers Gauche (Vert)</i> : Sélection par capture/sécante (touché = sélectionné).</td></tr>"
            "<tr><td><b>Nœud</b></td><td><span class='badge'>N</span></td><td>Place un nœud structurel 3D par clic direct ou magnétisme de grille.</td></tr>"
            "<tr><td><b>Poutre</b></td><td><span class='badge'>B</span></td><td>Crée une barre linéaire reliant deux nœuds. Cliquez le premier nœud puis le second.</td></tr>"
            "<tr><td><b>Poteau</b></td><td><span class='badge'>C</span></td><td>Génère un élément vertical s'élevant automatiquement sur la hauteur du niveau actif.</td></tr>"
            "<tr><td><b>Dalle</b></td><td><span class='badge'>L</span></td><td>Modélise un panneau de plancher ou voile surfacique par un polygone fermé de nœuds.</td></tr>"
            "<tr><td><b>Cube Structurel</b></td><td>-</td><td>Génère automatiquement une travée 3D complète (8 nœuds, 4 poteaux, 8 poutres, 1 dalle).</td></tr>"
            "<tr><td><b>Déplacer</b></td><td><span class='badge'>M</span></td><td>Applique un vecteur de translation (dX, dY, dZ) aux éléments sélectionnés.</td></tr>"
            "<tr><td><b>Copier / Répéter</b></td><td><span class='badge'>Ctrl+D</span></td><td>Duplique la sélection avec répétition multiple (ex: colonnes d'un portique).</td></tr>"
            "<tr><td><b>Supprimer</b></td><td><span class='badge'>Suppr</span></td><td>Supprime immédiatement les entités sélectionnées du modèle.</td></tr>"
            "</table>";

    case 2: // Grilles 3D & Niveaux
        return style +
            "<h2>📐 Système de Grilles 3D & Altimétrie</h2>"
            "<p>Le système de repérage de TSA est unifié pour garantir une cohérence absolue entre vue 2D et espace 3D :</p>"
            "<div class='card'>"
            "<h3>Types de Grilles</h3>"
            "<ul>"
            "<li><b>Cartésienne :</b> Définition d'axes X et Y avec espacements personnalisés (ex: 5.0, 6.0, 4.5 m) et bulles d'identification alphabétiques ou numériques.</li>"
            "<li><b>Cylindrique :</b> Rayons (R) et divisions angulaires (&theta;) pour les structures circulaires ou réservoirs.</li>"
            "</ul>"
            "</div>"
            "<div class='card'>"
            "<h3>Gestion des Étages & Niveaux</h3>"
            "<ul>"
            "<li>Accédez au <b>Gestionnaire d'Étages</b> via <span class='badge'>Ctrl+L</span>.</li>"
            "<li>La barre supérieure au-dessus du viewport permet de basculer instantanément le plan de travail actif d'un étage à un autre (ex: Niveau 0 : 0.00m, Étage 1 : +3.20m).</li>"
            "<li>Touche <span class='badge'>G</span> pour afficher/masquer la grille, et touche <span class='badge'>S</span> pour activer le magnétisme d'accrochage (Snap).</li>"
            "</ul>"
            "</div>";

    case 3: // Vues, Caméra & Coupes
        return style +
            "<h2>🎥 Vues, Navigation Caméra & Coupes 3D</h2>"
            "<div class='card'>"
            "<h3>Navigation Souris dans le Viewport 3D</h3>"
            "<ul>"
            "<li><b>Rotation Orbitale (Orbit 3D) :</b> Maintenez le <i>Bouton Central (Molette)</i> ou <i>Bouton Droit</i> et déplacez la souris.</li>"
            "<li><b>Déplacement Latéral (Pan) :</b> Maintenez <span class='badge'>Maj</span> + <i>Bouton Central</i> et déplacez la souris.</li>"
            "<li><b>Zoom Continu :</b> Faites tourner la <i>Molette</i> de la souris vers l'avant ou l'arrière.</li>"
            "<li><b>Zoom Étendu (Fit All) :</b> Appuyez sur la touche <span class='badge'>F</span> pour cadrer l'ensemble du modèle.</li>"
            "<li><b>Réinitialiser la Vue :</b> Appuyez sur la touche <span class='badge'>R</span> pour restaurer l'angle isométrique par défaut.</li>"
            "</ul>"
            "</div>"
            "<div class='card'>"
            "<h3>Projections CAO Robot / AutoCAD</h3>"
            "<ul>"
            "<li><b>Plan XY :</b> Vue en plan horizontale à l'altitude du niveau sélectionné.</li>"
            "<li><b>Plan XZ :</b> Vue en élévation de face (idéale pour analyser les portiques).</li>"
            "<li><b>Plan YZ :</b> Vue en élévation latérale / pignon.</li>"
            "<li><b>Coupes 3D (Graphic3d_ClipPlane) :</b> Permet de trancher interactivement le bâtiment selon l'axe X, Y ou Z avec inversion de face.</li>"
            "</ul>"
            "</div>";

    case 4: // Raccourcis & Console
        return style +
            "<h2>⌨️ Raccourcis Clavier & Ligne de Commande</h2>"
            "<p>TSA propose deux modes d'action rapide : les raccourcis directs au clavier et la console interactive façon AutoCAD.</p>"
            "<h3>Raccourcis Clavier Principaux</h3>"
            "<table>"
            "<tr><th>Touche</th><th>Action</th></tr>"
            "<tr><td><span class='badge'>Échap</span></td><td>Activer le mode Sélection / Annuler la commande en cours</td></tr>"
            "<tr><td><span class='badge'>N</span></td><td>Dessiner un Nœud</td></tr>"
            "<tr><td><span class='badge'>B</span></td><td>Dessiner une Poutre</td></tr>"
            "<tr><td><span class='badge'>C</span></td><td>Dessiner un Poteau</td></tr>"
            "<tr><td><span class='badge'>L</span></td><td>Dessiner une Dalle</td></tr>"
            "<tr><td><span class='badge'>M</span></td><td>Déplacer les éléments sélectionnés</td></tr>"
            "<tr><td><span class='badge'>Ctrl+D</span></td><td>Copier / Dupliquer la sélection</td></tr>"
            "<tr><td><span class='badge'>Suppr</span></td><td>Supprimer les éléments sélectionnés</td></tr>"
            "<tr><td><span class='badge'>G</span></td><td>Afficher / Masquer la grille 3D</td></tr>"
            "<tr><td><span class='badge'>S</span></td><td>Activer / Désactiver le magnétisme (Snap)</td></tr>"
            "<tr><td><span class='badge'>Ctrl+L</span></td><td>Gestionnaire des niveaux altimétriques et étages</td></tr>"
            "<tr><td><span class='badge'>F</span></td><td>Zoom étendu (Fit All)</td></tr>"
            "<tr><td><span class='badge'>R</span></td><td>Réinitialiser la vue de caméra</td></tr>"
            "<tr><td><span class='badge'>Ctrl+T</span> ou <span class='badge'>F10</span></td><td>Basculer Mode Sombre / Mode Clair</td></tr>"
            "<tr><td><span class='badge'>F1</span></td><td>Ouvrir ce Centre d'Aide & Documentation</td></tr>"
            "</table>"
            "<h3>Commandes de la Ligne de Commande (Console)</h3>"
            "<p>Tapez ces mots-clés dans la barre inférieure :</p>"
            "<table>"
            "<tr><th>Commande</th><th>Effet</th></tr>"
            "<tr><td><code>NODE</code> ou <code>N</code></td><td>Mode création de nœud</td></tr>"
            "<tr><td><code>BEAM</code> ou <code>B</code></td><td>Mode création de poutre</td></tr>"
            "<tr><td><code>COLUMN</code> ou <code>C</code></td><td>Mode création de poteau</td></tr>"
            "<tr><td><code>SLAB</code> ou <code>L</code></td><td>Mode création de dalle</td></tr>"
            "<tr><td><code>GRID</code> ou <code>G</code></td><td>Bascule visibilité de la grille</td></tr>"
            "<tr><td><code>FIT</code></td><td>Zoom étendu automatique</td></tr>"
            "<tr><td><code>RESET</code></td><td>Réinitialisation orientation caméra</td></tr>"
            "<tr><td><code>MOVE</code> ou <code>M</code></td><td>Ouvre le dialogue de déplacement</td></tr>"
            "<tr><td><code>COPY</code></td><td>Ouvre le dialogue de copie multiple</td></tr>"
            "<tr><td><code>DEL</code> ou <code>DELETE</code></td><td>Supprime la sélection</td></tr>"
            "<tr><td><code>THEME</code> / <code>DARK</code> / <code>LIGHT</code></td><td>Bascule entre le mode sombre et le mode clair</td></tr>"
            "<tr><td><code>HELP</code> / <code>AIDE</code> / <code>?</code></td><td>Ouvre la fenêtre d'aide complète</td></tr>"
            "</table>";

    case 5: // Analyse & Résultats EF
        return style +
            "<h2>⚙️ Analyse & Résultats Éléments Finis</h2>"
            "<div class='card'>"
            "<h3>Calcul Statique & Modal</h3>"
            "<p>Le solveur matriciel résout le système d'équations globales :</p>"
            "<p align='center'><code>[K] · {U} = {F}</code></p>"
            "<ul>"
            "<li><b>[K] :</b> Matrice de rigidité globale assemblée à partir des rigidités locales 3D de poutres d'Euler-Bernoulli et Timoshenko (6 DDL par nœud).</li>"
            "<li><b>{U} :</b> Vecteur des déplacements et rotations nodales.</li>"
            "<li><b>{F} :</b> Vecteur des forces et moments appliqués.</li>"
            "</ul>"
            "</div>"
            "<div class='card'>"
            "<h3>Exploitation des Résultats</h3>"
            "<ul>"
            "<li><b>Déplacements :</b> Visualisation de la déformée élastique avec facteur d'amplification ajustable.</li>"
            "<li><b>Diagrammes d'efforts internes :</b> Courbes continues de Moment Fléchissant (M<sub>y</sub>, M<sub>z</sub>), Effort Tranchant (V<sub>y</sub>, V<sub>z</sub>) et Effort Normal (N).</li>"
            "<li><b>Cartographie des contraintes :</b> Calcul des contraintes normales maximales &sigma; et tangentielles &tau;, ainsi que la contrainte équivalente de Von Mises.</li>"
            "</ul>"
            "</div>";

    case 6: // À Propos de TSA
    default:
        return style +
            "<h2>ℹ️ À Propos de Tsaraloha Structural Analysis (TSA)</h2>"
            "<div class='card'>"
            "<p><b>TSA - Plateforme de Conception & Calcul de Structures 3D</b></p>"
            "<p>Développé pour les ingénieurs de génie civil, bureaux d'études et architectes recherchant la puissance de calcul allié à l'ergonomie CAO moderne.</p>"
            "<ul>"
            "<li><b>Version :</b> 1.0.0 (Release 2026)</li>"
            "<li><b>Moteur Géométrique 3D :</b> OpenCASCADE Technology 8.0.1 (OCCT)</li>"
            "<li><b>Interface Utilisateur :</b> Qt 6.11 (Architecture Ribbon AutoCAD, Mode Sombre / Clair dynamique)</li>"
            "<li><b>Standard C++ :</b> C++20 (MSVC 64-bit)</li>"
            "<li><b>Auteur / Concepteur :</b> Christinot TSARALOHA</li>"
            "</ul>"
            "</div>";
    }
}

} // namespace TSA::UI
