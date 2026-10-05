#include "ElementResultsPanel.h"

#include "../../Analysis/ResultsModel.h"

#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace TSA::UI
{

using namespace TSA::Analysis;

ElementResultsPanel::ElementResultsPanel(QWidget* parent)
    : QGroupBox(tr("Résultats d'analyse"), parent)
{
    auto* lay = new QVBoxLayout(this);
    m_text = new QLabel(this);
    m_text->setTextFormat(Qt::RichText);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_text->setWordWrap(true);
    lay->addWidget(m_text);
    setVisible(false);
}

void ElementResultsPanel::setResultsModel(const std::shared_ptr<ResultsModel>& results)
{
    m_results = results;
    refresh();
}

void ElementResultsPanel::showElement(const std::optional<ElementKey>& key)
{
    m_key = key;
    refresh();
}

void ElementResultsPanel::refresh()
{
    const ElementResults* er = (m_key && m_results && m_results->hasResults() && m_results->availability().elementForces)
                                   ? m_results->getElementResults(*m_key)
                                   : nullptr;
    setVisible(er != nullptr);
    if (!er) return;

    const auto& meta = m_results->executionMetadata();
    const auto& u = m_results->units();
    const QString F = QString::fromStdString(u.force), M = QString::fromStdString(u.moment);

    QString engine = QString::fromStdString(meta.solverEngine);
    if (!meta.solverVersion.empty()) engine += QStringLiteral(" ") + QString::fromStdString(meta.solverVersion);
    QString html = tr("<b>%1</b><br>Moteur d'analyse : %2").arg(QString::fromStdString(m_key->label()), engine.toHtmlEscaped());
    if (!meta.analysisScope.empty())
        html += tr("<br>Portée : %1").arg(QString::fromStdString(meta.analysisScope).toHtmlEscaped());
    if (!m_results->caseOrComboName().empty())
        html += tr("<br>Chargement : %1").arg(QString::fromStdString(m_results->caseOrComboName()).toHtmlEscaped());

    auto cell = [](double v) { return QString::number(v, 'f', 2); };
    auto row = [&](const QString& name, double a, double b) {
        return QStringLiteral("<tr><td>%1</td><td align='right'>%2</td><td align='right'>%3</td></tr>").arg(name, cell(a), cell(b));
    };
    const auto& s = er->startForces;
    const auto& e = er->endForces;
    html += QStringLiteral("<table cellspacing='6'><tr><th></th><th>%1</th><th>%2</th></tr>").arg(tr("Début"), tr("Fin"));
    html += row(tr("N (%1)").arg(F), s.N, e.N);
    html += row(tr("Vy (%1)").arg(F), s.Vy, e.Vy);
    html += row(tr("Vz (%1)").arg(F), s.Vz, e.Vz);
    html += row(tr("Mx (%1)").arg(M), s.Mx, e.Mx);
    html += row(tr("My (%1)").arg(M), s.My, e.My);
    html += row(tr("Mz (%1)").arg(M), s.Mz, e.Mz);
    html += QStringLiteral("</table>");
    html += tr("Max |N| = %1 %2 · max |V| = %3 %2 · max |M| = %4 %5")
                .arg(cell(std::max(std::abs(er->maxNormalForce()), std::abs(er->minNormalForce()))), F,
                     cell(er->maxShearForce()), cell(er->maxBendingMoment()), M);
    m_text->setText(html);
}

} // namespace TSA::UI
