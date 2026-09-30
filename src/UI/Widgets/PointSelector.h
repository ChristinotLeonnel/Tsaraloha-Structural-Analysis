#pragma once

#include <QWidget>
#include <gp_Pnt.hxx>
#include <QString>
#include <QRegularExpression>
#include <sstream>
#include <cmath>
#include "../../Model/Model.h"
#include "../../Model/Node.h"

class QLineEdit;
class QToolButton;

class OccView;

namespace TSA::UI
{

/**
 * @brief Composant centralisé et réutilisable pour la sélection et la saisie de coordonnées 3D ou de nœuds.
 * 
 * Supporte trois modes d'entrée :
 * - Mode A : Saisie manuelle directe : (X, Y, Z), X; Y; Z ou N<id>
 * - Mode B : Sélection interactive 3D au curseur avec le moteur de snapping complet (Node Snap prioritaire)
 *            (Ne crée AUCUN nœud dans le modèle lors du pick)
 * - Mode C : Sélection dans la table des nœuds existants du modèle via NodeSelectionDialog
 */
class PointSelector : public QWidget
{
    Q_OBJECT

public:
    explicit PointSelector(QWidget* parent = nullptr);
    explicit PointSelector(TSA::Model::Model* model, OccView* occView = nullptr, QWidget* parent = nullptr);
    ~PointSelector() override = default;

    void setModel(TSA::Model::Model* model);
    void setOccView(OccView* occView);

    void setPoint(const gp_Pnt& pt);
    void setCoordinates(double x, double y, double z);
    void setNodeId(int nodeId);
    void clear();

    gp_Pnt point() const noexcept { return m_point; }
    double x() const noexcept { return m_point.X(); }
    double y() const noexcept { return m_point.Y(); }
    double z() const noexcept { return m_point.Z(); }
    int nodeId() const noexcept { return m_nodeId; }
    bool isNode() const noexcept { return m_nodeId > 0; }
    bool isValid() const noexcept { return m_isValid; }

    QString text() const;
    void setPlaceholderText(const QString& placeholder);
    void setReadOnly(bool readOnly);

    // Analyse syntaxique et formatage statiques (réutilisables et testables unitairement)
    static inline bool parsePointString(const QString& text, const TSA::Model::Model* model, gp_Pnt& outPoint, int& outNodeId)
    {
        QString s = text.trimmed();
        if (s.isEmpty()) return false;

        // 1. Détection syntaxe de nœud direct : "N25", "n25" ou entier seul
        static const QRegularExpression nodeRegex(R"(^[Nn]?\s*(\d+)$)");
        auto match = nodeRegex.match(s);
        if (match.hasMatch())
        {
            int nid = match.captured(1).toInt();
            if (model)
            {
                const auto* node = model->getNode(nid);
                if (node)
                {
                    outNodeId = nid;
                    outPoint = gp_Pnt(node->x(), node->y(), node->z());
                    return true;
                }
            }
        }

        // 2. Détection coordonnées : nettoyage parenthèses / crochets
        QString clean = s;
        clean.remove('(').remove(')').remove('[').remove(']');
        clean = clean.trimmed();

        double x = 0.0, y = 0.0, z = 0.0;

        // Format avec point-virgule (ex: "1250,5; 3500; 3000" ou "1250.5; 3500; 3000")
        if (clean.contains(';'))
        {
            QStringList parts = clean.split(';', Qt::SkipEmptyParts);
            if (parts.size() == 3)
            {
                bool ok1 = false, ok2 = false, ok3 = false;
                x = parts[0].trimmed().replace(',', '.').toDouble(&ok1);
                y = parts[1].trimmed().replace(',', '.').toDouble(&ok2);
                z = parts[2].trimmed().replace(',', '.').toDouble(&ok3);
                if (ok1 && ok2 && ok3)
                {
                    outPoint = gp_Pnt(x, y, z);
                    outNodeId = -1;
                    if (model)
                    {
                        for (const auto& [nid, node] : model->nodes())
                        {
                            if (std::abs(node.x() - x) < 1e-4 &&
                                std::abs(node.y() - y) < 1e-4 &&
                                std::abs(node.z() - z) < 1e-4)
                            {
                                outNodeId = nid;
                                break;
                            }
                        }
                    }
                    return true;
                }
            }
        }

        // Format avec virgules (séparateur standard avec points décimaux : "1250.5, 3500.0, 3000.0")
        if (clean.contains(','))
        {
            QStringList parts = clean.split(',', Qt::SkipEmptyParts);
            if (parts.size() == 3)
            {
                bool ok1 = false, ok2 = false, ok3 = false;
                x = parts[0].trimmed().toDouble(&ok1);
                y = parts[1].trimmed().toDouble(&ok2);
                z = parts[2].trimmed().toDouble(&ok3);
                if (ok1 && ok2 && ok3)
                {
                    outPoint = gp_Pnt(x, y, z);
                    outNodeId = -1;
                    if (model)
                    {
                        for (const auto& [nid, node] : model->nodes())
                        {
                            if (std::abs(node.x() - x) < 1e-4 &&
                                std::abs(node.y() - y) < 1e-4 &&
                                std::abs(node.z() - z) < 1e-4)
                            {
                                outNodeId = nid;
                                break;
                            }
                        }
                    }
                    return true;
                }
            }
        }

        // Format séparé par espaces (ex: "1250.0 3500.0 3000.0")
        std::string str = clean.toStdString();
        for (char& c : str)
        {
            if (c == ',' || c == ';') c = ' ';
        }
        std::istringstream iss(str);
        if (iss >> x >> y >> z)
        {
            outPoint = gp_Pnt(x, y, z);
            outNodeId = -1;
            if (model)
            {
                for (const auto& [nid, node] : model->nodes())
                {
                    if (std::abs(node.x() - x) < 1e-4 &&
                        std::abs(node.y() - y) < 1e-4 &&
                        std::abs(node.z() - z) < 1e-4)
                    {
                        outNodeId = nid;
                        break;
                    }
                }
            }
            return true;
        }

        return false;
    }

    static inline QString formatPointString(const gp_Pnt& pt, int nodeId = -1, const TSA::Model::Model* model = nullptr)
    {
        QString coordsStr = QString("(%1, %2, %3)")
            .arg(pt.X(), 0, 'f', 2)
            .arg(pt.Y(), 0, 'f', 2)
            .arg(pt.Z(), 0, 'f', 2);

        if (nodeId > 0)
        {
            if (model)
            {
                const auto* node = model->getNode(nodeId);
                if (node && !node->name().empty() && node->name() != QString("N%1").arg(nodeId).toStdString())
                {
                    return QString("N%1 [%2]  %3").arg(nodeId).arg(QString::fromStdString(node->name())).arg(coordsStr);
                }
            }
            return QString("N%1  %2").arg(nodeId).arg(coordsStr);
        }

        return coordsStr;
    }

signals:
    void pointChanged(const gp_Pnt& pt, int nodeId);
    void nodeSelected(int nodeId);
    void pickModeChanged(bool active);

public slots:
    void start3DPick();
    void cancel3DPick();
    void openNodeDialog();

private slots:
    void onTextEdited(const QString& text);
    void onReturnPressed();

private:
    void setupUi();
    void updateVisualState();
    void setInvalidAppearance(bool invalid);

private:
    TSA::Model::Model* m_model = nullptr;
    OccView* m_occView = nullptr;

    gp_Pnt m_point = gp_Pnt(0.0, 0.0, 0.0);
    int m_nodeId = -1;
    bool m_isValid = false;
    bool m_isPicking3D = false;

    QLineEdit* m_editField = nullptr;
    QToolButton* m_btnPick3D = nullptr;
    QToolButton* m_btnSelectNode = nullptr;
};

} // namespace TSA::UI
