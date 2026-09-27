#include "LibraryDependencyManager.h"
#include <set>
#include <queue>

namespace TSA::ExtensionSystem
{

void LibraryDependencyManager::registerManifest(const ExtensionManifest& manifest)
{
    m_manifests[manifest.id] = manifest;
}

void LibraryDependencyManager::unregisterManifest(const std::string& extensionId)
{
    m_manifests.erase(extensionId);
}

void LibraryDependencyManager::clear()
{
    m_manifests.clear();
}

ValidationResult LibraryDependencyManager::validateDependencies() const
{
    ValidationResult res;

    for (const auto& [id, manifest] : m_manifests)
    {
        for (const auto& dep : manifest.dependencies)
        {
            auto it = m_manifests.find(dep.id);
            if (it == m_manifests.end())
            {
                if (!dep.optional)
                {
                    res.addError("Dépendance manquante pour '" + id + "' : '" + dep.id + "' (>= " + dep.minimumVersion.toString() + ")");
                }
                else
                {
                    res.addWarning("Dépendance optionnelle non installée pour '" + id + "' : '" + dep.id + "'");
                }
            }
            else
            {
                if (it->second.version < dep.minimumVersion)
                {
                    res.addError("Version insuffisante pour '" + dep.id + "' requise par '" + id +
                                 "'. Disponible : " + it->second.version.toString() +
                                 ", Requise : >= " + dep.minimumVersion.toString());
                }
            }
        }
    }

    return res;
}

std::vector<std::string> LibraryDependencyManager::computeLoadOrder(ValidationResult* outResult) const
{
    std::map<std::string, int> inDegree;
    std::map<std::string, std::vector<std::string>> adj;

    for (const auto& [id, _] : m_manifests)
    {
        inDegree[id] = 0;
        adj[id] = {};
    }

    for (const auto& [id, manifest] : m_manifests)
    {
        for (const auto& dep : manifest.dependencies)
        {
            if (m_manifests.find(dep.id) != m_manifests.end())
            {
                // dep.id doit être chargé avant id
                adj[dep.id].push_back(id);
                inDegree[id]++;
            }
        }
    }

    std::queue<std::string> q;
    for (const auto& [id, deg] : inDegree)
    {
        if (deg == 0) q.push(id);
    }

    std::vector<std::string> order;
    while (!q.empty())
    {
        std::string u = q.front();
        q.pop();
        order.push_back(u);

        for (const auto& v : adj[u])
        {
            inDegree[v]--;
            if (inDegree[v] == 0)
            {
                q.push(v);
            }
        }
    }

    if (order.size() != m_manifests.size())
    {
        if (outResult)
        {
            outResult->addError("Cycle de dépendances détecté dans les extensions installées.");
        }
    }

    return order;
}

bool LibraryDependencyManager::areDependenciesSatisfied(const std::string& extensionId, std::vector<std::string>* missingDeps) const
{
    auto it = m_manifests.find(extensionId);
    if (it == m_manifests.end()) return false;

    bool satisfied = true;
    for (const auto& dep : it->second.dependencies)
    {
        if (dep.optional) continue;
        auto depIt = m_manifests.find(dep.id);
        if (depIt == m_manifests.end() || depIt->second.version < dep.minimumVersion)
        {
            satisfied = false;
            if (missingDeps)
            {
                missingDeps->push_back(dep.id);
            }
        }
    }
    return satisfied;
}

} // namespace TSA::ExtensionSystem
