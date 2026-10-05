#include "ModelingToolDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace TSA::UI
{

using TSA::Interaction::ParamType;

namespace
{
QDoubleSpinBox* spin(QWidget* parent, double value, double min, double max, int decimals, const QString& suffix)
{
    auto* s = new QDoubleSpinBox(parent);
    s->setDecimals(decimals);
    s->setRange(min, max);
    s->setValue(value);
    s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    return s;
}
} // namespace

ModelingToolDialog::ModelingToolDialog(TSA::Interaction::ModelingTool& tool, QWidget* parent)
    : QDialog(parent)
    , m_tool(tool)
{
    setWindowTitle(QString::fromStdString(tool.name()));
    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel(QString::fromStdString(tool.description()), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* form = new QFormLayout();
    for (auto& p : tool.parameters())
    {
        const QString label = QString::fromStdString(p.label);
        const std::string key = p.key;
        switch (p.type)
        {
        case ParamType::Bool:
        {
            auto* c = new QCheckBox(this);
            c->setChecked(p.value > 0.5);
            form->addRow(label, c);
            m_writers.push_back([this, key, c] { m_tool.setParam(key, c->isChecked() ? 1.0 : 0.0); });
            break;
        }
        case ParamType::Count:
        {
            auto* s = new QSpinBox(this);
            s->setRange(static_cast<int>(std::max(p.min, -1e9)), static_cast<int>(std::min(p.max, 1e9)));
            s->setValue(static_cast<int>(p.value));
            form->addRow(label, s);
            m_writers.push_back([this, key, s] { m_tool.setParam(key, s->value()); });
            break;
        }
        case ParamType::Point:
        {
            auto* row = new QWidget(this);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            auto* x = spin(row, p.point.X(), -1e6, 1e6, 3, QString());
            auto* y = spin(row, p.point.Y(), -1e6, 1e6, 3, QString());
            auto* z = spin(row, p.point.Z(), -1e6, 1e6, 3, QString());
            x->setPrefix("X "); y->setPrefix("Y "); z->setPrefix("Z ");
            h->addWidget(x); h->addWidget(y); h->addWidget(z);
            form->addRow(label, row);
            m_writers.push_back([this, key, x, y, z] { m_tool.setPointParam(key, gp_Pnt(x->value(), y->value(), z->value())); });
            break;
        }
        default:
        {
            const int decimals = p.type == ParamType::Angle ? 2 : (p.type == ParamType::Factor ? 4 : 3);
            const QString suffix = p.type == ParamType::Angle ? QStringLiteral(" °") : (p.type == ParamType::Length ? QStringLiteral(" m") : QString());
            auto* s = spin(this, p.value, std::max(p.min, -1e9), std::min(p.max, 1e9), decimals, suffix);
            form->addRow(label, s);
            m_writers.push_back([this, key, s] { m_tool.setParam(key, s->value()); });
            break;
        }
        }
    }
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Appliquer"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { commit(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void ModelingToolDialog::commit()
{
    for (auto& w : m_writers) w();
}

} // namespace TSA::UI
