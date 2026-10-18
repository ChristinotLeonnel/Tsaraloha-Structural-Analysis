#pragma once

// Vues nommées du projet actif : enregistrer la vue actuelle, restaurer (double-clic), renommer, supprimer.
// Les vues appartiennent au modèle du projet (Model::namedViews, chunk VIEW du .tsa) : le dialogue ne montre que
// celles du projet qui l'a ouvert.

#include <QDialog>

class QListWidget;
class QPushButton;
class OccView;

namespace TSA::Model { class Model; }

namespace TSA::UI
{

class NamedViewsDialog : public QDialog
{
    Q_OBJECT

public:
    NamedViewsDialog(TSA::Model::Model* model, OccView* view, QWidget* parent = nullptr);

    /// Actions du dialogue, appelables directement (tests) ; false si refusée (nom vide ou déjà pris, vue absente).
    bool saveCurrentView(const QString& name);
    bool restoreView(const QString& name);
    bool renameView(const QString& from, const QString& to);
    bool deleteView(const QString& name);
    QStringList viewNames() const;

signals:
    /// Vues du projet modifiées (projet à enregistrer).
    void namedViewsChanged();
    void viewRestored(const QString& name);

private:
    void refreshList(const QString& select = QString());
    QString selectedName() const;
    void onSave();
    void onRename();
    void onDelete();
    void updateButtons();

    TSA::Model::Model* m_model = nullptr;
    OccView* m_view = nullptr;
    QListWidget* m_list = nullptr;
    QPushButton* m_btnRestore = nullptr;
    QPushButton* m_btnRename = nullptr;
    QPushButton* m_btnDelete = nullptr;
};

} // namespace TSA::UI
