#pragma once

// Style des annotations de la vue 3D (textes des nœuds, appuis, charges, diagrammes, réactions, légendes, grilles) :
// un seul point de réglage pour toutes les classes de rendu (docs/DISPLAY_SCALES.md).
//
// Deux modes, qui n'agissent que sur la représentation (jamais sur la géométrie ni sur les valeurs) :
//   - taille constante à l'écran : hauteur en pixels logiques × facteur d'échelle de l'écran (DPI), indépendante du
//     zoom de la caméra (texte toujours lisible) ;
//   - taille liée au modèle : hauteur en mètres du modèle, convertie en pixels à l'échelle de la vue (bornée à [8, 32] px)
//     et réappliquée quand la caméra s'arrête : le texte grandit et rétrécit avec la vue. Jamais un texte OCCT « zoomable » :
//     OCCT ne le trace pas sous ~16 unités du monde (un texte de 0,3 m sortait haut de ~11 m, BUG-178).
// La hauteur de chaque texte = taille de base × facteur de son rôle (une valeur de charge est un peu plus petite qu'un
// nom de nœud, une légende un peu plus grande). Réglages enregistrés dans les paramètres de l'application.

#include <AIS_TextLabel.hxx>

#include <QObject>

namespace TSA::Viewer
{

enum class AnnotationSizeMode
{
    ScreenConstant,
    ModelLinked
};

enum class AnnotationRole
{
    NodeLabel,
    SupportLabel,
    LoadValue,
    DiagramValue,
    ReactionValue,
    Extremum,
    Legend,
    GridLabel,
    GridLabelBold
};

struct AnnotationSettings
{
    AnnotationSizeMode mode = AnnotationSizeMode::ScreenConstant;
    double basePixels = 12.0;          ///< taille de base (8 à 32 px logiques)
    double modelHeight = 0.20;         ///< mode lié au modèle : hauteur (m) d'un texte de taille de base
    double devicePixelRatio = 1.0;     ///< facteur d'échelle de l'écran (DPI), fourni par la vue
    double pixelsPerMeter = 0.0;       ///< échelle de la vue (px logiques par mètre, par paliers de 15 %), fournie par la vue ; 0 : inconnue
    bool operator==(const AnnotationSettings&) const = default;
};

class AnnotationStyle : public QObject
{
    Q_OBJECT
public:
    static AnnotationStyle& instance();

    static constexpr double kMinPixels = 8.0, kMaxPixels = 32.0, kDefaultPixels = 12.0;

    const AnnotationSettings& settings() const { return m_settings; }
    void setMode(AnnotationSizeMode mode);
    void setBasePixels(double px);              ///< borné à [8, 32]
    void setModelHeight(double metres);
    void setDevicePixelRatio(double ratio);
    /// Échelle de la vue (px logiques par mètre au centre de la vue), donnée par la vue quand la caméra s'arrête. Arrondie par
    /// paliers de 15 % ; en mode lié au modèle, changed() n'est émis que si la hauteur des textes change (≥ 0,5 px).
    /// Non enregistrée. Plusieurs vues : la dernière vue arrêtée fixe l'échelle.
    void setViewScale(double pixelsPerMeter);
    void reset();                               ///< valeurs initiales (constante à l'écran, 12 px)
    /// Adapter automatiquement : taille de base selon la hauteur de la vue (fenêtre petite → texte plus petit) et
    /// hauteur liée au modèle selon son envergure.
    void adaptTo(int viewHeightPx, double modelSpan);

    static double roleFactor(AnnotationRole role);
    /// Hauteur du texte en pixels physiques : taille de base (constante à l'écran), ou hauteur en mètres × échelle de la vue
    /// bornée à [8, 32] px (liée au modèle ; taille de base tant que l'échelle de la vue est inconnue) ; × rôle × facteur d'écran.
    double height(AnnotationRole role) const;
    /// Applique hauteur et mode au texte (à appeler à chaque création d'étiquette).
    void apply(const Handle(AIS_TextLabel)& label, AnnotationRole role) const;

    void load();   ///< paramètres de l'application
    void save() const;

signals:
    /// Réglages modifiés : les vues reconstruisent leurs annotations (au mouvement de caméra : seulement en mode lié au
    /// modèle, à l'arrêt, si la hauteur change).
    void changed();

private:
    AnnotationStyle();
    void commit(const AnnotationSettings& s);
    AnnotationSettings m_settings;
};

} // namespace TSA::Viewer
