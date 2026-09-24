#pragma once

#include <QDockWidget>

class QTextEdit;
class QLineEdit;
class QLabel;
class QPushButton;

namespace TSA::UI
{

class LogConsoleDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit LogConsoleDock(QWidget* parent = nullptr);

    void appendLog(const QString& message, const QString& type = "INFO");
    void clearLog();
    void updateTheme(bool isDark);

signals:
    void commandEntered(const QString& command);

private:
    void setupUi();

private:
    QTextEdit* m_output = nullptr;
    QLineEdit* m_input = nullptr;
    QLabel* m_promptLabel = nullptr;
    QPushButton* m_clearBtn = nullptr;
};

} // namespace TSA::UI
