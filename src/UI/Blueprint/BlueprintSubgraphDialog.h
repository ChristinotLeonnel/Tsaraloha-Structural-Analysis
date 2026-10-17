#pragma once

// Propriétés d'un sous-graphe Blueprint (éditeur partagé TSA / TSALab) : nom, catégorie, description, documentation et
// interface (entrées et sorties de données : nom stable, libellé, type, type d'objet, valeur par défaut, entrée
// obligatoire). Le corps n'est pas modifié ici : il s'édite dans l'éditeur (BlueprintEditor::openSubgraph).
// Les noms de broches sont contrôlés à la validation (isValidPinName, uniques par côté) ; une valeur par défaut est
// lue selon le type de la broche (réel en notation scientifique, virgule ou point ; point « x, y, z » ; identifiants
// « 1, 2, 3 ») et refusée si elle est illisible — jamais remplacée en silence.

#include "Blueprint/BlueprintGraph.h"

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

namespace TSA::UI
{

class BlueprintSubgraphDialog : public QDialog
{
    Q_OBJECT

public:
    /// objectTypes : types d'objet connus (complétion du champ « Objet »).
    BlueprintSubgraphDialog(const TSA::Blueprint::SubgraphDefinition& definition, const QStringList& objectTypes, QWidget* parent = nullptr);

    /// Définition modifiée (corps et version inchangés) ; valable après acceptation.
    TSA::Blueprint::SubgraphDefinition definition() const { return m_result; }

    void accept() override;

    /// Lecture d'une valeur par défaut saisie (texte vide : aucune valeur) ; faux si elle est illisible pour ce type.
    static bool parseDefault(const QString& text, const TSA::Blueprint::PinSpec& pin, TSA::Blueprint::Value& out);
    static QString defaultText(const TSA::Blueprint::PinSpec& pin);

private:
    QTableWidget* makeTable(const std::vector<TSA::Blueprint::PinSpec>& pins, bool inputs);
    void addRow(QTableWidget* table, const TSA::Blueprint::PinSpec& pin, bool inputs);
    /// Broches d'une table ; faux (et message) à la première broche invalide.
    bool readPins(QTableWidget* table, bool inputs, std::vector<TSA::Blueprint::PinSpec>& out, QString& error) const;

    TSA::Blueprint::SubgraphDefinition m_original;
    TSA::Blueprint::SubgraphDefinition m_result;
    QStringList m_objectTypes;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_category = nullptr;
    QLineEdit* m_description = nullptr;
    QPlainTextEdit* m_documentation = nullptr;
    QTableWidget* m_inputs = nullptr;
    QTableWidget* m_outputs = nullptr;
};

} // namespace TSA::UI
