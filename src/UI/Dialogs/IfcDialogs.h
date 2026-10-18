#pragma once

// Fenêtres de l'échange IFC (docs/IFC_MAPPING.md) : options de l'import, progression d'un travail mené dans un thread
// de travail, et rapport détaillé de fin d'import (produits importés, à préparer, non porteurs, partiels, ignorés ;
// avertissements ; enregistrement CSV).

#include "../../BIM/IFC/IfcImporter.h"

#include <QDialog>
#include <QProgressDialog>

class QCheckBox;

namespace TSA::UI
{

/// Progression d'un import ou d'un export IFC mené dans un thread de travail. « Annuler » (bouton, Échap, fermeture)
/// ne fait que demander l'arrêt : la fenêtre reste affichée, et la fenêtre principale bloquée, jusqu'à la fin
/// effective du travail (sinon une seconde commande pourrait démarrer dans la boucle locale de la première).
class IfcProgressDialog : public QProgressDialog
{
    Q_OBJECT
public:
    IfcProgressDialog(const QString& title, const QString& label, QWidget* parent = nullptr);
    bool cancelRequested() const { return m_cancelRequested; }
    /// Étape en cours (ignorée une fois l'annulation demandée : le libellé « Annulation… » reste affiché).
    void setStage(int percent, const QString& stage);

protected:
    void reject() override;
    void closeEvent(QCloseEvent* event) override;

private:
    void requestCancel();

    bool m_cancelRequested = false;
};

class IfcImportOptionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit IfcImportOptionsDialog(const QString& path, QWidget* parent = nullptr);
    /// Options choisies (progression non renseignée).
    TSA::BIM::Ifc::IfcImportOptions options() const;

private:
    QCheckBox* m_analytical = nullptr;
    QCheckBox* m_derive = nullptr;
    QCheckBox* m_geometry = nullptr;
};

class IfcImportReportDialog : public QDialog
{
    Q_OBJECT
public:
    IfcImportReportDialog(const QString& path, const TSA::BIM::Ifc::IfcImportReport& report, QWidget* parent = nullptr);

private:
    void saveCsv();

    QString m_path;
    TSA::BIM::Ifc::IfcImportReport m_report;
};

} // namespace TSA::UI
