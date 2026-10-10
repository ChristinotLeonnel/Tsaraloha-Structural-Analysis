// Cotations 3D : commandes (afficher, modifier, supprimer, style, nettoyage) ; les outils de création
// sont des outils du registre (catégorie Annotate, src/Interaction/Tools/DimensionTools).

#include "MainWindow.h"

#include "Dialogs/DimensionDialogs.h"
#include "../Annotation/DimensionGeometry.h"
#include "../Annotation/DimensionService.h"
#include "../Model/Model.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"

#include <QAction>
#include <QLabel>
#include <QMessageBox>

void MainWindow::createDimensionActions()
{
    m_actionDimensionsVisible = new QAction(QIcon(":/icons/dimensions/dim_visibility.svg"), tr("Afficher les cotations"), this);
    m_actionDimensionsVisible->setCheckable(true);
    m_actionDimensionsVisible->setChecked(true);
    m_actionDimensionsVisible->setToolTip(tr("Afficher ou masquer les cotations 3D (enregistré dans le projet)"));
    connect(m_actionDimensionsVisible, &QAction::toggled, this, &MainWindow::onToggleDimensionsVisible);

    m_actionEditDimension = new QAction(QIcon(":/icons/dimensions/dim_edit.svg"), tr("Modifier la cotation..."), this);
    m_actionEditDimension->setToolTip(tr("Modifier la cotation sélectionnée : position, texte, couleur, réassociation"));
    connect(m_actionEditDimension, &QAction::triggered, this, &MainWindow::onEditDimension);

    m_actionDeleteDimensions = new QAction(QIcon(":/icons/dimensions/dim_delete.svg"), tr("Supprimer les cotations"), this);
    m_actionDeleteDimensions->setToolTip(tr("Supprimer les cotations sélectionnées (Suppr fonctionne aussi)"));
    connect(m_actionDeleteDimensions, &QAction::triggered, this, &MainWindow::onDeleteDimensions);

    m_actionDimensionStyle = new QAction(QIcon(":/icons/dimensions/dim_style.svg"), tr("Style des cotations..."), this);
    m_actionDimensionStyle->setToolTip(tr("Unités, décimales, arrondi, hauteur du texte, flèches, couleurs"));
    connect(m_actionDimensionStyle, &QAction::triggered, this, &MainWindow::onDimensionStyle);

    m_actionCleanDimensions = new QAction(QIcon(":/icons/dimensions/dim_clean.svg"), tr("Supprimer les cotations invalides"), this);
    m_actionCleanDimensions->setToolTip(tr("Supprimer les cotations dont un nœud associé a été supprimé"));
    connect(m_actionCleanDimensions, &QAction::triggered, this, &MainWindow::onCleanDimensions);

    if (m_selectionManager)
    {
        connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::dimensionSelected, this, [this](int id) {
            if (!m_model || !m_statusInfo) return;
            auto it = m_model->dimensions().items.find(id);
            if (it == m_model->dimensions().items.end()) return;
            const auto layout = TSA::Annotation::layoutFor(*m_model, it->second, m_model->dimensions().style);
            QStringList texts;
            for (const auto& t : layout.texts) texts << QString::fromStdString(t.text);
            m_statusInfo->setText(tr("%1 #%2 : %3%4 — Suppr pour supprimer, « Modifier la cotation » pour la modifier")
                                      .arg(QString::fromStdString(TSA::Annotation::kindDisplayName(it->second.kind, it->second.axis)))
                                      .arg(id)
                                      .arg(texts.join(QStringLiteral(" ; ")))
                                      .arg(layout.invalidReference ? tr(" (référence invalide)") : QString()));
        });
    }
}

void MainWindow::syncDimensionActions()
{
    if (!m_actionDimensionsVisible || !m_model) return;
    const QSignalBlocker block(m_actionDimensionsVisible);
    m_actionDimensionsVisible->setChecked(m_model->dimensions().style.visible);
}

void MainWindow::onToggleDimensionsVisible(bool visible)
{
    if (!m_model) return;
    auto style = m_model->dimensions().style;
    if (style.visible == visible) return;
    style.visible = visible;
    TSA::Annotation::setDimensionStyle(*m_model, style);
    updateUndoRedoActions();
    updateWindowTitle();
}

void MainWindow::onEditDimension()
{
    if (!m_model || !m_selectionManager) return;
    const auto& sel = m_selectionManager->selectedDimensions();
    if (sel.size() != 1)
    {
        QMessageBox::information(this, tr("Modifier la cotation"), tr("Sélectionnez une cotation (clic sur sa ligne ou son texte)."));
        return;
    }
    TSA::UI::DimensionEditDialog dlg(m_model, *sel.begin(), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        updateUndoRedoActions();
        updateWindowTitle();
    }
}

void MainWindow::onDeleteDimensions()
{
    if (!m_model || !m_selectionManager) return;
    const std::set<int> ids = m_selectionManager->selectedDimensions();
    const int removed = TSA::Annotation::removeDimensions(*m_model, ids);
    if (removed) m_selectionManager->clearSelection();
    if (m_statusInfo) m_statusInfo->setText(removed ? tr("%1 cotation(s) supprimée(s)").arg(removed) : tr("Aucune cotation sélectionnée"));
    updateUndoRedoActions();
    updateWindowTitle();
}

void MainWindow::onDimensionStyle()
{
    if (!m_model) return;
    TSA::UI::DimensionStyleDialog dlg(m_model->dimensions().style, this);
    if (dlg.exec() != QDialog::Accepted) return;
    TSA::Annotation::setDimensionStyle(*m_model, dlg.style());
    syncDimensionActions();
    updateUndoRedoActions();
    updateWindowTitle();
}

void MainWindow::onCleanDimensions()
{
    if (!m_model) return;
    const int removed = TSA::Annotation::removeInvalidDimensions(*m_model);
    if (m_selectionManager) m_selectionManager->pruneMissing(*m_model);
    if (m_statusInfo) m_statusInfo->setText(tr("%1 cotation(s) invalide(s) supprimée(s)").arg(removed));
    updateUndoRedoActions();
    updateWindowTitle();
}
