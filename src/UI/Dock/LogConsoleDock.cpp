#include "LogConsoleDock.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDateTime>
#include <QScrollBar>

#include "../Theme/ThemeManager.h"

namespace TSA::UI
{

LogConsoleDock::LogConsoleDock(QWidget* parent)
    : QDockWidget(tr("CONSOLE / LIGNE DE COMMANDE"), parent)
{
    setObjectName("LogConsoleDock");
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &LogConsoleDock::updateTheme);
}

void LogConsoleDock::setupUi()
{
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

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

    m_clearBtn = new QPushButton(tr("Effacer"), container);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogConsoleDock::clearLog);
    inputLayout->addWidget(m_clearBtn);

    layout->addLayout(inputLayout);

    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_input->text().trimmed();
        if (!text.isEmpty())
        {
            appendLog(text, "CMD");
            emit commandEntered(text);
            m_input->clear();
        }
    });

    setWidget(container);
    setFixedHeight(140);

    updateTheme(ThemeManager::instance().isDarkMode());

    appendLog(tr("TSA Structural Analysis Modeler initialisé avec succès."), "SYS");
    appendLog(tr("Moteur graphique OpenCASCADE 8.0 actif."), "SYS");
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

    if (m_clearBtn)
    {
        if (isDark)
        {
            m_clearBtn->setStyleSheet(
                "QPushButton {"
                "   background-color: #334155;"
                "   color: #F8FAFC;"
                "   font-size: 10px;"
                "   border: none;"
                "   border-radius: 2px;"
                "   padding: 2px 8px;"
                "}"
                "QPushButton:hover { background-color: #475569; }"
            );
        }
        else
        {
            m_clearBtn->setStyleSheet(
                "QPushButton {"
                "   background-color: #E2E8F0;"
                "   color: #334155;"
                "   font-size: 10px;"
                "   border: 1px solid #CBD5E1;"
                "   border-radius: 2px;"
                "   padding: 2px 8px;"
                "}"
                "QPushButton:hover { background-color: #CBD5E1; }"
            );
        }
    }
}

void LogConsoleDock::appendLog(const QString& message, const QString& type)
{
    if (!m_output) return;

    bool isDark = ThemeManager::instance().isDarkMode();
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = isDark ? "#94A3B8" : "#475569";

    if (type == "CMD") color = isDark ? "#38BDF8" : "#0284C7";
    else if (type == "ERR") color = isDark ? "#F87171" : "#DC2626";
    else if (type == "WARN") color = isDark ? "#FBBF24" : "#D97706";
    else if (type == "SYS") color = isDark ? "#4ADE80" : "#16A34A";

    QString timeColor = isDark ? "#64748B" : "#94A3B8";
    m_output->append(QString("<span style='color:%1;'>[%2]</span> <b style='color:%3;'>[%4]</b> %5")
        .arg(timeColor, time, color, type, message.toHtmlEscaped()));
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void LogConsoleDock::clearLog()
{
    if (m_output)
    {
        m_output->clear();
    }
}

} // namespace TSA::UI
