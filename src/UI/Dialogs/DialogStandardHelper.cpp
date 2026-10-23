#include "DialogStandardHelper.h"
#include "../Widgets/StandardDialogHeader.h"
#include "../HelpLauncher.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QKeyEvent>

namespace TSA::UI
{

void applyStandardDialogSizing(QDialog* dialog, DialogSizeCategory category)
{
    if (!dialog) return;

    switch (category)
    {
    case DialogSizeCategory::Compact:
        dialog->setMinimumSize(420, 300);
        dialog->resize(460, 360);
        break;
    case DialogSizeCategory::Standard:
        dialog->setMinimumSize(520, 480);
        dialog->resize(560, 600);
        break;
    case DialogSizeCategory::Large:
        dialog->setMinimumSize(740, 540);
        dialog->resize(860, 640);
        break;
    case DialogSizeCategory::Studio:
        dialog->setMinimumSize(960, 620);
        dialog->resize(1080, 700);
        break;
    }
}

StandardDialogHeader* addStandardDialogHeader(QVBoxLayout* mainLayout,
                                              const QString& title,
                                              const QString& subtitle,
                                              const QString& iconPath)
{
    if (!mainLayout) return nullptr;

    auto* parentWidget = mainLayout->parentWidget();
    auto* header = new StandardDialogHeader(title, subtitle, iconPath, parentWidget);
    mainLayout->insertWidget(0, header);
    return header;
}

QHBoxLayout* createStandardButtonRow(QDialog* dialog,
                                     QPushButton* primaryBtn,
                                     QPushButton* cancelBtn,
                                     const QString& helpTopic,
                                     QWidget* extraLeftWidget)
{
    auto* rowLayout = new QHBoxLayout();
    rowLayout->setContentsMargins(0, 8, 0, 0);
    rowLayout->setSpacing(8);

    // 1. Bouton d'aide contextuel à gauche
    if (!helpTopic.isEmpty() && dialog)
    {
        if (auto* help = TSA::UI::createHelpButton(dialog, helpTopic))
        {
            rowLayout->addWidget(help);
        }
    }

    // 2. Widget additionnel à gauche (ex: bouton Réinitialiser)
    if (extraLeftWidget)
    {
        rowLayout->addWidget(extraLeftWidget);
    }

    // 3. Espace élastique central
    rowLayout->addStretch(1);

    // 4. Bouton secondaire / Annuler
    if (cancelBtn)
    {
        cancelBtn->setAutoDefault(false);
        rowLayout->addWidget(cancelBtn);
    }

    // 5. Bouton d'action primaire à droite
    if (primaryBtn)
    {
        primaryBtn->setProperty("primary", true);
        primaryBtn->setDefault(true);
        rowLayout->addWidget(primaryBtn);
    }

    return rowLayout;
}

StandardDialog::StandardDialog(DialogSizeCategory category, QWidget* parent)
    : QDialog(parent)
{
    initBase(category);
}

void StandardDialog::initBase(DialogSizeCategory category)
{
    applyStandardDialogSizing(this, category);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);
    m_mainLayout->setSpacing(12);

    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(10);
    m_mainLayout->addLayout(m_contentLayout, 1);
}

void StandardDialog::setDialogHeader(const QString& title,
                                     const QString& subtitle,
                                     const QString& iconPath)
{
    if (!m_header)
    {
        m_header = new StandardDialogHeader(title, subtitle, iconPath, this);
        m_mainLayout->insertWidget(0, m_header);
    }
    else
    {
        m_header->setTitle(title);
        m_header->setSubtitle(subtitle);
        m_header->setIcon(iconPath);
    }
}

QHBoxLayout* StandardDialog::setStandardButtons(QPushButton* primaryBtn,
                                                QPushButton* cancelBtn,
                                                const QString& helpTopic,
                                                QWidget* extraLeftWidget)
{
    m_buttonLayout = createStandardButtonRow(this, primaryBtn, cancelBtn, helpTopic, extraLeftWidget);
    m_mainLayout->addLayout(m_buttonLayout);
    return m_buttonLayout;
}

void StandardDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}

} // namespace TSA::UI
