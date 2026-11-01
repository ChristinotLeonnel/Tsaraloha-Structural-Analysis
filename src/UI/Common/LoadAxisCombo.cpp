#include "LoadAxisCombo.h"

#include "../../Coordinate/AxisColorConfig.h"

#include <QColor>
#include <QCoreApplication>
#include <QSignalBlocker>

namespace TSA::UI
{

void fillLoadAxisCombo(QComboBox* combo, bool local, const std::array<int, 3>& values)
{
    const auto& axes = TSA::Coordinate::AxisColorConfig::instance();
    const std::array<Quantity_Color, 3> colors { axes.axisXColor(), axes.axisYColor(), axes.axisZColor() };
    const std::array<const char*, 3> globalNames { QT_TRANSLATE_NOOP("TSA::UI::LoadAxisCombo", "X global"), QT_TRANSLATE_NOOP("TSA::UI::LoadAxisCombo", "Y global"),
                                                   QT_TRANSLATE_NOOP("TSA::UI::LoadAxisCombo", "Z global") };
    const std::array<const char*, 3> localNames { QT_TRANSLATE_NOOP("TSA::UI::LoadAxisCombo", "x local"), QT_TRANSLATE_NOOP("TSA::UI::LoadAxisCombo", "y local"),
                                                  QT_TRANSLATE_NOOP("TSA::UI::LoadAxisCombo", "z local") };
    const QSignalBlocker block(combo);
    const int current = combo->currentIndex();
    combo->clear();
    for (int k = 0; k < 3; ++k)
    {
        const std::size_t i = static_cast<std::size_t>(k);
        combo->addItem(QCoreApplication::translate("TSA::UI::LoadAxisCombo", local ? localNames[i] : globalNames[i]), values[i]);
        const Quantity_Color& c = colors[i];
        combo->setItemData(k, QColor::fromRgbF(float(c.Red()), float(c.Green()), float(c.Blue())), Qt::ForegroundRole);
    }
    combo->setCurrentIndex(current >= 0 ? current : 2);
}

} // namespace TSA::UI
