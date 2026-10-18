#pragma once

#include "../../BIM/Core/BimTypes.h"

#include <QGroupBox>

#include <optional>

class QLabel;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

/// Identité BIM et quantités de l'élément sélectionné, en lecture seule, sous sa vue de propriétés :
/// produit physique (nom, classe IFC, GlobalId, étage, éléments analytiques regroupés) et métré dérivé
/// du modèle (QuantityTakeoff), de l'élément et du produit entier. Masqué pour un nœud.
class ElementBimPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit ElementBimPanel(QWidget* parent = nullptr);

    void setModel(TSA::Model::Model* model);
    void showElement(const std::optional<TSA::BIM::AnalyticalRef>& ref);
    void refresh();

    /// Texte affiché (tests).
    QString text() const;

private:
    TSA::Model::Model* m_model = nullptr;
    std::optional<TSA::BIM::AnalyticalRef> m_ref;
    QLabel* m_text = nullptr;
};

} // namespace TSA::UI
