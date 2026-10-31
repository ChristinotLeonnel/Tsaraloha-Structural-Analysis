#pragma once

// Réglages d'affichage des repères de la vue 3D (repère global, indicateur d'angle, repères locaux des éléments) : un seul
// point de réglage, enregistré dans les paramètres de l'application. N'agit que sur la représentation.

#include <QObject>

namespace TSA::Viewer
{

enum class LocalAxisAnchor
{
    ElementCenter,      ///< centre géométrique de l'élément
    SelectedLocation    ///< point cliqué sur l'élément (centre si l'élément est choisi autrement ou s'il y en a plusieurs)
};

struct AxisDisplaySettings
{
    bool globalAxes = true;           ///< trièdre X, Y, Z à l'origine du modèle
    bool cornerIndicator = true;      ///< trièdre fixe dans le coin de la vue
    bool localAxes = true;            ///< repère x, y, z des éléments sélectionnés
    bool letters = true;              ///< lettres en bout d'axe (origine du modèle et éléments)
    double axisPixels = 70.0;         ///< longueur des axes, pixels logiques
    double labelPixels = 16.0;        ///< hauteur des lettres, pixels logiques
    LocalAxisAnchor anchor = LocalAxisAnchor::ElementCenter;
    bool operator==(const AxisDisplaySettings&) const = default;
};

class AxisDisplayStyle : public QObject
{
    Q_OBJECT
public:
    static AxisDisplayStyle& instance();

    static constexpr double kMinAxisPixels = 30.0, kMaxAxisPixels = 200.0;
    static constexpr double kMinLabelPixels = 8.0, kMaxLabelPixels = 32.0;

    const AxisDisplaySettings& settings() const { return m_settings; }
    /// Applique des réglages : longueurs bornées, enregistrement et signal changed() s'ils diffèrent.
    void setSettings(const AxisDisplaySettings& settings);
    void setGlobalAxes(bool on);
    void setCornerIndicator(bool on);
    void setLocalAxes(bool on);
    void setLetters(bool on);
    void setAxisPixels(double px);     ///< borné à [30, 200]
    void setLabelPixels(double px);    ///< borné à [8, 32]
    void setAnchor(LocalAxisAnchor anchor);
    void reset();                      ///< valeurs initiales

    void load();   ///< paramètres de l'application
    void save() const;

signals:
    /// Réglages modifiés : les vues mettent à jour leurs repères (aucune géométrie du modèle n'est touchée).
    void changed();

private:
    AxisDisplayStyle();
    AxisDisplaySettings m_settings;
};

} // namespace TSA::Viewer
