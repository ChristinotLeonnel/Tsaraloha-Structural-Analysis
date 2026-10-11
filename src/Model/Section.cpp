#include "Section.h"
#include "../ExtensionSystem/LibraryRegistry.h"
#include "../ExtensionSystem/DefinitionModels.h"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TSA::Model
{

double Section::area() const
{
    switch (shape)
    {
    case SectionShape::Rectangular:
        return width * height;
    case SectionShape::Circular:
        return M_PI * diameter * diameter / 4.0;
    case SectionShape::IShape:
        return 2.0 * (width * tf) + (height - 2.0 * tf) * tw;
    case SectionShape::Pipe:
        return M_PI * (diameter * diameter - (diameter - 2 * tw) * (diameter - 2 * tw)) / 4.0;
    case SectionShape::BoxHollow:
        return (width * height) - ((width - 2 * tw) * (height - 2 * tf));
    case SectionShape::UPN:
        return 2.0 * (width * tf) + (height - 2.0 * tf) * tw;
    case SectionShape::Angle:
        return (height + width - tw) * tw;
    case SectionShape::TSection:
        return width * tf + (height - tf) * tw;
    }
    return width * height;
}

double Section::iy() const
{
    switch (shape)
    {
    case SectionShape::Rectangular:
        return (width * height * height * height) / 12.0;
    case SectionShape::Circular:
        return (M_PI * std::pow(diameter, 4)) / 64.0;
    case SectionShape::IShape:
    case SectionShape::UPN:
    {
        double hInner = height - 2.0 * tf;
        return (width * std::pow(height, 3) - (width - tw) * std::pow(hInner, 3)) / 12.0;
    }
    case SectionShape::Pipe:
        return (M_PI * (std::pow(diameter, 4) - std::pow(diameter - 2 * tw, 4))) / 64.0;
    case SectionShape::BoxHollow:
        return (width * std::pow(height, 3) - (width - 2 * tw) * std::pow(height - 2 * tf, 3)) / 12.0;
    case SectionShape::Angle:
    {
        double t = (tw > 0.0) ? tw : 0.008;
        return (t * std::pow(height, 3) + width * std::pow(t, 3)) / 12.0;
    }
    case SectionShape::TSection:
    {
        double aFlange = width * tf;
        double aWeb = (height - tf) * tw;
        double aTotal = aFlange + aWeb;
        if (aTotal < 1e-6) return (width * std::pow(height, 3)) / 12.0;
        double yFlange = height - tf / 2.0;
        double yWeb = (height - tf) / 2.0;
        double yc = (aFlange * yFlange + aWeb * yWeb) / aTotal;
        double iyFlange = (width * std::pow(tf, 3)) / 12.0 + aFlange * std::pow(yFlange - yc, 2);
        double iyWeb = (tw * std::pow(height - tf, 3)) / 12.0 + aWeb * std::pow(yWeb - yc, 2);
        return iyFlange + iyWeb;
    }
    }
    return (width * height * height * height) / 12.0;
}

double Section::iz() const
{
    switch (shape)
    {
    case SectionShape::Rectangular:
        return (height * width * width * width) / 12.0;
    case SectionShape::Circular:
        return (M_PI * std::pow(diameter, 4)) / 64.0;
    case SectionShape::IShape:
    {
        double iflange = 2.0 * (tf * std::pow(width, 3) / 12.0);
        double iweb = (height - 2.0 * tf) * std::pow(tw, 3) / 12.0;
        return iflange + iweb;
    }
    case SectionShape::UPN:
    {
        double iflange = 2.0 * (tf * std::pow(width, 3) / 12.0);
        double iweb = (height - 2.0 * tf) * std::pow(tw, 3) / 12.0;
        return iflange + iweb;
    }
    case SectionShape::Pipe:
        return (M_PI * (std::pow(diameter, 4) - std::pow(diameter - 2 * tw, 4))) / 64.0;
    case SectionShape::BoxHollow:
        return (height * std::pow(width, 3) - (height - 2 * tf) * std::pow(width - 2 * tw, 3)) / 12.0;
    case SectionShape::Angle:
    {
        double t = (tw > 0.0) ? tw : 0.008;
        return (t * std::pow(width, 3) + height * std::pow(t, 3)) / 12.0;
    }
    case SectionShape::TSection:
    {
        double iflange = (tf * std::pow(width, 3)) / 12.0;
        double iweb = ((height - tf) * std::pow(tw, 3)) / 12.0;
        return iflange + iweb;
    }
    }
    return (height * width * width * width) / 12.0;
}

double Section::rolledIShapeTorsionConstant(double h, double b, double tw, double tf, double r)
{
    // El Darwish & Johnston (1965), formule des tables SCI « Blue Book » / British Steel : ailes et âme
    // rectangulaires + correction des nœuds âme-aile avec congés de rayon r. Validée sur 65 profils IPE et HE
    // (fiches British Steel 21.07.26 : écart ≤ 0,4 %, arrondi d'impression), test 332.
    const double D = ((tf + r) * (tf + r) + tw * (r + 0.25 * tw)) / (2.0 * r + tf);
    const double a1 = -0.042 + 0.2204 * tw / tf + 0.1355 * r / tf - 0.0865 * r * tw / (tf * tf) - 0.0725 * tw * tw / (tf * tf);
    return 2.0 / 3.0 * b * std::pow(tf, 3) + (h - 2.0 * tf) * std::pow(tw, 3) / 3.0 + 2.0 * a1 * std::pow(D, 4) - 0.420 * std::pow(tf, 4);
}

double Section::rootRadius() const
{
    if (shape != SectionShape::IShape) return 0.0;
    // Désignation normalisée → profil de référence (mêmes usines) ; rayon EN 10365 seulement si les dimensions
    // n'ont pas été modifiées.
    auto number = [&](const std::string& prefix) -> int {
        if (name.rfind(prefix, 0) != 0) return 0;
        try { return std::stoi(name.substr(prefix.size())); } catch (...) { return 0; }
    };
    Section ref;
    double r = 0.0;
    if (const int n = number("IPE "); n > 0)
    {
        ref = ipe(n);
        r = n <= 140 ? 7.0 : n <= 180 ? 9.0 : n <= 220 ? 12.0 : n <= 300 ? 15.0 : n <= 360 ? 18.0 : 21.0;
    }
    else if (const int a = number("HEA "), bb = number("HEB "); a > 0 || bb > 0)
    {
        const int m = a > 0 ? a : bb;
        ref = a > 0 ? hea(m) : heb(m);
        r = m <= 140 ? 12.0 : m <= 180 ? 15.0 : m <= 220 ? 18.0 : m <= 240 ? 21.0 : m <= 280 ? 24.0 : 27.0;
    }
    else
    {
        return 0.0;
    }
    // Numéros décrits par les usines (au-delà, elles renvoient un profil par défaut sous la même désignation).
    static const int kIpe[] = { 100, 120, 140, 160, 180, 200, 220, 240, 270, 300, 330, 360, 400 };
    static const int kHe[] = { 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300 };
    const int n = number("IPE ") > 0 ? number("IPE ") : number("HEA ") > 0 ? number("HEA ") : number("HEB ");
    const bool isIpe = number("IPE ") > 0;
    if (isIpe ? std::find(std::begin(kIpe), std::end(kIpe), n) == std::end(kIpe) : std::find(std::begin(kHe), std::end(kHe), n) == std::end(kHe))
        return 0.0;
    auto same = [](double x, double y) { return std::abs(x - y) <= 1e-7; };
    if (!same(ref.height, height) || !same(ref.width, width) || !same(ref.tw, tw) || !same(ref.tf, tf)) return 0.0;
    return r * 1e-3;
}

double Section::iw() const
{
    switch (shape)
    {
    case SectionShape::IShape:
        // Ailes seules, bisymétrique (congés et âme négligés) : écart ≤ 1,2 % aux fiches British Steel.
        return tf * std::pow(width, 3) * std::pow(height - tf, 2) / 24.0;
    case SectionShape::Rectangular:
    case SectionShape::Circular:
    case SectionShape::Pipe:
    case SectionShape::BoxHollow:
        return 0.0; // sections pleines et fermées : gauchissement négligeable devant la torsion de Saint-Venant
    default:
        return std::nan(""); // UPN, cornière, T : non calculé
    }
}

double Section::it() const
{
    switch (shape)
    {
    case SectionShape::Circular:
        return (M_PI * std::pow(diameter, 4)) / 32.0;
    case SectionShape::IShape:
    {
        // Profil laminé normalisé : congés pris en compte ; sinon parois minces (sous-estime J de 12 à 29 %
        // pour un profil laminé, exact pour un profil reconstitué soudé sans congé).
        const double r = rootRadius();
        if (r > 0.0) return rolledIShapeTorsionConstant(height, width, tw, tf, r);
        return (2.0 * width * std::pow(tf, 3) + (height - tf) * std::pow(tw, 3)) / 3.0;
    }
    case SectionShape::UPN:
        // Parois minces, épaisseur d'aile moyenne (ailes inclinées et congés non modélisés, non comparé à un
        // catalogue : aucune référence vérifiée dans ce travail).
        return (2.0 * width * std::pow(tf, 3) + (height - tf) * std::pow(tw, 3)) / 3.0;
    case SectionShape::TSection:
        return (width * std::pow(tf, 3) + (height - tf) * std::pow(tw, 3)) / 3.0;
    case SectionShape::Angle:
        return (height + width - tw) * std::pow(tw, 3) / 3.0;
    case SectionShape::Rectangular:
    {
        double b = std::min(width, height);
        double a = std::max(width, height);
        return a * std::pow(b, 3) * (1.0 / 3.0 - 0.21 * (b / a) * (1.0 - std::pow(b / a, 4) / 12.0));
    }
    case SectionShape::Pipe:
    {
        // Tube rond : J de Saint-Venant = moment polaire (exact pour une couronne circulaire).
        const double d = std::max(0.0, diameter - 2.0 * tw);
        return M_PI * (std::pow(diameter, 4) - std::pow(d, 4)) / 32.0;
    }
    case SectionShape::BoxHollow:
    {
        // Caisson (profil creux fermé) : J ≠ moment polaire Iy + Iz (≈ 34 % de trop pour un carré).
        // Bredt sur la ligne moyenne, Am = (b − tw)(h − tf), ∮ds/t = 2(h − tf)/tw + 2(b − tw)/tf, plus
        // la contribution des parois minces Σ s·t³/3 (EN 10219-2, sans arrondis d'angle).
        const double t1 = tw, t2 = (tf > 0.0) ? tf : tw;
        const double bm = width - t1, hm = height - t2;
        if (bm <= 0.0 || hm <= 0.0 || t1 <= 0.0) return iy() + iz();
        const double am = bm * hm;
        const double perimeterOverT = 2.0 * hm / t1 + 2.0 * bm / t2;
        return 4.0 * am * am / perimeterOverT + (2.0 * hm * std::pow(t1, 3) + 2.0 * bm * std::pow(t2, 3)) / 3.0;
    }
    }
    return iy() + iz();
}

double Section::wy() const
{
    double h2 = std::max(1e-4, height / 2.0);
    return iy() / h2;
}

double Section::wz() const
{
    // `width` est toujours correctement renseigné (y compris pour Circular/Pipe, où les
    // usines recopient le diamètre dans `width`) : pas besoin de retester `diameter`, qui
    // conserve sa valeur par défaut (0.30) pour toutes les formes non circulaires et fausserait
    // le résultat.
    double w2 = std::max(1e-4, width / 2.0);
    return iz() / w2;
}

Section Section::rectangular(double b, double h, const std::string& name)
{
    Section s;
    s.shape = SectionShape::Rectangular;
    s.width = b;
    s.height = h;
    s.name = name.empty() ? ("Rect " + std::to_string(static_cast<int>(b * 1000)) + "x" + std::to_string(static_cast<int>(h * 1000))) : name;
    return s;
}

Section Section::circular(double d, const std::string& name)
{
    Section s;
    s.shape = SectionShape::Circular;
    s.diameter = d;
    s.width = d;
    s.height = d;
    s.name = name.empty() ? ("Circ D" + std::to_string(static_cast<int>(d * 1000))) : name;
    return s;
}

Section Section::ipe(int number)
{
    Section s;
    s.shape = SectionShape::IShape;
    s.name = "IPE " + std::to_string(number);
    switch (number)
    {
    case 100: s.height = 0.100; s.width = 0.055; s.tw = 0.0041; s.tf = 0.0057; break;
    case 120: s.height = 0.120; s.width = 0.064; s.tw = 0.0044; s.tf = 0.0063; break;
    case 140: s.height = 0.140; s.width = 0.073; s.tw = 0.0047; s.tf = 0.0069; break;
    case 160: s.height = 0.160; s.width = 0.082; s.tw = 0.0050; s.tf = 0.0074; break;
    case 180: s.height = 0.180; s.width = 0.091; s.tw = 0.0053; s.tf = 0.0080; break;
    case 200: s.height = 0.200; s.width = 0.100; s.tw = 0.0056; s.tf = 0.0085; break;
    case 220: s.height = 0.220; s.width = 0.110; s.tw = 0.0059; s.tf = 0.0092; break;
    case 240: s.height = 0.240; s.width = 0.120; s.tw = 0.0062; s.tf = 0.0098; break;
    case 270: s.height = 0.270; s.width = 0.135; s.tw = 0.0066; s.tf = 0.0102; break;
    case 300: s.height = 0.300; s.width = 0.150; s.tw = 0.0071; s.tf = 0.0107; break;
    case 330: s.height = 0.330; s.width = 0.160; s.tw = 0.0075; s.tf = 0.0115; break;
    case 360: s.height = 0.360; s.width = 0.170; s.tw = 0.0080; s.tf = 0.0127; break;
    case 400: s.height = 0.400; s.width = 0.180; s.tw = 0.0086; s.tf = 0.0135; break;
    default:  s.height = 0.100; s.width = 0.055; s.tw = 0.0041; s.tf = 0.0057; break;
    }
    return s;
}

Section Section::hea(int number)
{
    Section s;
    s.shape = SectionShape::IShape;
    s.name = "HEA " + std::to_string(number);
    switch (number)
    {
    case 100: s.height = 0.096; s.width = 0.100; s.tw = 0.0050; s.tf = 0.0080; break;
    case 120: s.height = 0.114; s.width = 0.120; s.tw = 0.0050; s.tf = 0.0080; break;
    case 140: s.height = 0.133; s.width = 0.140; s.tw = 0.0055; s.tf = 0.0085; break;
    case 160: s.height = 0.152; s.width = 0.160; s.tw = 0.0060; s.tf = 0.0090; break;
    case 180: s.height = 0.171; s.width = 0.180; s.tw = 0.0060; s.tf = 0.0095; break;
    case 200: s.height = 0.190; s.width = 0.200; s.tw = 0.0065; s.tf = 0.0100; break;
    case 220: s.height = 0.210; s.width = 0.220; s.tw = 0.0070; s.tf = 0.0110; break;
    case 240: s.height = 0.230; s.width = 0.240; s.tw = 0.0075; s.tf = 0.0120; break;
    case 260: s.height = 0.250; s.width = 0.260; s.tw = 0.0075; s.tf = 0.0125; break;
    case 280: s.height = 0.270; s.width = 0.280; s.tw = 0.0080; s.tf = 0.0130; break;
    case 300: s.height = 0.290; s.width = 0.300; s.tw = 0.0085; s.tf = 0.0140; break;
    default:  s.height = 0.190; s.width = 0.200; s.tw = 0.0065; s.tf = 0.0100; break;
    }
    return s;
}

Section Section::heb(int number)
{
    Section s;
    s.shape = SectionShape::IShape;
    s.name = "HEB " + std::to_string(number);
    switch (number)
    {
    case 100: s.height = 0.100; s.width = 0.100; s.tw = 0.0060; s.tf = 0.0100; break;
    case 120: s.height = 0.120; s.width = 0.120; s.tw = 0.0065; s.tf = 0.0110; break;
    case 140: s.height = 0.140; s.width = 0.140; s.tw = 0.0070; s.tf = 0.0120; break;
    case 160: s.height = 0.160; s.width = 0.160; s.tw = 0.0080; s.tf = 0.0130; break;
    case 180: s.height = 0.180; s.width = 0.180; s.tw = 0.0085; s.tf = 0.0140; break;
    case 200: s.height = 0.200; s.width = 0.200; s.tw = 0.0090; s.tf = 0.0150; break;
    case 220: s.height = 0.220; s.width = 0.220; s.tw = 0.0095; s.tf = 0.0160; break;
    case 240: s.height = 0.240; s.width = 0.240; s.tw = 0.0100; s.tf = 0.0170; break;
    case 260: s.height = 0.260; s.width = 0.260; s.tw = 0.0100; s.tf = 0.0175; break;
    case 280: s.height = 0.280; s.width = 0.280; s.tw = 0.0105; s.tf = 0.0180; break;
    case 300: s.height = 0.300; s.width = 0.300; s.tw = 0.0110; s.tf = 0.0190; break;
    default:  s.height = 0.200; s.width = 0.200; s.tw = 0.0090; s.tf = 0.0150; break;
    }
    return s;
}

Section Section::upn(int number)
{
    Section s;
    s.shape = SectionShape::UPN;
    s.name = "UPN " + std::to_string(number);
    switch (number)
    {
    case 80:  s.height = 0.080; s.width = 0.045; s.tw = 0.0060; s.tf = 0.0080; break;
    case 100: s.height = 0.100; s.width = 0.050; s.tw = 0.0060; s.tf = 0.0085; break;
    case 120: s.height = 0.120; s.width = 0.055; s.tw = 0.0070; s.tf = 0.0090; break;
    case 140: s.height = 0.140; s.width = 0.060; s.tw = 0.0070; s.tf = 0.0100; break;
    case 160: s.height = 0.160; s.width = 0.065; s.tw = 0.0075; s.tf = 0.0105; break;
    case 180: s.height = 0.180; s.width = 0.070; s.tw = 0.0080; s.tf = 0.0110; break;
    case 200: s.height = 0.200; s.width = 0.075; s.tw = 0.0085; s.tf = 0.0115; break;
    case 220: s.height = 0.220; s.width = 0.080; s.tw = 0.0090; s.tf = 0.0125; break;
    case 240: s.height = 0.240; s.width = 0.085; s.tw = 0.0095; s.tf = 0.0130; break;
    case 260: s.height = 0.260; s.width = 0.090; s.tw = 0.0100; s.tf = 0.0140; break;
    case 280: s.height = 0.280; s.width = 0.095; s.tw = 0.0100; s.tf = 0.0150; break;
    case 300: s.height = 0.300; s.width = 0.100; s.tw = 0.0100; s.tf = 0.0160; break;
    default:  s.height = 0.100; s.width = 0.050; s.tw = 0.0060; s.tf = 0.0085; break;
    }
    return s;
}

Section Section::angle(double h, double b, double t, const std::string& name)
{
    Section s;
    s.shape = SectionShape::Angle;
    s.height = h;
    s.width = b;
    s.tw = t;
    s.tf = t;
    s.name = name.empty() ? ("L " + std::to_string(static_cast<int>(h * 1000)) + "x" +
                             std::to_string(static_cast<int>(b * 1000)) + "x" +
                             std::to_string(static_cast<int>(t * 1000))) : name;
    return s;
}

Section Section::boxHollow(double b, double h, double tw, double tf, const std::string& name)
{
    Section s;
    s.shape = SectionShape::BoxHollow;
    s.width = b;
    s.height = h;
    s.tw = tw;
    s.tf = (tf > 0.0) ? tf : tw;
    s.name = name.empty() ? ("Tube " + std::to_string(static_cast<int>(b * 1000)) + "x" +
                             std::to_string(static_cast<int>(h * 1000)) + "x" +
                             std::to_string(static_cast<int>(tw * 1000))) : name;
    return s;
}

Section Section::pipe(double diameter, double thickness, const std::string& name)
{
    Section s;
    s.shape = SectionShape::Pipe;
    s.diameter = diameter;
    s.width = diameter;
    s.height = diameter;
    s.tw = thickness;
    s.name = name.empty() ? ("Tube D" + std::to_string(static_cast<int>(diameter * 1000)) + "x" +
                             std::to_string(static_cast<int>(thickness * 1000))) : name;
    return s;
}

Section Section::tSection(double h, double b, double tw, double tf, const std::string& name)
{
    Section s;
    s.shape = SectionShape::TSection;
    s.height = h;
    s.width = b;
    s.tw = tw;
    s.tf = tf;
    s.name = name.empty() ? ("T " + std::to_string(static_cast<int>(b * 1000)) + "x" +
                             std::to_string(static_cast<int>(h * 1000)) + "x" +
                             std::to_string(static_cast<int>(tw * 1000))) : name;
    return s;
}

std::vector<Section> Section::defaultLibrary()
{
    const auto& extSections = TSA::ExtensionSystem::LibraryRegistry::instance().allSections();
    if (!extSections.empty())
    {
        std::vector<Section> result;
        result.reserve(extSections.size());
        int id = 1;
        for (const auto& secDef : extSections)
        {
            result.push_back(secDef.toModelSection(id++));
        }
        std::sort(result.begin(), result.end(), [](const Section& a, const Section& b) {
            return a.name < b.name;
        });
        return result;
    }

    return {
        // Profils IPE
        Section::ipe(100),
        Section::ipe(120),
        Section::ipe(140),
        Section::ipe(160),
        Section::ipe(180),
        Section::ipe(200),
        Section::ipe(240),
        Section::ipe(300),
        // Profils HEA
        Section::hea(100),
        Section::hea(120),
        Section::hea(140),
        Section::hea(160),
        Section::hea(200),
        Section::hea(240),
        // Profils HEB
        Section::heb(100),
        Section::heb(120),
        Section::heb(160),
        Section::heb(200),
        Section::heb(240),
        // Profils UPN
        Section::upn(100),
        Section::upn(120),
        Section::upn(140),
        Section::upn(160),
        Section::upn(200),
        // Cornières
        Section::angle(0.050, 0.050, 0.005),
        Section::angle(0.060, 0.060, 0.006),
        Section::angle(0.080, 0.080, 0.008),
        Section::angle(0.100, 0.100, 0.010),
        // Profils en T
        Section::tSection(0.100, 0.100, 0.008, 0.010, "T 100x100x8"),
        Section::tSection(0.120, 0.120, 0.009, 0.011, "T 120x120x9"),
        Section::tSection(0.140, 0.140, 0.010, 0.012, "T 140x140x10"),
        // Tubes rectangulaires / carrés
        Section::boxHollow(0.100, 0.100, 0.005),
        Section::boxHollow(0.150, 0.150, 0.006),
        Section::boxHollow(0.200, 0.100, 0.006),
        // Tubes ronds
        Section::pipe(0.114, 0.005),
        Section::pipe(0.168, 0.006),
        // Rectangulaires béton / bois
        Section::rectangular(0.30, 0.50, "Rect 300x500"),
        Section::rectangular(0.40, 0.50, "Rect 400x500"),
        Section::rectangular(0.30, 0.30, "Rect 300x300"),
        Section::rectangular(0.40, 0.40, "Rect 400x400"),
        Section::rectangular(0.20, 0.40, "Rect 200x400"),
        // Circulaires
        Section::circular(0.20, "Circ D200"),
        Section::circular(0.30, "Circ D300"),
        Section::circular(0.40, "Circ D400"),
        Section::circular(0.50, "Circ D500"),
        Section::circular(0.60, "Circ D60 (D600)"),
        Section::circular(0.80, "Circ D800")
    };
}

} // namespace TSA::Model
