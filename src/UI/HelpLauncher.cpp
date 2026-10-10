#include "HelpLauncher.h"

#include "../Help/HelpTopics.h"

#include <ProductIdentity.h>

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>

namespace TSA::UI
{

namespace
{

Q_LOGGING_CATEGORY(lcHelp, "tsa.help")

std::function<bool(const QUrl&)>& opener()
{
    static std::function<bool(const QUrl&)> instance;
    return instance;
}

/// Message non bloquant : l'utilisateur peut copier l'adresse et l'ouvrir lui-même.
void showFailure(QWidget* parent, const QString& reason, const QUrl& url)
{
    QString text = reason.toHtmlEscaped();
    if (!url.isEmpty())
        text += QStringLiteral("<br><br>%1<br><b>%2</b>")
                    .arg(QObject::tr("Adresse de la page (sélectionnable) :"), url.toString().toHtmlEscaped());
    auto* box = new QMessageBox(QMessageBox::Warning, QObject::tr("Documentation en ligne"), text, QMessageBox::Ok, parent);
    box->setTextFormat(Qt::RichText);
    box->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

} // namespace

bool isOnlineHelpAvailable()
{
    return !QString::fromUtf8(TSA::Product::kDocsBaseUrl).trimmed().isEmpty();
}

QPushButton* createHelpButton(QWidget* parent, const QString& topicId)
{
    if (!isOnlineHelpAvailable()) return nullptr;
    auto* button = new QPushButton(QObject::tr("Aide"), parent);
    button->setObjectName(QStringLiteral("helpButton"));
    button->setProperty("helpTopic", topicId);
    button->setToolTip(QObject::tr("Ouvrir la page de documentation en ligne de cette fenêtre"));
    button->setAutoDefault(false);
    QObject::connect(button, &QPushButton::clicked, button, [parent, topicId]() { openHelpTopic(parent, topicId); });
    return button;
}

HelpOpenResult openHelpTopic(QWidget* parent, const QString& topicId)
{
    // TSA est en français : la documentation est demandée en français (repli du site sinon).
    const TSA::Help::HelpUrl help = TSA::Help::officialHelpUrl(topicId, QStringLiteral("fr"));

    if (!isOnlineHelpAvailable()) return HelpOpenResult::NotConfigured;

    if (!help.error.isEmpty())
    {
        qCWarning(lcHelp).noquote() << help.error;
        showFailure(parent, help.error, QUrl());
        return HelpOpenResult::Failed;
    }
    if (!help.knownTopic)
        qCWarning(lcHelp).noquote() << "Identifiant d'aide inconnu :" << topicId << "— accueil de la documentation ouvert.";

    const bool opened = opener() ? opener()(help.url) : QDesktopServices::openUrl(help.url);
    if (!opened)
    {
        qCWarning(lcHelp).noquote() << "Ouverture du navigateur impossible :" << help.url.toString();
        showFailure(parent, QObject::tr("Le navigateur n'a pas pu être ouvert."), help.url);
        return HelpOpenResult::Failed;
    }
    return HelpOpenResult::Opened;
}

void setHelpUrlOpenerForTesting(std::function<bool(const QUrl&)> opener_)
{
    opener() = std::move(opener_);
}

} // namespace TSA::UI
