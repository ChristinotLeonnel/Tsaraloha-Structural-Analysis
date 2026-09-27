#pragma once

#include <QWidget>

namespace TSA::Model { class Model; }

namespace TSA::UI
{

/**
 * @brief Interface fondamentale pour les panneaux de propriétés spécialisés par classe métier (Règle 14).
 * Chaque vue spécialisée est dédiée à un type d'élément (Poutre, Câble, Voile, Dalle, etc.)
 * tout en utilisant l'infrastructure commune du modèle TSA, de TSALib et de la gestion Undo/Redo.
 */
class IElementPropertyView : public QWidget
{
    Q_OBJECT

public:
    explicit IElementPropertyView(QWidget* parent = nullptr) : QWidget(parent) {}
    ~IElementPropertyView() override = default;

    virtual void setModel(TSA::Model::Model* model) = 0;
    virtual void setElementId(int id) = 0;
    virtual int elementId() const = 0;
    virtual void refreshView() = 0;
    virtual void refreshLibraries() = 0;
    virtual void applyChanges() = 0;

signals:
    void elementModified();
};

} // namespace TSA::UI
