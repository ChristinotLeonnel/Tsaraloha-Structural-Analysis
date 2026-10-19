#include "Fem3DOptionsWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

namespace TSA::UI
{

Fem3DOptionsWidget::Fem3DOptionsWidget(QWidget* parent)
    : AnalysisEngineOptionsWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* group = new QGroupBox(tr("Moteur EF 3D Fem3D (moteur secondaire)"), this);
    auto* form = new QFormLayout(group);
    m_mode = new QComboBox(group);
    m_mode->addItem(tr("Fem3D seul"), QStringLiteral("fem3d"));
    m_mode->addItem(tr("Fem3D, comparé à OpenSees"), QStringLiteral("compare"));
    m_mode->setToolTip(tr("Comparaison : le modèle est aussi calculé par OpenSees. Les résultats affichés restent ceux de "
                          "Fem3D ; les écarts sont donnés dans une table et signalés au-delà de la tolérance."));
    form->addRow(tr("Calcul :"), m_mode);
    m_fallback = new QComboBox(group);
    m_fallback->addItem(tr("Refuser le calcul"), QStringLiteral("none"));
    m_fallback->addItem(tr("Calculer par OpenSees"), QStringLiteral("opensees"));
    m_fallback->setToolTip(tr("Modèle hors du périmètre de Fem3D (charge ponctuelle ou partielle sur barre, moment réparti…). "
                              "Un mécanisme ou une donnée invalide n'est jamais confié à un autre moteur."));
    form->addRow(tr("Hors périmètre :"), m_fallback);
    auto* note = new QLabel(tr("Un calcul confié à OpenSees est annoncé avant le calcul et ses résultats portent l'identité "
                               "d'OpenSees."), group);
    note->setWordWrap(true);
    form->addRow(note);
    lay->addWidget(group);
    loadSettings({});
}

void Fem3DOptionsWidget::loadSettings(const QJsonObject& s)
{
    m_mode->setCurrentIndex(std::max(0, m_mode->findData(s.value("mode").toString(QStringLiteral("fem3d")))));
    m_fallback->setCurrentIndex(std::max(0, m_fallback->findData(s.value("fallback").toString(QStringLiteral("none")))));
}

QJsonObject Fem3DOptionsWidget::saveSettings() const
{
    return QJsonObject { { "mode", m_mode->currentData().toString() }, { "fallback", m_fallback->currentData().toString() } };
}

} // namespace TSA::UI
