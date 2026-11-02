#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QLineEdit;

namespace TSA::UI
{

/// Saisie d'un mot de passe, masquée par défaut, avec la case « Afficher le mot de passe ».
class PasswordPromptDialog : public QDialog
{
    Q_OBJECT

public:
    PasswordPromptDialog(const QString& title, const QString& label, QWidget* parent = nullptr);
    QString password() const;

    /// Case qui affiche ou masque le texte de `fields` (plusieurs champs : saisie et confirmation). Décochée : texte masqué.
    static QCheckBox* createVisibilityToggle(const QList<QLineEdit*>& fields, QWidget* parent);

private:
    QLineEdit* m_password = nullptr;
};

} // namespace TSA::UI
