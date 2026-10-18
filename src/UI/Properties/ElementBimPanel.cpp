#include "ElementBimPanel.h"

#include "../../BIM/Core/ProductTypes.h"
#include "../../BIM/Core/QuantityTakeoff.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Model/Model.h"

#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

namespace TSA::UI
{

ElementBimPanel::ElementBimPanel(QWidget* parent)
    : QGroupBox(tr("Identité BIM et quantités"), parent)
{
    auto* lay = new QVBoxLayout(this);
    m_text = new QLabel(this);
    m_text->setTextFormat(Qt::RichText);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);   // GlobalId copiable
    m_text->setWordWrap(true);
    lay->addWidget(m_text);
    setVisible(false);
}

void ElementBimPanel::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refresh();
}

void ElementBimPanel::showElement(const std::optional<TSA::BIM::AnalyticalRef>& ref)
{
    m_ref = ref;
    refresh();
}

QString ElementBimPanel::text() const
{
    return m_text->text();
}

void ElementBimPanel::refresh()
{
    const auto q = (m_model && m_ref) ? TSA::BIM::elementQuantities(*m_model, *m_ref) : std::nullopt;
    if (!q)
    {
        m_text->clear();
        setVisible(false);
        return;
    }

    const auto& bim = m_model->bim();
    const auto* product = bim.physicalOf(*m_ref);
    auto esc = [](const std::string& s) { return QString::fromStdString(s).toHtmlEscaped(); };
    auto num = [](double v, int d) { return QString::number(v, 'f', d); };

    QString html;
    if (product)
    {
        const std::string name = product->name.empty() ? TSA::BIM::defaultProductName(*product) : product->name;
        html += tr("<b>%1</b> (produit P%2)").arg(esc(name)).arg(product->id);
        QString cls = QString::fromLatin1(TSA::BIM::ifcEntity(product->category));
        if (!product->predefinedType.empty()) cls += QStringLiteral(" · ") + esc(product->predefinedType);
        html += tr("<br>Classe IFC : %1").arg(cls);
        if (const auto* type = bim.type(product->typeId))
        {
            html += tr("<br>Type : %1").arg(esc(type->name));
            const std::string dev = TSA::BIM::typeDeviation(*m_model, *product);
            if (!dev.empty()) html += tr(" — <i>diffère du type : %1</i>").arg(esc(dev));
        }
        else html += tr("<br>Type : aucun");
        html += tr("<br>GlobalId : <code>%1</code>").arg(esc(product->globalId));
        const std::string storey = bim.resolvedStorey(*m_model, *product);
        const auto* lm = m_model->levelManager();
        const auto* level = (lm && !storey.empty()) ? lm->getLevel(storey) : nullptr;
        html += tr("<br>Étage : %1").arg(level ? esc(level->name) : tr("aucun niveau"));
        QStringList parts;
        for (const auto& r : product->analytical) parts << QString::fromStdString(r.label());
        html += tr("<br>Éléments de calcul : %1").arg(parts.join(QStringLiteral(", ")).toHtmlEscaped());
    }
    else
    {
        html += tr("<b>%1</b> : aucun produit physique").arg(esc(q->label));
    }
    const std::string analyticalGuid = bim.analyticalGlobalId(*m_ref);
    if (!analyticalGuid.empty())
        html += tr("<br>GlobalId analytique : <code>%1</code>").arg(esc(analyticalGuid));

    html += tr("<br><br><b>Quantités</b> (calculées depuis le modèle)<br>%1 · %2").arg(esc(q->material), esc(q->section));
    if (q->length > 0.0) html += tr("<br>Longueur : %1 m").arg(num(q->length, 3));
    if (q->grossArea > 0.0)
    {
        html += q->openingArea > 0.0
            ? tr("<br>Surface : %1 m² (brute %2 − ouvertures %3)").arg(num(q->area, 3), num(q->grossArea, 3), num(q->openingArea, 3))
            : tr("<br>Surface : %1 m²").arg(num(q->area, 3));
    }
    html += tr("<br>Volume : %1 m³").arg(num(q->volume, 4));
    html += tr("<br>Masse : %1 kg").arg(num(q->mass, 1));
    if (product && product->analytical.size() > 1)
    {
        const auto t = TSA::BIM::productQuantities(*m_model, *product);
        html += tr("<br>Produit entier (%1 éléments) : %2 m · %3 m³ · %4 kg")
                    .arg(t.count).arg(num(t.length, 3), num(t.volume, 4), num(t.mass, 1));
    }
    m_text->setText(html);
    setVisible(true);
}

} // namespace TSA::UI
