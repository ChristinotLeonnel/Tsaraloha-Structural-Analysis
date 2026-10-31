#pragma once

// Trièdre de la vue 3D, seul dessin d'axes de TSA : flèches colorées, lettres en bout d'axe, taille constante à l'écran.
// Sert au repère global, aux repères locaux des éléments et au plan de travail. Le trièdre est construit une fois en pixels
// autour de son origine : le placer ne change que sa transformation, sans recalculer sa géométrie.

#include <AIS_Trihedron.hxx>
#include <AIS_ViewCube.hxx>
#include <Quantity_Color.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <array>
#include <string>

namespace TSA::Viewer
{

struct AxisTriadAppearance
{
    std::array<Quantity_Color, 3> colors;       ///< x, y, z
    std::array<std::string, 3> labels;          ///< lettres de x, y, z
    double lengthPixels = 70.0;                 ///< pixels physiques
    double labelPixels = 16.0;                  ///< pixels physiques
    bool letters = true;
    bool transverseAxes = true;                 ///< faux : axe x seul
    Quantity_Color outline;                     ///< contour des lettres, lisibles sur tout fond
};

/// Couleur d'axe à l'écran : les valeurs de AxisColorConfig lues en sRVB (lues en RVB linéaire, OCCT les affiche pâles).
Quantity_Color axisScreenColor(const Quantity_Color& configured);

/// Trièdre visuel, au-dessus de la scène. Son point d'ancrage compte dans V3d_View::FitAll (voir OccView::fitScene).
Handle(AIS_Trihedron) createAxisTriad(const AxisTriadAppearance& appearance);
/// Couleurs, lettres, tailles. Sur un trièdre déjà affiché : redessiner ensuite (AIS_InteractiveContext::Redisplay).
void styleAxisTriad(const Handle(AIS_Trihedron)& triad, const AxisTriadAppearance& appearance);
/// Axes du ViewCube : mêmes couleurs que les trièdres (colors, outline de l'apparence). Redessiner ensuite.
void styleViewCubeAxes(const Handle(AIS_ViewCube)& cube, const AxisTriadAppearance& appearance);
/// Place le trièdre. En persistance de zoom, son repère local est en pixels et son origine est le point d'ancrage en
/// coordonnées monde : la transformation locale ne porte que la rotation, la translation passe par l'ancrage.
void placeAxisTriad(const Handle(AIS_Trihedron)& triad, const gp_Trsf& frame);
void placeAxisTriad(const Handle(AIS_Trihedron)& triad, const gp_Pnt& origin, const gp_Dir& x, const gp_Dir& y, const gp_Dir& z);

} // namespace TSA::Viewer
