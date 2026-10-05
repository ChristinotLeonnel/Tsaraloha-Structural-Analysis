#pragma once

// Copie des attributs d'un élément vers un élément nouvellement créé (géométrie exclue).
// Usage interne au modèle (copie, rotation-copie, symétrie, division) : une seule liste
// d'attributs, pour qu'aucune opération n'en oublie (section, rôle, relâchements…).
// L'appelant notifie ensuite l'élément (notify*Modified) : les vues ont été créées à l'ajout
// avec les valeurs par défaut.

#include "Beam.h"
#include "Cable/Cable.h"
#include "Column.h"
#include "Slab.h"
#include "TrussMember.h"

namespace TSA::Model
{

inline void copyBeamAttributes(const Beam& src, Beam& dst)
{
    dst.setRole(src.role());
    dst.setSection(src.section());
    dst.setMaterial(src.material());
    dst.setRotation(src.rotation());
    dst.setEccentricity(src.eccentricity());
    dst.setStartRelease(src.startRelease());
    dst.setEndRelease(src.endRelease());
    dst.setColor(src.color());
}

inline void copyColumnAttributes(const Column& src, Column& dst)
{
    dst.setSection(src.section());
    dst.setMaterial(src.material());
    dst.setRotation(src.rotation());
    dst.setColor(src.color());
}

inline void copyCableAttributes(const Cable& src, Cable& dst)
{
    dst.setType(src.type());
    dst.setSection(src.section());
    dst.setMaterial(src.material());
    dst.setPrestress(src.prestress());
    dst.setAnalysisProperties(src.analysisProperties());
    dst.setStartAnchor(src.startAnchor());
    dst.setEndAnchor(src.endAnchor());
    dst.setColor(src.color());
}

inline void copyTrussAttributes(const TrussMember& src, TrussMember& dst)
{
    dst.setRole(src.role());
    dst.setSection(src.section());
    dst.setMaterial(src.material());
    dst.setColor(src.color());
}

inline void copySlabAttributes(const Slab& src, Slab& dst)
{
    dst.setThickness(src.thickness());
    dst.setMaterial(src.material());
    dst.setSlabType(src.slabType());
    dst.setColor(src.color());
}

} // namespace TSA::Model
