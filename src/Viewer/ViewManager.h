#pragma once

#include <QObject>
#include <V3d_View.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include "../Coordinate/WorkPlane.h"

namespace TSA::Viewer
{

/**
 * @brief Énumération des vues de caméra standard dans TSA (Section 5, 7, 8).
 */
enum class StandardCameraView
{
    ThreeD_Axo,     // Vue Axonométrique 3D
    Top,            // Vue de Dessus (regard vers -Z)
    Bottom,         // Vue de Dessous (regard vers +Z)
    Front,          // Vue de Face (regard vers +Y)
    Back,           // Vue Arrière (regard vers -Y)
    Left,           // Vue Gauche (regard vers +X)
    Right,          // Vue Droite (regard vers -X)
    NormalToPlane,  // Vue perpendiculaire face au WorkPlane actif (+Zwp)
    OppositePlane,  // Vue arrière du WorkPlane actif (-Zwp)
    Custom          // Orientation libre
};

/**
 * @brief Gestionnaire de caméra et de vue spécialisé (Règle 5, 7 & 11).
 * Contrôle exclusivement :
 * - Orientation de la caméra
 * - Cadrage (FitAll, Zoom)
 * - Type de projection caméra (Perspective vs Orthographique)
 * - Vues Face, Arrière, Dessus, Dessous, Gauche, Droite, Normal au plan
 *
 * IMPORTANT : Ne modifie JAMAIS le modèle structural ni les coordonnées du WorkPlane.
 */
class ViewManager : public QObject
{
    Q_OBJECT

public:
    explicit ViewManager(QObject* parent = nullptr);
    ~ViewManager() override = default;

    StandardCameraView currentView() const noexcept { return m_currentView; }
    void setCurrentView(StandardCameraView v) noexcept { m_currentView = v; }

    bool isOrthographic() const noexcept { return m_isOrthographic; }
    void setOrthographic(bool ortho, const Handle(V3d_View)& view);

    // Application d'une orientation standard
    void applyStandardView(StandardCameraView stdView,
                           const Handle(V3d_View)& view,
                           const TSA::Coordinate::WorkPlane& wp);

    // Orientation face au plan actif
    void viewNormalToWorkPlane(const Handle(V3d_View)& view,
                               const TSA::Coordinate::WorkPlane& wp,
                               bool opposite = false);

signals:
    void cameraViewChanged(StandardCameraView view);
    void projectionTypeChanged(bool isOrthographic);

private:
    StandardCameraView m_currentView = StandardCameraView::ThreeD_Axo;
    bool m_isOrthographic = false;
};

} // namespace TSA::Viewer
