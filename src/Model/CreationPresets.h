#pragma once

#include "Material.h"
#include "Section.h"
#include <string>

namespace TSA::Model
{

struct BeamPreset
{
    Section section = Section::rectangular(0.30, 0.50, "R30x50");
    Material material = Material::concreteC25_30();
    double betaAngle = 0.0;
    std::string color = "";
};

struct ColumnPreset
{
    Section section = Section::rectangular(0.30, 0.30, "R30x30");
    Material material = Material::concreteC25_30();
    double betaAngle = 0.0;
    std::string color = "";
};

struct SlabPreset
{
    double thickness = 0.20;
    Material material = Material::concreteC25_30();
    std::string color = "";
};

struct WallPreset
{
    double thickness = 0.20;
    double height = 3.00;
    double offset = 0.0;
    Material material = Material::concreteC25_30();
    std::string color = "";
};

struct StructurePresets
{
    BeamPreset beam;
    ColumnPreset column;
    SlabPreset slab;
    WallPreset wall;
    bool showDialogBeforeDrawing = true;
};

} // namespace TSA::Model
