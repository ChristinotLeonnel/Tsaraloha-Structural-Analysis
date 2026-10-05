#pragma once

// Saisie par fenêtre d'un outil de modification / dessin : formulaire généré à partir des
// paramètres déclarés par l'outil (aucune fenêtre propre à un outil).

#include "../../Interaction/Tools/ModelingTool.h"

#include <QDialog>

#include <functional>
#include <vector>

class QFormLayout;

namespace TSA::UI
{

class ModelingToolDialog : public QDialog
{
    Q_OBJECT

public:
    ModelingToolDialog(TSA::Interaction::ModelingTool& tool, QWidget* parent = nullptr);

    /// Recopie les valeurs saisies dans les paramètres de l'outil.
    void commit();
    /// Nombre de champs générés (tests).
    int fieldCount() const { return static_cast<int>(m_writers.size()); }

private:
    TSA::Interaction::ModelingTool& m_tool;
    std::vector<std::function<void()>> m_writers;
};

} // namespace TSA::UI
