#include "StandardDialogHeader.h"
#include "../Theme/ThemedIcon.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

namespace TSA::UI
{

StandardDialogHeader::StandardDialogHeader(const QString& title,
                                           const QString& subtitle,
                                           const QString& iconPath,
                                           QWidget* parent)
    : QWidget(parent)
{
    setupUi(title, subtitle, iconPath);
}

void StandardDialogHeader::setupUi(const QString& title, const QString& subtitle, const QString& iconPath)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 4);
    rootLayout->setSpacing(6);

    auto* topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(12);

    // Icône contextuelle
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(32, 32);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    setIcon(iconPath);
    topLayout->addWidget(m_iconLabel);

    // Titre et sous-titre
    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setProperty("dialogTitle", true);
    m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    textLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(subtitle, this);
    m_subtitleLabel->setProperty("dialogSubtitle", true);
    m_subtitleLabel->setWordWrap(true);
    m_subtitleLabel->setVisible(!subtitle.isEmpty());
    textLayout->addWidget(m_subtitleLabel);

    topLayout->addLayout(textLayout, 1);
    rootLayout->addLayout(topLayout);

    // Ligne séparatrice fine
    m_divider = new QFrame(this);
    m_divider->setProperty("dialogHeaderDivider", true);
    m_divider->setFrameShape(QFrame::HLine);
    m_divider->setFrameShadow(QFrame::Plain);
    m_divider->setFixedHeight(1);
    rootLayout->addWidget(m_divider);
}

void StandardDialogHeader::setTitle(const QString& title)
{
    if (m_titleLabel)
    {
        m_titleLabel->setText(title);
    }
}

void StandardDialogHeader::setSubtitle(const QString& subtitle)
{
    if (m_subtitleLabel)
    {
        m_subtitleLabel->setText(subtitle);
        m_subtitleLabel->setVisible(!subtitle.isEmpty());
    }
}

void StandardDialogHeader::setIcon(const QString& iconPath)
{
    if (!m_iconLabel) return;

    if (!iconPath.isEmpty())
    {
        QIcon icon = TSA::UI::themedIcon(iconPath);
        if (!icon.isNull())
        {
            m_iconLabel->setPixmap(icon.pixmap(32, 32));
            m_iconLabel->show();
            return;
        }
    }
    m_iconLabel->hide();
}

QString StandardDialogHeader::title() const
{
    return m_titleLabel ? m_titleLabel->text() : QString();
}

QString StandardDialogHeader::subtitle() const
{
    return m_subtitleLabel ? m_subtitleLabel->text() : QString();
}

} // namespace TSA::UI
