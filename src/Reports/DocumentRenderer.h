#pragma once

// Rendu des documents produits par les templates (docs/TEMPLATES.md) : HTML autonome (UTF-8, styles et
// images incorporés) et PDF (QTextDocument → QPdfWriter, format de page choisi). Le moteur de rendu ne
// connaît ni le modèle ni la mise en page : il reçoit le HTML produit par un template.

#include "../Templates/TemplatePackage.h"

#include <QString>
#include <QStringList>

namespace TSA::Reports
{

struct PageSetup
{
    QString size = QStringLiteral("A4");   ///< A4, A3
    bool landscape = false;
    double marginMm = 15.0;
};

class DocumentRenderer
{
public:
    /// Rendu d'un rapport d'un paquet ; diagnostics complets dans le résultat.
    static Templates::RenderResult render(const Templates::TemplatePackage& package, const QString& reportId, const QJsonObject& data,
                                          const QJsonObject& optionValues = {});

    static bool writeHtml(const QString& html, const QString& path, QString* error = nullptr);
    static bool writePdf(const QString& html, const QString& path, const PageSetup& page = {}, QString* error = nullptr);

    /// Références à des ressources hors du document (fichiers locaux, réseau) : vide = document autonome.
    static QStringList externalReferences(const QString& html);
};

} // namespace TSA::Reports
