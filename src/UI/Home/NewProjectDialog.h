#pragma once

// Boîte « Nouveau projet » du Start Center : nom, emplacement et gabarit du projet à créer.
// Le fichier .tsa est créé immédiatement (il apparaît ainsi dans les projets récents).

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace TSA::UI
{

enum class ProjectTemplate
{
    GeneralStructure, ///< modèle vide avec la grille cartésienne par défaut
    Empty             ///< modèle vide, sans grille
};

struct NewProjectSettings
{
    QString name;
    QString directory;
    ProjectTemplate projectTemplate = ProjectTemplate::GeneralStructure;

    QString filePath() const;
};

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget* parent = nullptr);

    NewProjectSettings settings() const;

    void accept() override;

private:
    void browse();
    void validate();
    static QString defaultDirectory();
    static QString uniqueName(const QString& directory);

private:
    QLineEdit* m_name = nullptr;
    QLineEdit* m_directory = nullptr;
    QComboBox* m_template = nullptr;
    QLabel* m_target = nullptr;
    QLabel* m_error = nullptr;
    QPushButton* m_btnCreate = nullptr;
};

} // namespace TSA::UI
