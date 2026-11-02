#pragma once
// Style d'affichage des charges (docs/DISPLAY_SCALES.md § 4) : réglage de l'application, à la manière d'un style de cote
// AutoCAD — cas affichés, code couleur, texte des valeurs (hauteur liée au modèle ou constante à l'écran, décimales,
// symbole, unité, nom du cas, fond), épaisseur des flèches. Purement graphique : aucune valeur de charge n'est modifiée.
#include "../Geometry/LoadDisplayScale.h"

#include <QColor>
#include <QObject>
#include <QString>

namespace TSA::Viewer
{

enum class LoadCaseFilter
{
    ActiveCase,   ///< seul le cas actif du projet (LoadManager::activeLoadCaseId) est dessiné
    AllCases      ///< tous les cas en même temps (flèches et valeurs superposées)
};

enum class LoadColorMode
{
    ByCase,        ///< une couleur par cas de charge (palette), flèches et texte assortis
    ByIntensity,   ///< dégradé bleu → rouge selon l'intensité dans la famille d'unités
    ByFamily,      ///< une couleur par famille (forces, moments, linéiques, surfaciques)
    Single         ///< une seule couleur
};

enum class LoadTextSize
{
    /// Hauteur en mètres, comme un texte de cote, convertie en pixels à l'échelle de la vue et bornée à
    /// [textMinPixels, textMaxPixels] : le texte grandit en zoomant sans jamais devenir illisible ni démesuré.
    /// Jamais un texte OCCT « zoomable » : OCCT ne trace pas un texte zoomable de moins de ~16 unités du monde (un texte
    /// de 0,25 m sortait haut de ~11 m et recouvrait la structure, BUG-177).
    ModelLinked,
    ScreenConstant  ///< hauteur en pixels, inchangée au zoom
};

struct LoadDisplaySettings
{
    LoadCaseFilter cases = LoadCaseFilter::ActiveCase;
    LoadColorMode colors = LoadColorMode::ByCase;
    QColor singleColor = QColor(0xD0, 0x30, 0x30);
    LoadTextSize textSize = LoadTextSize::ModelLinked;
    double textHeight = 0.0;     ///< m (mode lié au modèle) ; 0 : automatique d'après la structure
    double textPixels = 12.0;    ///< px logiques (mode constant à l'écran), 8 à 32
    double textMinPixels = 9.0;  ///< px logiques : plancher du mode lié au modèle (vue d'ensemble)
    double textMaxPixels = 16.0; ///< px logiques : plafond du mode lié au modèle (vue rapprochée)
    bool declutter = true;       ///< valeurs qui se chevauchent à l'écran masquées (la plus forte reste), zoomer pour les lire
    int decimals = 2;            ///< 0 à 4
    bool showSymbol = true;      ///< « q = », « P = », « M = »
    bool showUnits = true;
    bool showCaseName = false;   ///< « [Q] » après la valeur (utile avec « tous les cas »)
    bool textBackground = true;  ///< fond derrière le texte (lisible devant la structure)
    double arrowWidth = 1.0;     ///< facteur d'épaisseur des flèches (0,5 à 3)

    bool operator==(const LoadDisplaySettings&) const = default;
};

class LoadDisplayStyle : public QObject
{
    Q_OBJECT
public:
    static LoadDisplayStyle& instance();

    static constexpr double kMinTextPixels = 8.0, kMaxTextPixels = 32.0;
    static constexpr double kMinArrowWidth = 0.5, kMaxArrowWidth = 3.0;

    const LoadDisplaySettings& settings() const noexcept { return m_settings; }
    /// Valeurs bornées ; signal changed() seulement si quelque chose change. Enregistré dans les réglages de l'application.
    void setSettings(const LoadDisplaySettings& settings);
    void reset();

    /// Le cas caseId est-il dessiné ? activeCaseId : cas actif du projet.
    bool caseShown(int caseId, int activeCaseId) const noexcept;

    /// Couleur d'un cas de rang caseRank (0, 1, 2… dans l'ordre des cas du projet) : palette de 10 couleurs distinctes.
    static QColor caseColor(int caseRank);
    /// Dégradé bleu (0) → vert → orange → rouge (1).
    static QColor intensityColor(double ratio);
    static QColor familyColor(TSA::Geometry::LoadFamily family);
    /// Couleur d'une charge selon le mode : caseRank (cas), ratio = |I| / maximum de la famille, famille.
    QColor loadColor(int caseRank, double ratio, TSA::Geometry::LoadFamily family) const;

    /// Texte d'une valeur : « q = -12.50 kN/m [Q] » selon les réglages (symbole, décimales, unité, nom du cas).
    QString formatValue(const QString& symbol, double value, const QString& unit, const QString& caseName = {}) const;

    /// Hauteur du texte dans le mode lié au modèle : réglée, sinon automatique (0,25 × longueur de référence des flèches,
    /// soit ~0,25 m pour un étage de 3 m : la hauteur d'un texte de 2,5 mm sur un plan au 1/100).
    double modelTextHeight(double referenceLength) const;
    /// Hauteur d'une étiquette en pixels physiques : pixelsPerMeter = échelle de la vue à l'étiquette, dpr = facteur d'écran.
    /// Lié au modèle : modelTextHeight × pixelsPerMeter borné à [min, max] × dpr ; constant : textPixels × dpr.
    double labelPixels(double pixelsPerMeter, double referenceLength, double dpr = 1.0) const;

    void load();   ///< paramètres de l'application (Display/Loads/*)
    void save() const;

signals:
    /// Réglages modifiés : les vues redessinent les charges (aucune reconstruction au mouvement de caméra).
    void changed();

private:
    LoadDisplayStyle();
    static LoadDisplaySettings bounded(LoadDisplaySettings s);
    LoadDisplaySettings m_settings;
};

} // namespace TSA::Viewer
