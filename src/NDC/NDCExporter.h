#pragma once

#include "NDCDocumentModel.h"
#include <QString>
#include <QStringList>

namespace TSA::NDC
{

/**
 * @brief Service d'export de la Note de Calcul (NDC) aux formats HTML et PDF vectoriel.
 */
class NDCExporter
{
public:
    /// HTML de la note : template « config.templateId » du dépôt (rendu par le moteur de templates, images
    /// incorporées). Repli explicite sur NDCDocument::toHtml si le template est introuvable ou en erreur
    /// (*diagnostics le signale). *templateUsed : « id version (origine) » ou « générateur historique ».
    static QString renderHtml(const NDCDocument& doc, QStringList* diagnostics = nullptr, QString* templateUsed = nullptr);

    static bool exportToHtml(const NDCDocument& doc, const QString& filePath, QString* error = nullptr);
    static bool exportToPdf(const NDCDocument& doc, const QString& filePath, QString* error = nullptr);
};

} // namespace TSA::NDC
