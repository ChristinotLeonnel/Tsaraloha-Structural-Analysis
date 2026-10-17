#pragma once

#include "Foundation.h"
#include "Material.h"
#include "Section.h"
#include "TrussMember.h"
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

/// Semelle posée par le mode Dessin Fondation (dimensions affichées dans l'invite avant le clic).
struct FoundationPreset
{
    FoundationType type = FoundationType::IsolatedFooting;
    double widthA = 1.50;
    double lengthB = 1.50;
    double heightH = 0.50;
    Material material = Material::concreteC25_30();
    std::string color = "";
};

/// Barre de treillis posée par le mode Dessin Treillis.
struct TrussPreset
{
    Section section = Section::circular(0.10, "C100");
    Material material = Material::steelS235();
    TrussMemberRole role = TrussMemberRole::Diagonal;
    std::string color = "";
};

struct StructurePresets
{
    BeamPreset beam;
    ColumnPreset column;
    SlabPreset slab;
    WallPreset wall;
    FoundationPreset foundation;
    TrussPreset truss;
    bool showDialogBeforeDrawing = true;
};

} // namespace TSA::Model
