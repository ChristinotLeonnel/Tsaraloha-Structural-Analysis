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

namespace TSA::UI
{

LogConsoleDock::LogConsoleDock(QWidget* parent)
    : QDockWidget(tr("CONSOLE / LIGNE DE COMMANDE"), parent)
{
    setObjectName("LogConsoleDock");
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    setupUi();
}

void LogConsoleDock::setupUi()
{
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    m_output = new QTextEdit(container);
    m_output->setReadOnly(true);
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
    layout->addWidget(m_output, 1);

    // Ligne de commande interactive façon AutoCAD
    auto* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(6);

    auto* promptLabel = new QLabel(tr("Commande :"), container);
    promptLabel->setStyleSheet("font-family: Consolas, monospace; font-weight: bold; color: #0284C7;");
    inputLayout->addWidget(promptLabel);

    m_input = new QLineEdit(container);
    m_input->setPlaceholderText(tr("Tapez une commande (ex: BEAM, NODE, GRID, FIT) ou un raccourci..."));
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
    inputLayout->addWidget(m_input, 1);

    auto* clearBtn = new QPushButton(tr("Effacer"), container);
    clearBtn->setStyleSheet(
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
    connect(clearBtn, &QPushButton::clicked, this, &LogConsoleDock::clearLog);
    inputLayout->addWidget(clearBtn);

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

    appendLog(tr("TSA Structural Analysis Modeler initialisé avec succès."), "SYS");
    appendLog(tr("Moteur graphique OpenCASCADE 8.0 actif."), "SYS");
}

void LogConsoleDock::appendLog(const QString& message, const QString& type)
{
    if (!m_output) return;

    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = "#94A3B8";

    if (type == "CMD") color = "#38BDF8";
    else if (type == "ERR") color = "#F87171";
    else if (type == "WARN") color = "#FBBF24";
    else if (type == "SYS") color = "#4ADE80";

    m_output->append(QString("<span style='color:#64748B;'>[%1]</span> <b style='color:%2;'>[%3]</b> %4")
        .arg(time, color, type, message.toHtmlEscaped()));
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
