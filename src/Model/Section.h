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
    BoxHollow
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
    double iy() const; // Inertie flexion selon axe fort
    double iz() const; // Inertie flexion selon axe faible
    double it() const; // Inertie torsionnelle de Saint-Venant

    // Usines prédéfinies
    static Section rectangular(double b, double h, const std::string& name = "");
    static Section circular(double d, const std::string& name = "");
    static Section ipe(int number); // IPE 160, 200, 240, 270, 300, 330, 360, 400
    static Section hea(int number); // HEA 160, 200, 240, 300
    static Section heb(int number); // HEB 160, 200, 240, 300
};

} // namespace TSA::Model
