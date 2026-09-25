#include "TSAPreviewGenerator.h"
#include "../Model/Model.h"

#include <QPainter>
#include <QPainterPath>
#include <QBuffer>
#include <QGuiApplication>
#include <cmath>
#include <algorithm>
#include <map>

namespace TSA::IO
{

namespace
{
struct ScreenPoint
{
    double x = 0.0;
    double y = 0.0;
    double depth = 0.0;
};

// Projection axonométrique isométrique standard (Azimut 45°, Élévation 30°)
ScreenPoint projectPoint(double X, double Y, double Z,
                         double Cx, double Cy, double Cz,
                         double scale, double offsetX, double offsetY)
{
    const double cos45 = 0.7071067811865475;
    const double sin45 = 0.7071067811865475;
    const double cos30 = 0.8660254037844386;
    const double sin30 = 0.5000000000000000;

    double dx = X - Cx;
    double dy = Y - Cy;
    double dz = Z - Cz;

    // Rotation horizontale Azimut 45°
    double xr = (dx - dy) * cos45;
    double yr = (dx + dy) * sin45;

    // Projection verticale Élévation 30°
    double u = xr;
    double v = -yr * sin30 + dz * cos30;
    double depth = yr * cos30 + dz * sin30;

    ScreenPoint pt;
    pt.x = offsetX + u * scale;
    pt.y = offsetY - v * scale; // Inversion axe vertical écran
    pt.depth = depth;
    return pt;
}
} // namespace

QImage TSAPreviewGenerator::generateThumbnail(const TSA::Model::Model& model,
                                              int width,
                                              int height,
                                              const QImage& directCapture)
{
    // 1. Tenter la capture directe si fournie
    if (!directCapture.isNull())
    {
        return directCapture.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // 2. Rendu vectoriel offscreen 3D déterministe (indépendant de GPU/OpenGL)
    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Fond professionnel style Robot / Dark CAD
    QLinearGradient bgGrad(0, 0, width, height);
    bgGrad.setColorAt(0.0, QColor(24, 32, 47));
    bgGrad.setColorAt(1.0, QColor(11, 15, 25));
    painter.fillRect(0, 0, width, height, bgGrad);

    // Cadre subtil
    painter.setPen(QPen(QColor(51, 65, 85), 2.0));
    painter.drawRoundedRect(1, 1, width - 2, height - 2, 8, 8);

    const bool hasGuiApp = (QGuiApplication::instance() != nullptr);

    const auto& nodesMap = model.nodes();
    if (nodesMap.empty())
    {
        // Projet vide : afficher un graphique de repère neutre
        if (hasGuiApp)
        {
            painter.setPen(QColor(148, 163, 184));
            QFont f = painter.font();
            f.setPointSize(14);
            f.setBold(true);
            painter.setFont(f);
            painter.drawText(QRect(0, 0, width, height), Qt::AlignCenter, "TSA — Projet Structurel");
        }
        return img;
    }

    // Calcul de la Bounding Box 3D
    double minX = 1e9, maxX = -1e9;
    double minY = 1e9, maxY = -1e9;
    double minZ = 1e9, maxZ = -1e9;

    for (const auto& [id, n] : nodesMap)
    {
        minX = std::min(minX, n.x());
        maxX = std::max(maxX, n.x());
        minY = std::min(minY, n.y());
        maxY = std::max(maxY, n.y());
        minZ = std::min(minZ, n.z());
        maxZ = std::max(maxZ, n.z());
    }

    double Cx = (minX + maxX) * 0.5;
    double Cy = (minY + maxY) * 0.5;
    double Cz = (minZ + maxZ) * 0.5;

    // Calcul du facteur d'échelle pour cadrage automatique (padding de 60 px)
    const double cos45 = 0.7071067811865475;
    const double sin30 = 0.5;
    const double cos30 = 0.8660254037844386;

    double minU = 1e9, maxU = -1e9;
    double minV = 1e9, maxV = -1e9;

    for (const auto& [id, n] : nodesMap)
    {
        double dx = n.x() - Cx;
        double dy = n.y() - Cy;
        double dz = n.z() - Cz;
        double u = (dx - dy) * cos45;
        double v = -(dx + dy) * cos45 * sin30 + dz * cos30;
        minU = std::min(minU, u);
        maxU = std::max(maxU, u);
        minV = std::min(minV, v);
        maxV = std::max(maxV, v);
    }

    double spanU = std::max(1.0, maxU - minU);
    double spanV = std::max(1.0, maxV - minV);

    double availW = width - 120.0;
    double availH = height - 120.0;
    double scale = std::min(availW / spanU, availH / spanV);

    double offX = width * 0.5 - ((minU + maxU) * 0.5) * scale;
    double offY = height * 0.5 + ((minV + maxV) * 0.5) * scale;

    // Projeter tous les nœuds
    std::map<int, ScreenPoint> projectedNodes;
    for (const auto& [id, n] : nodesMap)
    {
        projectedNodes[id] = projectPoint(n.x(), n.y(), n.z(), Cx, Cy, Cz, scale, offX, offY);
    }

    // 1. Grille de référence au sol (Z = minZ)
    painter.setPen(QPen(QColor(51, 65, 85, 90), 1.0, Qt::DashLine));
    int gridSteps = 4;
    double gStepX = (maxX - minX > 0.1) ? (maxX - minX) / gridSteps : 1.0;
    double gStepY = (maxY - minY > 0.1) ? (maxY - minY) / gridSteps : 1.0;
    for (int i = 0; i <= gridSteps; ++i)
    {
        double gx = minX + i * gStepX;
        auto pA = projectPoint(gx, minY, minZ, Cx, Cy, Cz, scale, offX, offY);
        auto pB = projectPoint(gx, maxY, minZ, Cx, Cy, Cz, scale, offX, offY);
        painter.drawLine(QPointF(pA.x, pA.y), QPointF(pB.x, pB.y));

        double gy = minY + i * gStepY;
        auto pC = projectPoint(minX, gy, minZ, Cx, Cy, Cz, scale, offX, offY);
        auto pD = projectPoint(maxX, gy, minZ, Cx, Cy, Cz, scale, offX, offY);
        painter.drawLine(QPointF(pC.x, pC.y), QPointF(pD.x, pD.y));
    }

    // 2. Rendu des Dalles (Surfaces semi-transparentes)
    for (const auto& [id, slab] : model.slabs())
    {
        const auto& ids = slab.nodeIds();
        if (ids.size() >= 3)
        {
            QPolygonF poly;
            for (int nid : ids)
            {
                if (projectedNodes.count(nid))
                {
                    poly << QPointF(projectedNodes[nid].x, projectedNodes[nid].y);
                }
            }
            painter.setBrush(QColor(14, 165, 233, 70));
            painter.setPen(QPen(QColor(56, 189, 248), 1.5));
            painter.drawPolygon(poly);
        }
    }

    // 3. Rendu des Poutres et Barres
    auto renderBar = [&](int startId, int endId, const TSA::Model::Section& sec, const QColor& color, double widthMul) {
        if (!projectedNodes.count(startId) || !projectedNodes.count(endId)) return;
        const auto& p1 = projectedNodes[startId];
        const auto& p2 = projectedNodes[endId];

        double barThickness = 3.5;
        if (sec.shape == TSA::Model::SectionShape::Circular)
        {
            barThickness = std::clamp(sec.diameter * scale * 0.8, 3.5, 14.0);
        }
        else
        {
            barThickness = std::clamp(std::max(sec.width, sec.height) * scale * 0.8, 3.5, 14.0);
        }
        barThickness *= widthMul;

        // Ombre de la barre pour effet 3D relief
        painter.setPen(QPen(QColor(0, 0, 0, 90), barThickness + 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(p1.x, p1.y + 1.5), QPointF(p2.x, p2.y + 1.5));

        // Corps de la barre
        painter.setPen(QPen(color, barThickness, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(p1.x, p1.y), QPointF(p2.x, p2.y));
    };

    // Poteaux (orange / ambre technique)
    for (const auto& [id, col] : model.columns())
    {
        renderBar(col.startNodeId(), col.endNodeId(), col.section(), QColor(245, 158, 11), 1.1);
    }

    // Poutres (bleu électrique moderne)
    for (const auto& [id, beam] : model.beams())
    {
        renderBar(beam.startNodeId(), beam.endNodeId(), beam.section(), QColor(59, 130, 246), 1.0);
    }

    // Treillis (cyan fin)
    for (const auto& [id, truss] : model.trussMembers())
    {
        renderBar(truss.startNodeId(), truss.endNodeId(), truss.section(), QColor(6, 182, 212), 0.75);
    }

    // 4. Rendu des Nœuds (points sphériques rouges avec lueur)
    for (const auto& [id, pt] : projectedNodes)
    {
        // Lueur externe
        painter.setBrush(QColor(239, 68, 68, 80));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(pt.x, pt.y), 7.0, 7.0);

        // Point central
        painter.setBrush(QColor(248, 113, 113));
        painter.setPen(QPen(QColor(185, 28, 28), 1.2));
        painter.drawEllipse(QPointF(pt.x, pt.y), 4.5, 4.5);

        // Reflet spéculaire
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(pt.x - 1.2, pt.y - 1.2), 1.5, 1.5);
    }

    // 5. Badge et informations du modèle dans les angles
    // Angle inférieur droit : Badge TSA
    int badgeW = 90, badgeH = 28;
    int badgeX = width - badgeW - 16, badgeY = height - badgeH - 16;
    QLinearGradient badgeGrad(badgeX, badgeY, badgeX + badgeW, badgeY + badgeH);
    badgeGrad.setColorAt(0.0, QColor(124, 58, 237));
    badgeGrad.setColorAt(1.0, QColor(74, 0, 224));
    painter.setBrush(badgeGrad);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(badgeX, badgeY, badgeW, badgeH, 6, 6);

    if (hasGuiApp)
    {
        painter.setPen(Qt::white);
        QFont badgeFont = painter.font();
        badgeFont.setPointSize(10);
        badgeFont.setBold(true);
        painter.setFont(badgeFont);
        painter.drawText(QRect(badgeX, badgeY, badgeW, badgeH), Qt::AlignCenter, "TSA 3D");

        // Angle supérieur gauche : statistiques du modèle
        painter.setPen(QColor(148, 163, 184));
        QFont infoFont = painter.font();
        infoFont.setPointSize(9);
        infoFont.setBold(false);
        painter.setFont(infoFont);
        QString statsText = QString("%1 Nœuds • %2 Éléments")
            .arg(nodesMap.size())
            .arg(model.beams().size() + model.columns().size() + model.slabs().size());
        painter.drawText(16, 24, statsText);
    }

    return img;
}

QByteArray TSAPreviewGenerator::generatePngData(const TSA::Model::Model& model,
                                                int width,
                                                int height,
                                                const QImage& directCapture)
{
    QImage img = generateThumbnail(model, width, height, directCapture);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return bytes;
}

} // namespace TSA::IO
