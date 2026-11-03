#pragma once

// Fenêtres de l'échange IFC (docs/IFC_MAPPING.md) : options de l'import et rapport détaillé de fin d'import (produits
// importés, à préparer, non porteurs, partiels, ignorés ; avertissements ; enregistrement CSV). La progression d'un
// import ou d'un export (thread de travail) : TaskProgressDialog.

#include "../../BIM/IFC/IfcImporter.h"

#include <QDialog>

class QCheckBox;

namespace TSA::UI
{

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
