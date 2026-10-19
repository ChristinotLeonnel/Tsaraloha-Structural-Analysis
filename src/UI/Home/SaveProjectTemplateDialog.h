#pragma once

// Boîte « Enregistrer comme template » : nom et description du modèle de projet (Project/ProjectTemplates.h).

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace TSA::Project
{
class ProjectTemplateStore;
}

namespace TSA::UI
{

class SaveProjectTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    SaveProjectTemplateDialog(const TSA::Project::ProjectTemplateStore& store, const QString& suggestedName, QWidget* parent = nullptr);

    QString templateName() const;
    QString description() const;

private:
    void validate();

private:
    const TSA::Project::ProjectTemplateStore& m_store;
    QLineEdit* m_name = nullptr;
    QPlainTextEdit* m_description = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_btnSave = nullptr;
};

} // namespace TSA::UI
