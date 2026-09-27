#pragma once

#include <QDockWidget>

class QTextEdit;
class QLineEdit;
class QLabel;
class QPushButton;
class QComboBox;

namespace TSA::UI
{

class LogConsoleDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit LogConsoleDock(QWidget* parent = nullptr);
    ~LogConsoleDock() override;

    void appendLog(const QString& message, const QString& type = "INFO", const QString& module = "General");
    void clearLog();
    void updateTheme(bool isDark);

signals:
    void commandEntered(const QString& command);
    void exportReportRequested();

private slots:
    void onFilterChanged();

private:
    void setupUi();

private:
    QTextEdit* m_output = nullptr;
    QLineEdit* m_input = nullptr;
    QLabel* m_promptLabel = nullptr;
    QComboBox* m_levelFilterCombo = nullptr;
    QComboBox* m_moduleFilterCombo = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;

    struct StoredLogMessage
    {
        QString timestamp;
        QString type;
        QString module;
        QString text;
    };
    std::vector<StoredLogMessage> m_history;
};

} // namespace TSA::UI
