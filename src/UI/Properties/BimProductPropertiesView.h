#pragma once

// Inspection d'un produit physique importé (IFC) : identité, classe d'origine, état vis-à-vis du calcul, étage,
// classification, remarques d'import et jeux de propriétés (Psets, quantités). Lecture seule : les données IFC sont
// conservées telles quelles pour la réexportation (docs/IFC_MAPPING.md).

#include <QWidget>

class QTreeWidget;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

class BimProductPropertiesView : public QWidget
{
    Q_OBJECT
public:
    explicit BimProductPropertiesView(TSA::Model::Model* model, QWidget* parent = nullptr);
    void setModel(TSA::Model::Model* model) { m_model = model; }
    /// Affiche le produit ; false s'il n'existe pas (vue vidée).
    bool setProduct(int productId);

private:
    TSA::Model::Model* m_model = nullptr;
    QTreeWidget* m_tree = nullptr;
};

} // namespace TSA::UI
