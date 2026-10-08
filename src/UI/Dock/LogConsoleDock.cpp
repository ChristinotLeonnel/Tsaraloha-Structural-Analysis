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

LogConsoleDock::LogConsoleDock(QWidget* parent)
    : QDockWidget(tr("CONSOLE & DIAGNOSTICS"), parent)
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
    filterLabel->setStyleSheet("font-size: 10px; font-weight: bold; color: #8B949E;");
    toolbarLayout->addWidget(filterLabel);

    m_levelFilterCombo = new QComboBox(container);
    m_levelFilterCombo->addItems({ tr("Tous niveaux"), "INFO", "WARN", "ERROR", "CRIT", "DEBUG" });
    m_levelFilterCombo->setToolTip(tr("Filtrer par niveau de gravité"));
    connect(m_levelFilterCombo, &QComboBox::currentIndexChanged, this, &LogConsoleDock::onFilterChanged);
    toolbarLayout->addWidget(m_levelFilterCombo);

    m_moduleFilterCombo = new QComboBox(container);
    m_moduleFilterCombo->addItems({ tr("Tous modules"), "App", "CartesianGrid", "Command", "Model", "OCCT", "UI", "Qt" });
    m_moduleFilterCombo->setToolTip(tr("Filtrer par sous-système"));
    connect(m_moduleFilterCombo, &QComboBox::currentIndexChanged, this, &LogConsoleDock::onFilterChanged);
    toolbarLayout->addWidget(m_moduleFilterCombo);

    toolbarLayout->addStretch(1);

    m_exportBtn = new QPushButton(tr("Exporter Rapport..."), container);
    m_exportBtn->setToolTip(tr("Générer un rapport de diagnostic complet"));
    connect(m_exportBtn, &QPushButton::clicked, this, &LogConsoleDock::exportReportRequested);
    toolbarLayout->addWidget(m_exportBtn);

    m_clearBtn = new QPushButton(tr("Effacer"), container);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogConsoleDock::clearLog);
    toolbarLayout->addWidget(m_clearBtn);

    layout->addLayout(toolbarLayout);

    // Zone de texte console
    m_output = new QTextEdit(container);
    m_output->setReadOnly(true);
    layout->addWidget(m_output, 1);

    // Ligne de commande interactive façon AutoCAD
    auto* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(6);

    m_promptLabel = new QLabel(tr("Commande :"), container);
    inputLayout->addWidget(m_promptLabel);

    m_input = new QLineEdit(container);
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

void LogConsoleDock::updateTheme(bool isDark)
{
    if (m_output)
    {
        if (isDark)
        {
            m_output->setStyleSheet(
                "QTextEdit {"
                "   background-color: #1E293B;"
                "   color: #E2E8F0;"
                "   font-family: 'Consolas', 'Courier New', monospace;"
                "   font-size: 11px;"
                "   border: 1px solid #334155;"
                "   border-radius: 3px;"
                "}"
            );
        }
        else
        {
            m_output->setStyleSheet(
                "QTextEdit {"
                "   background-color: #FFFFFF;"
                "   color: #1E293B;"
                "   font-family: 'Consolas', 'Courier New', monospace;"
                "   font-size: 11px;"
                "   border: 1px solid #CBD5E1;"
                "   border-radius: 3px;"
                "}"
            );
        }
    }

    if (m_promptLabel)
    {
        m_promptLabel->setStyleSheet(isDark
            ? "font-family: Consolas, monospace; font-weight: bold; color: #38BDF8;"
            : "font-family: Consolas, monospace; font-weight: bold; color: #0284C7;");
    }

    if (m_input)
    {
        if (isDark)
        {
            m_input->setStyleSheet(
                "QLineEdit {"
                "   background-color: #0F172A;"
                "   color: #38BDF8;"
                "   font-family: 'Consolas', monospace;"
                "   font-size: 11px;"
                "   border: 1px solid #334155;"
                "   border-radius: 3px;"
                "   padding: 2px 6px;"
                "}"
                "QLineEdit:focus {"
                "   border: 1px solid #38BDF8;"
                "}"
            );
        }
        else
        {
            m_input->setStyleSheet(
                "QLineEdit {"
                "   background-color: #F8FAFC;"
                "   color: #0369A1;"
                "   font-family: 'Consolas', monospace;"
                "   font-size: 11px;"
                "   border: 1px solid #CBD5E1;"
                "   border-radius: 3px;"
                "   padding: 2px 6px;"
                "}"
                "QLineEdit:focus {"
                "   border: 1px solid #0284C7;"
                "}"
            );
        }
    }

    QString btnStyle = isDark
        ? "QPushButton { background-color: #334155; color: #F8FAFC; font-size: 10px; border: 1px solid #475569; border-radius: 2px; padding: 2px 8px; } QPushButton:hover { background-color: #475569; }"
        : "QPushButton { background-color: #E2E8F0; color: #334155; font-size: 10px; border: 1px solid #CBD5E1; border-radius: 2px; padding: 2px 8px; } QPushButton:hover { background-color: #CBD5E1; }";

    if (m_clearBtn) m_clearBtn->setStyleSheet(btnStyle);
    if (m_exportBtn) m_exportBtn->setStyleSheet(btnStyle);

    QString comboStyle = isDark
        ? "QComboBox { background-color: #0F172A; color: #E2E8F0; border: 1px solid #334155; border-radius: 2px; font-size: 10px; padding: 2px 4px; }"
        : "QComboBox { background-color: #F8FAFC; color: #1E293B; border: 1px solid #CBD5E1; border-radius: 2px; font-size: 10px; padding: 2px 4px; }";

    if (m_levelFilterCombo) m_levelFilterCombo->setStyleSheet(comboStyle);
    if (m_moduleFilterCombo) m_moduleFilterCombo->setStyleSheet(comboStyle);
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

    bool isDark = ThemeManager::instance().isDarkMode();
    QString color = isDark ? "#94A3B8" : "#475569";

    if (type == "CMD") color = isDark ? "#38BDF8" : "#0284C7";
    else if (type == "ERR" || type == "ERROR") color = isDark ? "#F87171" : "#DC2626";
    else if (type == "WARN") color = isDark ? "#FBBF24" : "#D97706";
    else if (type == "SYS") color = isDark ? "#4ADE80" : "#16A34A";
    else if (type == "CRIT") color = "#EF4444";
    else if (type == "DEBUG") color = isDark ? "#A78BFA" : "#7C3AED";

    QString timeColor = isDark ? "#64748B" : "#94A3B8";
    QString modTag = module.isEmpty() ? "" : QString("<span style='color:%1;'>[%2]</span> ").arg(isDark ? "#38BDF8" : "#0284C7", module);

    m_output->append(QString("<span style='color:%1;'>[%2]</span> <b style='color:%3;'>[%4]</b> %5%6")
        .arg(timeColor, time, color, type, modTag, message.toHtmlEscaped()));
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void LogConsoleDock::onFilterChanged()
{
    if (!m_output) return;

    m_output->clear();
    QString selectedLevel = m_levelFilterCombo ? m_levelFilterCombo->currentText() : "";
    QString selectedModule = m_moduleFilterCombo ? m_moduleFilterCombo->currentText() : "";

    bool isDark = ThemeManager::instance().isDarkMode();
    QString timeColor = isDark ? "#64748B" : "#94A3B8";

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

        QString color = isDark ? "#94A3B8" : "#475569";
        if (item.type == "CMD") color = isDark ? "#38BDF8" : "#0284C7";
        else if (item.type == "ERR" || item.type == "ERROR") color = isDark ? "#F87171" : "#DC2626";
        else if (item.type == "WARN") color = isDark ? "#FBBF24" : "#D97706";
        else if (item.type == "SYS") color = isDark ? "#4ADE80" : "#16A34A";
        else if (item.type == "CRIT") color = "#EF4444";
        else if (item.type == "DEBUG") color = isDark ? "#A78BFA" : "#7C3AED";

        QString modTag = item.module.isEmpty() ? "" : QString("<span style='color:%1;'>[%2]</span> ").arg(isDark ? "#38BDF8" : "#0284C7", item.module);

        m_output->append(QString("<span style='color:%1;'>[%2]</span> <b style='color:%3;'>[%4]</b> %5%6")
            .arg(timeColor, item.timestamp, color, item.type, modTag, item.text.toHtmlEscaped()));
    }
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
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
