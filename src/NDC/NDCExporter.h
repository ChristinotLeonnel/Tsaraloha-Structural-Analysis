#pragma once

#include "NDCDocumentModel.h"
#include <QString>

namespace TSA::NDC
{

/**
 * @brief Service d'export de la Note de Calcul (NDC) aux formats HTML et PDF vectoriel.
 */
class NDCExporter
{
public:
    static bool exportToHtml(const NDCDocument& doc, const QString& filePath, QString* error = nullptr);
    static bool exportToPdf(const NDCDocument& doc, const QString& filePath, QString* error = nullptr);
};

} // namespace TSA::NDC
