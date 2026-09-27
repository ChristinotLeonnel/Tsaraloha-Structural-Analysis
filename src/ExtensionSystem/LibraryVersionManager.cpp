#include "LibraryVersionManager.h"
#include <cmath>

namespace TSA::ExtensionSystem
{

static bool approxEqual(double a, double b)
{
    return std::abs(a - b) <= 1e-6 * (std::abs(a) + std::abs(b) + 1.0);
}

VersionComparisonResult LibraryVersionManager::compare(const MechanicalSnapshot& projectSnapshot,
                                                       const SemanticVersion& projectVersion,
                                                       const MaterialDefinition& libraryDefinition) const
{
    VersionComparisonResult result;
    result.definitionId = libraryDefinition.id;
    result.oldVersion = projectVersion;
    result.newVersion = libraryDefinition.version;

    auto checkProp = [&](const std::string& name, double oldV, double newV, const std::string& unit) {
        if (!approxEqual(oldV, newV))
        {
            PropertyDiff diff;
            diff.propertyName = name;
            diff.oldValue = oldV;
            diff.newValue = newV;
            diff.unit = unit;
            diff.isModified = true;
            result.modifiedProperties.push_back(diff);
        }
        else
        {
            result.unchangedProperties.push_back(name);
        }
    };

    checkProp("Module d'Young (E)", projectSnapshot.youngModulus, libraryDefinition.youngModulus.toBaseSI(), "Pa");
    checkProp("Coefficient de Poisson (nu)", projectSnapshot.poissonRatio, libraryDefinition.poissonRatio, "-");
    checkProp("Masse volumique (rho)", projectSnapshot.density, libraryDefinition.density.toBaseSI(), "kg/m3");

    double newFk = libraryDefinition.fck.toBaseSI() > 0.0 ? libraryDefinition.fck.toBaseSI() : libraryDefinition.ft.toBaseSI();
    checkProp("Résistance caractéristique (fk)", projectSnapshot.characteristicStrength, newFk, "Pa");
    checkProp("Limite d'élasticité (fy)", projectSnapshot.yieldStrength, libraryDefinition.fy.toBaseSI(), "Pa");
    checkProp("Coefficient thermique (alpha)", projectSnapshot.thermalCoeff, libraryDefinition.thermalCoeff.toBaseSI(), "1/K");

    return result;
}

bool LibraryVersionManager::isCompatible(const SemanticVersion& requiredVersion, const SemanticVersion& availableVersion) const
{
    // Selon SemVer : Changement de version majeure = rupture de compatibilité
    if (availableVersion.major != requiredVersion.major)
    {
        return false;
    }
    // La version disponible doit être supérieure ou égale à la version requise
    return !(availableVersion < requiredVersion);
}

} // namespace TSA::ExtensionSystem
