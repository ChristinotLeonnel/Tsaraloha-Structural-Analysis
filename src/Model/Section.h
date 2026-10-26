#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <vector>

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
    /// Rayon de congé âme-aile d'un profil en I (m), donnée de la section (BUG-110, format 2.0). Vide tant que la section
    /// n'est pas entrée dans le modèle : il vaut alors legacyRootRadius(). fixFilletRadius() le fige une fois (élément
    /// créé, fichier ≤ 1.12 relu, import) ; ensuite, ni un renommage ni un changement de dimension ne le modifient.
    std::optional<double> filletRadius;

    /// Version des formules des propriétés (A, I, J, W, congés) : enregistrée dans le chunk SECP ; toute évolution des
    /// formules l'incrémente et impose une migration explicite des projets (jamais un changement silencieux).
    static constexpr int kSectionModel = 1;

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
    /// Rayon de congé utilisé par les formules (m) : filletRadius s'il est figé, sinon legacyRootRadius() ; 0 hors
    /// profil en I.
    double rootRadius() const;
    /// Algorithme historique (≤ 1.12), conservé à l'identique pour la migration : rayon EN 10365 des profils reconnus
    /// par leur désignation (« IPE 300 », « HEA 200 », « HEB 160 ») ET leurs dimensions à 1e-7 près ; 0 sinon.
    double legacyRootRadius() const;
    /// Fige le rayon (algorithme historique) s'il ne l'est pas encore ; sans effet hors profil en I.
    void fixFilletRadius()
    {
        if (shape == SectionShape::IShape && !filletRadius) filletRadius = legacyRootRadius();
    }
    /// J d'un profil en I laminé à congés (El Darwish & Johnston, unités cohérentes).
    static double rolledIShapeTorsionConstant(double h, double b, double tw, double tf, double r);
    /// Module de résistance élastique Wel,y = Iy / v_max (m³), v_max = distance du centre de gravité à la fibre la
    /// plus éloignée selon la hauteur (h/2 pour les sections symétriques ; T et cornière : fibre opposée au centre).
    double wy() const;
    /// Module de résistance élastique Wel,z = Iz / w_max (m³) (b/2 si symétrique ; UPN et cornière : fibre extrême).
    double wz() const;
    /// Position du centre de gravité : distance à la fibre inférieure (selon h) et à la fibre de gauche (selon b,
    /// dos de l'âme d'un UPN, face extérieure de l'aile verticale d'une cornière) (m).
    double centroidFromBottom() const;
    double centroidFromLeft() const;
    /// Produit d'inertie Iyz dans les axes parallèles aux côtés (m⁴) : non nul pour une cornière, dont les axes
    /// principaux sont inclinés. Le calcul utilise Iy et Iz (axes géométriques) : couplage des flexions négligé,
    /// signalé (docs/AUDIT_CALCULS.md).
    double iyz() const;
    /// Contribution des quatre congés âme-aile d'un profil en I laminé (rayon r) : aire et inerties (m², m⁴).
    static void rolledIShapeFillets(double h, double tw, double tf, double r, double* area, double* iy, double* iz);

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
