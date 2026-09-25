#pragma once

#include <QImage>
#include <QByteArray>

namespace TSA::Model
{
    class Model;
}

namespace TSA::IO
{

class TSAPreviewGenerator
{
public:
    /**
     * @brief Génère une image QImage représentant le modèle 3D cadré
     * @param model Le modèle structural source
     * @param width Largeur de la miniature (ex: 512)
     * @param height Hauteur de la miniature (ex: 512)
     * @param directCapture Capture d'image de la vue 3D active (optionnelle)
     * @return QImage de la miniature
     */
    static QImage generateThumbnail(const TSA::Model::Model& model,
                                    int width = 512,
                                    int height = 512,
                                    const QImage& directCapture = QImage());

    /**
     * @brief Génère directement les octets compressés au format PNG
     */
    static QByteArray generatePngData(const TSA::Model::Model& model,
                                      int width = 512,
                                      int height = 512,
                                      const QImage& directCapture = QImage());
};

} // namespace TSA::IO
