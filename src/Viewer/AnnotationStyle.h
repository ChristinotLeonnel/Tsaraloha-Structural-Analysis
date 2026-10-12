#pragma once

// Style des annotations de la vue 3D (textes des nœuds, appuis, charges, diagrammes, réactions, légendes, grilles) :
// un seul point de réglage pour toutes les classes de rendu (docs/DISPLAY_SCALES.md).
//
// Deux modes, qui n'agissent que sur la représentation (jamais sur la géométrie ni sur les valeurs) :
//   - taille constante à l'écran : hauteur en pixels logiques × facteur d'échelle de l'écran (DPI), indépendante du
//     zoom de la caméra (texte toujours lisible) ;
//   - taille liée au modèle : hauteur en mètres du modèle (texte « zoomable »), qui grandit et rétrécit avec la vue.
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
    void reset();                               ///< valeurs initiales (constante à l'écran, 12 px)
    /// Adapter automatiquement : taille de base selon la hauteur de la vue (fenêtre petite → texte plus petit) et
    /// hauteur liée au modèle selon son envergure.
    void adaptTo(int viewHeightPx, double modelSpan);

    static double roleFactor(AnnotationRole role);
    /// Hauteur du texte : pixels physiques (constante à l'écran) ou mètres (liée au modèle).
    double height(AnnotationRole role) const;
    /// Applique hauteur et mode au texte (à appeler à chaque création d'étiquette).
    void apply(const Handle(AIS_TextLabel)& label, AnnotationRole role) const;

    void load();   ///< paramètres de l'application
    void save() const;

signals:
    /// Réglages modifiés : les vues reconstruisent leurs annotations (aucune reconstruction au mouvement de caméra).
    void changed();

private:
    AnnotationStyle();
    void commit(const AnnotationSettings& s);
    AnnotationSettings m_settings;
};

} // namespace TSA::Viewer
