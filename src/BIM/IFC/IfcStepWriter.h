#pragma once

// Écriture d'un fichier d'échange STEP (ISO 10303-21) pour IFC. Indépendant du schéma : les
// entités sont fournies par les mappers (IfcMapper, IfcGeometryMapper…). Aucune dépendance
// au rendu (OCCT / OpenGL).

#include <string>
#include <string_view>
#include <vector>

namespace TSA::BIM::Ifc
{

struct StepHeader
{
    std::string fileName;
    std::string author;
    std::string organization;
    std::string schema = "IFC4X3_ADD2";
    std::string viewDefinition = "ReferenceView";
    std::string timeStamp;   ///< ISO 8601 ; vide = maintenant
};

class IfcStepWriter
{
public:
    /// Ajoute « #n=ENTITY(args); » et retourne n. `entity` en majuscules (IFCBEAM).
    int add(std::string_view entity, const std::string& args);
    std::size_t count() const { return m_lines.size(); }

    /// Document complet (en-tête + données).
    std::string document(const StepHeader& header) const;

    // Encodage des valeurs
    static std::string str(const std::string& utf8);      ///< 'texte' (échappement STEP, \X2\ pour l'Unicode)
    static std::string optStr(const std::string& utf8);   ///< $ si vide
    static std::string real(double v);                    ///< 1.5, 0., 1.E-05
    static std::string ref(int id) { return "#" + std::to_string(id); }
    static std::string refs(const std::vector<int>& ids);  ///< (#1,#2)
    static std::string reals(const std::vector<double>& v); ///< (0.,1.,2.)
    static std::string boolean(bool b) { return b ? ".T." : ".F."; }
    static std::string enumeration(std::string_view v) { return "." + std::string(v) + "."; }

private:
    std::vector<std::string> m_lines;
};

} // namespace TSA::BIM::Ifc
