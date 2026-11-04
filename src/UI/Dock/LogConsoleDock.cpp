#include "LogConsoleDock.h"
#include "App/ProductInfo.h"
#include "../../Diagnostics/Logger.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDateTime>
#include <QScrollBar>

#include "../Theme/ThemeManager.h"

namespace TSA::UI
{

namespace
{
/// Ligne de journal en HTML, couleurs du thème actif (heure, niveau, module, texte échappé).
QString formatLogLine(const QString& time, const QString& type, const QString& module, const QString& text)
{
    const auto& tm = ThemeManager::instance();
    const QString moduleTag = module.isEmpty()
        ? QString()
        : QStringLiteral("<span style='color:%1;'>[%2]</span> ").arg(tm.tokens().logCommand.name(), module.toHtmlEscaped());
    return QStringLiteral("<span style='color:%1;'>[%2]</span> <b style='color:%3;'>[%4]</b> %5%6")
        .arg(tm.tokens().logTime.name(), time, tm.logLevelColor(type).name(), type.toHtmlEscaped(), moduleTag, text.toHtmlEscaped());
}
} // namespace

LogConsoleDock::LogConsoleDock(QWidget* parent)
    : QDockWidget(tr("CONSOLE et DIAGNOSTICS"), parent)
{
    setObjectName("LogConsoleDock");
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &LogConsoleDock::updateTheme);

    // Abonnement thread-safe aux événements du Logger central
    TSA::Diagnostics::Logger::instance().addListener("LogConsoleDock", [this](const TSA::Diagnostics::LogEntry& entry) {
        QString msg = QString::fromStdString(entry.message);
        QString type = QString::fromStdString(logLevelToString(entry.level));
        QString mod = QString::fromStdString(entry.module);
        QMetaObject::invokeMethod(this, [this, msg, type, mod]() {
            appendLog(msg, type, mod);
        }, Qt::QueuedConnection);
    });
}

LogConsoleDock::~LogConsoleDock()
{
    TSA::Diagnostics::Logger::instance().removeListener("LogConsoleDock");
}

void LogConsoleDock::setupUi()
{
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    // Barre d'outils supérieure : Filtres et export
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(6);

    auto* filterLabel = new QLabel(tr("Filtres :"), container);
    filterLabel->setProperty("caption", true);
    toolbarLayout->addWidget(filterLabel);

    m_levelFilterCombo = new QComboBox(container);
    m_levelFilterCombo->setProperty("compact", true);
    m_levelFilterCombo->addItems({ tr("Tous niveaux"), "INFO", "WARN", "ERROR", "CRIT", "DEBUG" });
    m_levelFilterCombo->setToolTip(tr("Filtrer par niveau de gravité"));
    connect(m_levelFilterCombo, &QComboBox::currentIndexChanged, this, &LogConsoleDock::onFilterChanged);
    toolbarLayout->addWidget(m_levelFilterCombo);

    m_moduleFilterCombo = new QComboBox(container);
    m_moduleFilterCombo->setProperty("compact", true);
    m_moduleFilterCombo->addItems({ tr("Tous modules"), "App", "CartesianGrid", "Command", "Model", "OCCT", "UI", "Qt" });
    m_moduleFilterCombo->setToolTip(tr("Filtrer par sous-système"));
    connect(m_moduleFilterCombo, &QComboBox::currentIndexChanged, this, &LogConsoleDock::onFilterChanged);
    toolbarLayout->addWidget(m_moduleFilterCombo);

    toolbarLayout->addStretch(1);

    m_exportBtn = new QPushButton(tr("Exporter Rapport..."), container);
    m_exportBtn->setProperty("compact", true);
    m_exportBtn->setToolTip(tr("Générer un rapport de diagnostic complet"));
    connect(m_exportBtn, &QPushButton::clicked, this, &LogConsoleDock::exportReportRequested);
    toolbarLayout->addWidget(m_exportBtn);

    m_clearBtn = new QPushButton(tr("Effacer"), container);
    m_clearBtn->setProperty("compact", true);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogConsoleDock::clearLog);
    toolbarLayout->addWidget(m_clearBtn);

    layout->addLayout(toolbarLayout);

    // Zone de texte console
    m_output = new QTextEdit(container);
    m_output->setReadOnly(true);
    m_output->setProperty("console", true);
    layout->addWidget(m_output, 1);

    // Ligne de commande interactive façon AutoCAD
    auto* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(6);

    m_promptLabel = new QLabel(tr("Commande :"), container);
    m_promptLabel->setProperty("consolePrompt", true);
    inputLayout->addWidget(m_promptLabel);

    m_input = new QLineEdit(container);
    m_input->setProperty("console", true);
    m_input->setPlaceholderText(tr("Tapez une commande (ex: BEAM, NODE, GRID, FIT) ou un raccourci..."));
    inputLayout->addWidget(m_input, 1);

    layout->addLayout(inputLayout);

    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_input->text().trimmed();
        if (!text.isEmpty())
        {
            appendLog(text, "CMD", "UI");
            emit commandEntered(text);
            m_input->clear();
        }
    });

    setWidget(container);
    setFixedHeight(175);

    updateTheme(ThemeManager::instance().isDarkMode());

    appendLog(QString::fromUtf8(TSA::Product::kConsoleBanner), "SYS", "App");
    appendLog(tr("Moteur graphique OpenCASCADE 8.0 actif."), "SYS", "OCCT");
}

void LogConsoleDock::updateTheme(bool /*isDark*/)
{
    // Champs, boutons et zone de texte suivent la feuille de style de l'application (propriétés console, compact,
    // consolePrompt posées dans setupUi). Les lignes déjà écrites portent leurs couleurs en HTML : elles sont
    // réécrites avec les couleurs du nouveau thème.
    onFilterChanged();
}

void LogConsoleDock::appendLog(const QString& message, const QString& type, const QString& module)
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");

    // Stocker dans l'historique interne pour permettre le filtrage dynamique
    m_history.push_back({ time, type, module, message });
    if (m_history.size() > 500)
    {
        m_history.erase(m_history.begin());
    }

    // Vérifier si le message correspond aux filtres actifs
    QString selectedLevel = m_levelFilterCombo ? m_levelFilterCombo->currentText() : "";
    if (!selectedLevel.isEmpty() && selectedLevel != tr("Tous niveaux") && selectedLevel != type)
    {
        return;
    }

    QString selectedModule = m_moduleFilterCombo ? m_moduleFilterCombo->currentText() : "";
    if (!selectedModule.isEmpty() && selectedModule != tr("Tous modules") && selectedModule != module)
    {
        return;
    }

    if (!m_output) return;

    m_output->append(formatLogLine(time, type, module, message));
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void LogConsoleDock::onFilterChanged()
{
    if (!m_output) return;

    m_output->clear();
    QString selectedLevel = m_levelFilterCombo ? m_levelFilterCombo->currentText() : "";
    QString selectedModule = m_moduleFilterCombo ? m_moduleFilterCombo->currentText() : "";

    for (const auto& item : m_history)
    {
        if (!selectedLevel.isEmpty() && selectedLevel != tr("Tous niveaux") && selectedLevel != item.type)
        {
            continue;
        }
        if (!selectedModule.isEmpty() && selectedModule != tr("Tous modules") && selectedModule != item.module)
        {
            continue;
        }

        m_output->append(formatLogLine(item.timestamp, item.type, item.module, item.text));
    }
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void LogConsoleDock::prefillCommand(const QString& text)
{
    if (!m_input) return;
    show();
    raise();
    m_input->setText(text);
    m_input->setFocus(Qt::OtherFocusReason);
    m_input->setCursorPosition(static_cast<int>(text.size()));
}

void LogConsoleDock::clearLog()
{
    m_history.clear();
    if (m_output)
    {
        m_output->clear();
    }
}

} // namespace TSA::UI
