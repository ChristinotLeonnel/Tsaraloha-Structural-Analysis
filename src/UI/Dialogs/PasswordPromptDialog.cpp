#include "PasswordPromptDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace TSA::UI
{

PasswordPromptDialog::PasswordPromptDialog(const QString& title, const QString& label, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setMinimumWidth(380);
    auto* layout = new QVBoxLayout(this);
    auto* text = new QLabel(label, this);
    text->setWordWrap(true);
    layout->addWidget(text);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_password);
    layout->addWidget(createVisibilityToggle({ m_password }, this));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_password->setFocus();
}

QString PasswordPromptDialog::password() const
{
    return m_password->text();
}

QCheckBox* PasswordPromptDialog::createVisibilityToggle(const QList<QLineEdit*>& fields, QWidget* parent)
{
    auto* toggle = new QCheckBox(tr("Afficher le mot de passe"), parent);
    toggle->setToolTip(tr("Montre le texte saisi : à éviter si quelqu'un peut voir l'écran"));
    connect(toggle, &QCheckBox::toggled, parent, [fields](bool visible) {
        for (QLineEdit* field : fields) field->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
    });
    return toggle;
}

} // namespace TSA::UI
