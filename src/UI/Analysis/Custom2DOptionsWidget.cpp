#include "Custom2DOptionsWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>

namespace TSA::UI
{

Custom2DOptionsWidget::Custom2DOptionsWidget(QWidget* parent)
    : AnalysisEngineOptionsWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* group = new QGroupBox(tr("Méthode des déplacements"), this);
    auto* form = new QFormLayout(group);
    m_inextensible = new QCheckBox(tr("Barres inextensibles (hypothèse de la méthode des rotations)"), group);
    m_inextensible->setToolTip(tr("Néglige le raccourcissement des barres (EA multiplié par 10⁴), comme la méthode des "
                                  "rotations classique. Décoché : effort normal et déformation axiale réels."));
    form->addRow(m_inextensible);
    m_curvePoints = new QSpinBox(group);
    m_curvePoints->setRange(3, 2001);
    m_curvePoints->setToolTip(tr("Points calculés par barre pour les courbes N, V, M et la déformée "
                                 "(les extrema, zéros et discontinuités sont toujours ajoutés)."));
    form->addRow(tr("Points par courbe :"), m_curvePoints);
    lay->addWidget(group);
    loadSettings({});
}

void Custom2DOptionsWidget::loadSettings(const QJsonObject& s)
{
    m_inextensible->setChecked(s.value("inextensible").toBool(false));
    m_curvePoints->setValue(s.value("curvePoints").toInt(41));
}

QJsonObject Custom2DOptionsWidget::saveSettings() const
{
    return QJsonObject { { "inextensible", m_inextensible->isChecked() }, { "curvePoints", m_curvePoints->value() } };
}

} // namespace TSA::UI
