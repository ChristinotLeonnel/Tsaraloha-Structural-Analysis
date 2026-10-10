#pragma once

// Interne à TSA3D : correspondances énumérations TSA ↔ chaînes du format, unités, utilitaires JSON.

#include "Tsa3d.h"

#include <QJsonArray>
#include <QJsonValue>

#include <optional>
#include <utility>
#include <vector>

namespace TSA::IO::Tsa3d::detail
{

/// Nom d'une valeur d'énumération : les listes ci-dessous suivent l'ordre des énumérations TSA.
template <typename E>
QString enumName(const QStringList& names, E value)
{
    const int i = static_cast<int>(value);
    return i >= 0 && i < names.size() ? names.at(i) : names.value(0);
}
/// Valeur d'énumération d'un nom (insensible à la casse) ; nullopt si inconnu.
template <typename E>
std::optional<E> enumValue(const QStringList& names, const QString& name)
{
    for (int i = 0; i < names.size(); ++i)
        if (names.at(i).compare(name, Qt::CaseInsensitive) == 0) return static_cast<E>(i);
    return std::nullopt;
}

// Tables (définies dans Tsa3dCommon.cpp) : voir docs/TSA3D.md §Énumérations.
QStringList sectionShapes();
QStringList materialTypes();
QStringList dofStates();
QStringList orientationTypes();
QStringList barRoles();
QStringList eccentricities();
QStringList slabTypes();
QStringList foundationTypes();
QStringList trussRoles();
QStringList cableTypes();
QStringList cableModes();
QStringList loadCaseCategories();
QStringList combinationTypes();
QStringList loadDirections();
QStringList memberLoadKinds();
QStringList coordSystems();
QStringList memberTypes();     ///< beam, column, truss, cable
QStringList surfaceTypes();    ///< slab, wall
QStringList meshCellTypes();   ///< line2, zeroLength, tri3, quad4, tet4, hex8

/// Facteurs vers les unités internes de TSA (m, kN, Pa, kPa, deg). nullopt : unité inconnue.
std::optional<double> lengthFactor(const QString& unit);
std::optional<double> forceFactor(const QString& unit);
std::optional<double> stressFactor(const QString& unit);     ///< vers Pa (matériaux)
std::optional<double> pressureFactor(const QString& unit);   ///< vers kPa (sol, charges surfaciques)
std::optional<double> angleFactor(const QString& unit);      ///< vers degrés

struct Units
{
    double length = 1.0, force = 1.0, stress = 1.0, pressure = 1.0, angle = 1.0;
};
/// Lit le bloc « units » (absent : unités TSA) ; signale les unités inconnues.
Units readUnits(const QJsonObject& doc, Report* report);
QJsonObject tsaUnits();

/// Clés reconnues par objet (le reste est conservé tel quel comme données inconnues).
QStringList knownKeys(const QString& collection);
QStringList knownTopLevelKeys();

QString jsonPath(const QString& collection, int index, const QString& field = QString());

} // namespace TSA::IO::Tsa3d::detail
