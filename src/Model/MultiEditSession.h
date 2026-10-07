#pragma once

// Édition groupée dans le panneau de propriétés (BUG-005).
// Le panneau affiche l'élément PRINCIPAL de la sélection ; chaque modification de celui-ci est
// reportée sur les autres éléments sélectionnés du même type, champ par champ : seuls les champs
// réellement modifiés sont reportés (jamais le nom, les nœuds ni les coordonnées). L'entrée
// Annuler poussée par la vue avant la modification couvre aussi les éléments reportés (instantané
// complet du modèle).

#include "Beam.h"
#include "Column.h"
#include "Foundation.h"
#include "ModelDiff.h"
#include "Node.h"
#include "SelectionQuery.h"
#include "Slab.h"
#include "TrussMember.h"
#include "Wall.h"

#include <set>
#include <variant>

namespace TSA::Model
{

class Model;

class MultiEditSession
{
public:
    /// Types pris en charge : nœud (appui), poutre, poteau, treillis, dalle, voile, fondation.
    static bool supports(ElementKind kind) noexcept;

    /// Démarre une édition groupée : `others` = autres éléments du même type (le principal en est
    /// retiré). Sans autre élément ou type non pris en charge, la session reste inactive.
    void begin(const Model& model, ElementKind kind, int primaryId, std::set<int> others);
    void clear() noexcept;

    bool active() const noexcept { return m_kind.has_value() && !m_others.empty(); }
    ElementKind kind() const noexcept { return m_kind.value_or(ElementKind::Node); }
    int primaryId() const noexcept { return m_primaryId; }
    size_t count() const noexcept { return active() ? m_others.size() + 1 : 0; }

    /// À appeler après une modification de l'élément principal : reporte les champs qui ont
    /// changé depuis le dernier appel sur les autres éléments, sans notifier. Retourne le
    /// différentiel des éléments reportés (à notifier par l'appelant).
    ModelDiff propagate(Model& model);
    /// Relit l'état de l'élément principal (après Annuler / Rétablir ou une modification externe).
    void resync(const Model& model) { capture(model); }

private:
    void capture(const Model& model);

    std::optional<ElementKind> m_kind;
    int m_primaryId = 0;
    std::set<int> m_others;
    std::variant<std::monostate, Node, Beam, Column, TrussMember, Slab, Wall, Foundation> m_before;
};

} // namespace TSA::Model
