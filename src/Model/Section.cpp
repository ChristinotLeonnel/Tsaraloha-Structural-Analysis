#include "Section.h"
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
    {
        double hInner = height - 2.0 * tf;
        return (width * std::pow(height, 3) - (width - tw) * std::pow(hInner, 3)) / 12.0;
    }
    case SectionShape::Pipe:
        return (M_PI * (std::pow(diameter, 4) - std::pow(diameter - 2 * tw, 4))) / 64.0;
    case SectionShape::BoxHollow:
        return (width * std::pow(height, 3) - (width - 2 * tw) * std::pow(height - 2 * tf, 3)) / 12.0;
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
    case SectionShape::Pipe:
        return (M_PI * (std::pow(diameter, 4) - std::pow(diameter - 2 * tw, 4))) / 64.0;
    case SectionShape::BoxHollow:
        return (height * std::pow(width, 3) - (height - 2 * tf) * std::pow(width - 2 * tw, 3)) / 12.0;
    }
    return (height * width * width * width) / 12.0;
}

double Section::it() const
{
    // Approximation torsionnelle de Saint-Venant
    switch (shape)
    {
    case SectionShape::Circular:
        return (M_PI * std::pow(diameter, 4)) / 32.0;
    case SectionShape::IShape:
        return (2.0 * width * std::pow(tf, 3) + (height - tf) * std::pow(tw, 3)) / 3.0;
    case SectionShape::Rectangular:
    {
        double b = std::min(width, height);
        double a = std::max(width, height);
        return a * std::pow(b, 3) * (1.0 / 3.0 - 0.21 * (b / a) * (1.0 - std::pow(b / a, 4) / 12.0));
    }
    default:
        return iy() + iz();
    }
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
    case 160: s.height = 0.160; s.width = 0.082; s.tw = 0.0050; s.tf = 0.0074; break;
    case 200: s.height = 0.200; s.width = 0.100; s.tw = 0.0056; s.tf = 0.0085; break;
    case 240: s.height = 0.240; s.width = 0.120; s.tw = 0.0062; s.tf = 0.0098; break;
    case 270: s.height = 0.270; s.width = 0.135; s.tw = 0.0066; s.tf = 0.0102; break;
    case 300: s.height = 0.300; s.width = 0.150; s.tw = 0.0071; s.tf = 0.0107; break;
    case 330: s.height = 0.330; s.width = 0.160; s.tw = 0.0075; s.tf = 0.0115; break;
    case 360: s.height = 0.360; s.width = 0.170; s.tw = 0.0080; s.tf = 0.0127; break;
    case 400: s.height = 0.400; s.width = 0.180; s.tw = 0.0086; s.tf = 0.0135; break;
    default:  s.height = 0.200; s.width = 0.100; s.tw = 0.0056; s.tf = 0.0085; break;
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
    case 160: s.height = 0.152; s.width = 0.160; s.tw = 0.0060; s.tf = 0.0090; break;
    case 200: s.height = 0.190; s.width = 0.200; s.tw = 0.0065; s.tf = 0.0100; break;
    case 240: s.height = 0.230; s.width = 0.240; s.tw = 0.0075; s.tf = 0.0120; break;
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
    case 160: s.height = 0.160; s.width = 0.160; s.tw = 0.0080; s.tf = 0.0130; break;
    case 200: s.height = 0.200; s.width = 0.200; s.tw = 0.0090; s.tf = 0.0150; break;
    case 240: s.height = 0.240; s.width = 0.240; s.tw = 0.0100; s.tf = 0.0170; break;
    case 300: s.height = 0.300; s.width = 0.300; s.tw = 0.0110; s.tf = 0.0190; break;
    default:  s.height = 0.200; s.width = 0.200; s.tw = 0.0090; s.tf = 0.0150; break;
    }
    return s;
}

} // namespace TSA::Model
