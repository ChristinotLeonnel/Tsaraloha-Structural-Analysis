#pragma once

#include <QDialog>

class QListWidget;
class QTextBrowser;
class QLineEdit;

namespace TSA::UI
{

class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(QWidget* parent = nullptr);
    ~HelpDialog() override = default;

    void selectTopic(int index);

private slots:
    void onTopicChanged(int row);
    void onSearchChanged(const QString& filter);

private:
    void setupUi();
    void updateTheme();
    QString getHelpContent(int topicIndex) const;

private:
    QListWidget* m_topicsList = nullptr;
    QTextBrowser* m_browser = nullptr;
    QLineEdit* m_searchBox = nullptr;
};

} // namespace TSA::UI
