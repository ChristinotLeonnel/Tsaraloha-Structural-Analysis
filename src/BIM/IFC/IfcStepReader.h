#pragma once

// Lecture d'un fichier STEP (ISO 10303-21) en entités génériques, indépendante du schéma IFC.
// Les chaînes sont décodées en UTF-8 (\X2\…\X0\, \X\hh, \S\c). Aucune dépendance au rendu.

#include <map>
#include <string>
#include <vector>

namespace TSA::BIM::Ifc
{

struct StepValue
{
    enum class Kind { Null, Derived, Integer, Real, String, Enum, Ref, List, Typed };
    Kind kind = Kind::Null;
    double number = 0.0;            ///< Integer / Real
    std::string text;               ///< String (UTF-8), Enum (sans points), Typed (nom du type)
    int ref = 0;                    ///< Ref
    std::vector<StepValue> list;    ///< List ; Typed : un seul élément (la valeur)

    bool isNull() const { return kind == Kind::Null || kind == Kind::Derived; }
    /// Valeur numérique (Integer, Real ou Typed numérique).
    double asNumber(double fallback = 0.0) const;
    /// Texte (String, Enum ou Typed texte) ; vide sinon.
    std::string asText() const;
    bool asBool(bool fallback = false) const;   ///< .T. / .F. (Enum ou Typed)
    std::vector<int> refs() const;              ///< références d'une liste
};

struct StepEntity
{
    int id = 0;
    std::string type;                 ///< en majuscules (IFCBEAM)
    std::vector<StepValue> args;

    const StepValue& arg(std::size_t i) const;
};

class IfcStepReader
{
public:
    bool parse(const std::string& content, std::string* error = nullptr);
    bool parseFile(const std::string& path, std::string* error = nullptr);

    const std::string& schema() const { return m_schema; }
    const StepEntity* get(int id) const;
    const StepEntity* get(const StepValue& refValue) const { return refValue.kind == StepValue::Kind::Ref ? get(refValue.ref) : nullptr; }
    /// Entités d'un type exact (majuscules : "IFCBEAM").
    std::vector<const StepEntity*> byType(const std::string& type) const;
    std::size_t size() const { return m_entities.size(); }

private:
    std::map<int, StepEntity> m_entities;
    std::map<std::string, std::vector<int>> m_byType;
    std::string m_schema;
};

} // namespace TSA::BIM::Ifc
