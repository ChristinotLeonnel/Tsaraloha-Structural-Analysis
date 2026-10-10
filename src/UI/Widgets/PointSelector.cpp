#include "PointSelector.h"
#include "../Dialogs/NodeSelectionDialog.h"
#include "../../Model/Model.h"
#include "../../Model/Node.h"
#include "../../Viewer/OccView.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QRegularExpression>
#include <sstream>
#include <cmath>

namespace TSA::UI
{

PointSelector::~PointSelector()
{
    if (m_occView && m_occView->interactionManager())
    {
        blockSignals(true);
        m_occView->interactionManager()->cancelSelectionRequestFrom(this);
    }
}

PointSelector::PointSelector(QWidget* parent)
    : PointSelector(nullptr, nullptr, parent)
{
}

PointSelector::PointSelector(TSA::Model::Model* model, OccView* occView, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_occView(occView)
{
    setupUi();
}

void PointSelector::setupUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_editField = new QLineEdit(this);
    m_editField->setPlaceholderText(tr("(X, Y, Z) ou N<id>"));
    m_editField->setClearButtonEnabled(true);
    connect(m_editField, &QLineEdit::textEdited, this, &PointSelector::onTextEdited);
    connect(m_editField, &QLineEdit::returnPressed, this, &PointSelector::onReturnPressed);

    m_btnPick3D = new QToolButton(this);
    m_btnPick3D->setText(tr("📍 3D"));
    m_btnPick3D->setToolTip(tr("Sélectionner directement dans la vue 3D avec accrochage (Node Snap prioritaire)"));
    m_btnPick3D->setCheckable(true);
    m_btnPick3D->setCursor(Qt::PointingHandCursor);
    connect(m_btnPick3D, &QToolButton::clicked, this, [this](bool checked) {
        if (checked)
        {
            start3DPick();
        }
        else
        {
            cancel3DPick();
        }
    });

    m_btnSelectNode = new QToolButton(this);
    m_btnSelectNode->setText(tr("🔢 Nœud"));
    m_btnSelectNode->setToolTip(tr("Sélectionner un nœud existant dans la liste du modèle"));
    m_btnSelectNode->setCursor(Qt::PointingHandCursor);
    connect(m_btnSelectNode, &QToolButton::clicked, this, &PointSelector::openNodeDialog);

    layout->addWidget(m_editField, 1);
    layout->addWidget(m_btnPick3D);
    layout->addWidget(m_btnSelectNode);
}

void PointSelector::setModel(TSA::Model::Model* model)
{
    m_model = model;
    if (m_isValid && m_nodeId > 0 && m_model)
    {
        updateVisualState();
    }
}

void PointSelector::setOccView(OccView* occView)
{
    m_occView = occView;
}

void PointSelector::setPoint(const gp_Pnt& pt)
{
    m_point = pt;
    m_nodeId = -1;
    m_isValid = true;

    // Vérifier si un nœud existant correspond exactement à ces coordonnées
    if (m_model)
    {
        for (const auto& [nid, node] : m_model->nodes())
        {
            if (std::abs(node.x() - pt.X()) < 1e-4 &&
                std::abs(node.y() - pt.Y()) < 1e-4 &&
                std::abs(node.z() - pt.Z()) < 1e-4)
            {
                m_nodeId = nid;
                break;
            }
        }
    }

    updateVisualState();
    emit pointChanged(m_point, m_nodeId);
    if (m_nodeId > 0)
    {
        emit nodeSelected(m_nodeId);
    }
}

void PointSelector::setCoordinates(double x, double y, double z)
{
    setPoint(gp_Pnt(x, y, z));
}

void PointSelector::setNodeId(int nodeId)
{
    m_nodeId = nodeId;
    if (m_model && nodeId > 0)
    {
        const auto* node = m_model->getNode(nodeId);
        if (node)
        {
            m_point = gp_Pnt(node->x(), node->y(), node->z());
            m_isValid = true;
            updateVisualState();
            emit pointChanged(m_point, m_nodeId);
            emit nodeSelected(m_nodeId);
            return;
        }
    }

    if (nodeId <= 0)
    {
        m_nodeId = -1;
        updateVisualState();
    }
}

void PointSelector::clear()
{
    m_point = gp_Pnt(0.0, 0.0, 0.0);
    m_nodeId = -1;
    m_isValid = false;
    m_editField->clear();
    setInvalidAppearance(false);
    emit pointChanged(m_point, -1);
}

QString PointSelector::text() const
{
    return m_editField->text();
}

void PointSelector::setPlaceholderText(const QString& placeholder)
{
    m_editField->setPlaceholderText(placeholder);
}

void PointSelector::setReadOnly(bool readOnly)
{
    m_editField->setReadOnly(readOnly);
    m_btnPick3D->setEnabled(!readOnly);
    m_btnSelectNode->setEnabled(!readOnly);
}

void PointSelector::start3DPick()
{
    if (!m_occView)
    {
        m_btnPick3D->setChecked(false);
        return;
    }

    m_isPicking3D = true;
    m_btnPick3D->setChecked(true);
    m_btnPick3D->setText(tr("📍 Clic 3D..."));
    m_btnPick3D->setStyleSheet("background-color: #0d6efd; color: white; font-weight: bold; border-radius: 3px;");
    emit pickModeChanged(true);

    m_occView->pickPoint3D(
        [this](const gp_Pnt& pt, int detectedNodeId) {
            m_isPicking3D = false;
            m_btnPick3D->setChecked(false);
            m_btnPick3D->setText(tr("📍 3D"));
            m_btnPick3D->setStyleSheet("");
            emit pickModeChanged(false);

            if (detectedNodeId > 0)
            {
                setNodeId(detectedNodeId);
            }
            else
            {
                setPoint(pt);
            }
        },
        [this]() {
            m_isPicking3D = false;
            m_btnPick3D->setChecked(false);
            m_btnPick3D->setText(tr("📍 3D"));
            m_btnPick3D->setStyleSheet("");
            emit pickModeChanged(false);
        },
        this);   // requête annulée si le sélecteur (sa fenêtre) est détruit
}

void PointSelector::cancel3DPick()
{
    if (m_isPicking3D)
    {
        m_isPicking3D = false;
        m_btnPick3D->setChecked(false);
        m_btnPick3D->setText(tr("📍 3D"));
        m_btnPick3D->setStyleSheet("");
        emit pickModeChanged(false);

        if (m_occView && m_occView->interactionManager())
        {
            m_occView->interactionManager()->cancelSelectionRequest();
        }
    }
}

void PointSelector::openNodeDialog()
{
    if (!m_model)
        return;

    NodeSelectionDialog dlg(m_model, m_occView, this);
    if (m_nodeId > 0)
    {
        dlg.setSelectedNodeId(m_nodeId);
    }

    if (dlg.exec() == QDialog::Accepted)
    {
        int selId = dlg.selectedNodeId();
        if (selId > 0)
        {
            setNodeId(selId);
        }
    }
}

void PointSelector::onTextEdited(const QString& text)
{
    if (text.trimmed().isEmpty())
    {
        m_isValid = false;
        m_nodeId = -1;
        setInvalidAppearance(false);
        emit pointChanged(gp_Pnt(0, 0, 0), -1);
        return;
    }

    gp_Pnt parsedPt;
    int parsedNodeId = -1;
    if (parsePointString(text, m_model, parsedPt, parsedNodeId))
    {
        m_point = parsedPt;
        m_nodeId = parsedNodeId;
        m_isValid = true;
        setInvalidAppearance(false);
        emit pointChanged(m_point, m_nodeId);
        if (m_nodeId > 0)
        {
            emit nodeSelected(m_nodeId);
        }
    }
    else
    {
        m_isValid = false;
        setInvalidAppearance(true);
    }
}

void PointSelector::onReturnPressed()
{
    if (m_isValid)
    {
        updateVisualState();
    }
}

void PointSelector::updateVisualState()
{
    m_editField->blockSignals(true);
    m_editField->setText(formatPointString(m_point, m_nodeId, m_model));
    m_editField->blockSignals(false);
    setInvalidAppearance(false);
}

void PointSelector::setInvalidAppearance(bool invalid)
{
    if (invalid)
    {
        m_editField->setStyleSheet("QLineEdit { border: 1.5px solid #e05252; background-color: rgba(224, 82, 82, 0.08); }");
        m_editField->setToolTip(tr("Format invalide. Formats acceptés : (X, Y, Z), X; Y; Z ou N<id>"));
    }
    else
    {
        m_editField->setStyleSheet("");
        m_editField->setToolTip(m_nodeId > 0 ? tr("Nœud N%1 sélectionné").arg(m_nodeId) : tr("Point 3D"));
    }
}

} // namespace TSA::UI

