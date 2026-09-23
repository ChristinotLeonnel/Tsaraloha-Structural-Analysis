#pragma once

#include <QDockWidget>

class QTextEdit;
class QLineEdit;

namespace TSA::UI
{

class LogConsoleDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit LogConsoleDock(QWidget* parent = nullptr);

    void appendLog(const QString& message, const QString& type = "INFO");
    void clearLog();

signals:
    void commandEntered(const QString& command);

private:
    void setupUi();

private:
    QTextEdit* m_output = nullptr;
    QLineEdit* m_input = nullptr;
};

} // namespace TSA::UI
