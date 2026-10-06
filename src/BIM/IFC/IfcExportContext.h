#pragma once

// État partagé par les mappers pendant un export IFC (un seul export à la fois, sur un modèle constant).

#include "IfcStepWriter.h"

#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::BIM
{
class BimModel;
}

namespace TSA::BIM::Ifc
{

struct IfcExportContext
{
    IfcStepWriter& w;
    const TSA::Model::Model& model;
    const TSA::BIM::BimModel& bim;

    int bodyContext = 0;       ///< sous-contexte 'Body' (MODEL_VIEW)
    int axisContext = 0;       ///< sous-contexte 'Axis' (GRAPH_VIEW)
    int modelContext = 0;      ///< contexte 3D 'Model' (topologie analytique)
    int analysisPlacement = 0; ///< IfcLocalPlacement partagé des objets analytiques

    std::vector<std::string> warnings;
};

} // namespace TSA::BIM::Ifc
