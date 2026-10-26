#pragma once

#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QVariant>
#include <QWidget>

#include <initializer_list>
#include <map>

namespace TSA::UI
{

/// Valeurs des champs d'un panneau Propriétés au chargement de l'élément. Un panneau ne réécrit dans le modèle que
/// les champs que l'utilisateur a modifiés : une valeur du modèle que le champ ne sait pas représenter (profil du
/// catalogue ou hors liste, matériau absent de la liste, précision au-delà des décimales affichées, valeur hors de
/// la plage du champ) n'est jamais remplacée par celle du champ en modifiant un autre champ (le nom, la couleur…).
class EditedFields
{
public:
    /// Mémorise la valeur affichée de chaque champ (fin du chargement, ou après écriture dans le modèle).
    void capture(std::initializer_list<const QWidget*> widgets)
    {
        m_values.clear();
        for (const auto* w : widgets) m_values[w] = valueOf(w);
    }

    /// Vrai si le champ a changé depuis la capture (un champ non capturé est considéré comme modifié).
    bool changed(const QWidget* w) const
    {
        const auto it = m_values.find(w);
        return it == m_values.end() || it->second != valueOf(w);
    }

    bool anyChanged(std::initializer_list<const QWidget*> widgets) const
    {
        for (const auto* w : widgets)
            if (changed(w)) return true;
        return false;
    }

private:
    static QVariant valueOf(const QWidget* w)
    {
        if (const auto* s = qobject_cast<const QDoubleSpinBox*>(w)) return s->value();
        if (const auto* s = qobject_cast<const QSpinBox*>(w)) return s->value();
        if (const auto* c = qobject_cast<const QComboBox*>(w)) return c->currentData();
        if (const auto* e = qobject_cast<const QLineEdit*>(w)) return e->text();
        if (const auto* b = qobject_cast<const QAbstractButton*>(w)) return b->isChecked();
        return {};
    }

    std::map<const QWidget*, QVariant> m_values;
};

} // namespace TSA::UI
