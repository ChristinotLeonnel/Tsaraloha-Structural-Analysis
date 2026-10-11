#pragma once

#include <string>
#include <vector>
#include <cmath>

namespace TSA::Model
{

enum class SectionShape
{
    Rectangular,
    Circular,
    IShape,
    Pipe,
    BoxHollow,
    UPN,
    Angle,
    TSection
};

struct Section
{
    int id = 0;
    std::string name = "Rect 300x500";
    SectionShape shape = SectionShape::Rectangular;

    // Dimensions en mètres
    double width = 0.30;   // b
    double height = 0.50;  // h
    double diameter = 0.30;// D
    double tw = 0.01;      // épaisseur âme (m)
    double tf = 0.015;     // épaisseur aile (m)

    // Caractéristiques géométriques calculées
    double area() const;
    double iy() const; // Inertie flexion selon axe fort (m4)
    double iz() const; // Inertie flexion selon axe faible (m4)
    /// Constante de torsion de Saint-Venant J (m⁴). Profils en I laminés normalisés (IPE, HEA, HEB, rayon de
    /// congé EN 10365 connu) : formule d'El Darwish & Johnston avec congés, celle des tables SCI ; autres
    /// formes : formules de parois minces / Bredt / sections pleines (docs/SECTIONS.md).
    double it() const;
    /// Constante de gauchissement Iw (m⁶) : profil en I bisymétrique tf·b³·(h − tf)²/24 ; nulle (négligeable)
    /// pour les sections pleines et fermées ; NaN si non disponible (UPN, cornière, T).
    double iw() const;
    /// Rayon de congé (m) des profils laminés normalisés reconnus par leur désignation (« IPE 300 », « HEA 200 »,
    /// « HEB 160 ») ET leurs dimensions EN 10365 ; 0 sinon (section paramétrique ou modifiée). Non stocké dans le
    /// fichier : déduit à chaque calcul, identique avant et après réouverture.
    double rootRadius() const;
    /// J d'un profil en I laminé à congés (El Darwish & Johnston, unités cohérentes).
    static double rolledIShapeTorsionConstant(double h, double b, double tw, double tf, double r);
    double wy() const; // Module de résistance élastique fort Wel,y = Iy / (h/2) (m3)
    double wz() const; // Module de résistance élastique faible Wel,z = Iz / (b/2) (m3)

    // Usines prédéfinies
    static Section rectangular(double b, double h, const std::string& name = "");
    static Section circular(double d, const std::string& name = "");
    static Section ipe(int number); // IPE 100, 120, 140, 160, 180, 200, 220, 240, 270, 300, 330, 360, 400
    static Section hea(int number); // HEA 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300
    static Section heb(int number); // HEB 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300
    static Section upn(int number); // UPN 80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300
    static Section angle(double h, double b, double t, const std::string& name = ""); // Cornière L h x b x t
    static Section tSection(double h, double b, double tw, double tf, const std::string& name = ""); // Profil en T
    static Section boxHollow(double b, double h, double tw, double tf = 0.0, const std::string& name = ""); // Tube rectangulaire
    static Section pipe(double diameter, double thickness, const std::string& name = ""); // Tube rond
    static std::vector<Section> defaultLibrary();
};

} // namespace TSA::Model
