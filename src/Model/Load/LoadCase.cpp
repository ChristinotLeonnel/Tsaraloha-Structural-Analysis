#include "LoadCase.h"

namespace TSA::Model
{

LoadCase::LoadCase(int id, const std::string& name,
                   LoadCaseCategory category,
                   bool includeSelfWeight,
                   double selfWeightFactor,
                   const std::string& description)
    : m_id(id)
    , m_name(name)
    , m_category(category)
    , m_includeSelfWeight(includeSelfWeight)
    , m_selfWeightFactor(selfWeightFactor)
    , m_description(description)
{
}

} // namespace TSA::Model
