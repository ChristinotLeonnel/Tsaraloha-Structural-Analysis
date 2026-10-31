#include "AxisDisplayStyle.h"

#include <QSettings>

#include <algorithm>
#include <cmath>

namespace TSA::Viewer
{

namespace
{
AxisDisplaySettings bounded(AxisDisplaySettings s)
{
    const AxisDisplaySettings defaults;
    s.axisPixels = std::clamp(std::isfinite(s.axisPixels) ? s.axisPixels : defaults.axisPixels, AxisDisplayStyle::kMinAxisPixels,
                              AxisDisplayStyle::kMaxAxisPixels);
    s.labelPixels = std::clamp(std::isfinite(s.labelPixels) ? s.labelPixels : defaults.labelPixels, AxisDisplayStyle::kMinLabelPixels,
                               AxisDisplayStyle::kMaxLabelPixels);
    return s;
}
} // namespace

AxisDisplayStyle& AxisDisplayStyle::instance()
{
    static AxisDisplayStyle s;
    return s;
}

AxisDisplayStyle::AxisDisplayStyle()
{
    load();
}

void AxisDisplayStyle::setSettings(const AxisDisplaySettings& settings)
{
    const AxisDisplaySettings s = bounded(settings);
    if (s == m_settings) return;
    m_settings = s;
    save();
    emit changed();
}

void AxisDisplayStyle::setGlobalAxes(bool on)
{
    auto s = m_settings;
    s.globalAxes = on;
    setSettings(s);
}

void AxisDisplayStyle::setCornerIndicator(bool on)
{
    auto s = m_settings;
    s.cornerIndicator = on;
    setSettings(s);
}

void AxisDisplayStyle::setLocalAxes(bool on)
{
    auto s = m_settings;
    s.localAxes = on;
    setSettings(s);
}

void AxisDisplayStyle::setLetters(bool on)
{
    auto s = m_settings;
    s.letters = on;
    setSettings(s);
}

void AxisDisplayStyle::setAxisPixels(double px)
{
    auto s = m_settings;
    s.axisPixels = px;
    setSettings(s);
}

void AxisDisplayStyle::setLabelPixels(double px)
{
    auto s = m_settings;
    s.labelPixels = px;
    setSettings(s);
}

void AxisDisplayStyle::setAnchor(LocalAxisAnchor anchor)
{
    auto s = m_settings;
    s.anchor = anchor;
    setSettings(s);
}

void AxisDisplayStyle::reset()
{
    setSettings(AxisDisplaySettings {});
}

void AxisDisplayStyle::load()
{
    QSettings q;
    const AxisDisplaySettings d;
    AxisDisplaySettings s;
    s.globalAxes = q.value("Display/Axes/global", d.globalAxes).toBool();
    s.cornerIndicator = q.value("Display/Axes/corner", d.cornerIndicator).toBool();
    s.localAxes = q.value("Display/Axes/local", d.localAxes).toBool();
    s.letters = q.value("Display/Axes/letters", d.letters).toBool();
    s.axisPixels = q.value("Display/Axes/axisPixels", d.axisPixels).toDouble();
    s.labelPixels = q.value("Display/Axes/labelPixels", d.labelPixels).toDouble();
    s.anchor = q.value("Display/Axes/atSelectedLocation", false).toBool() ? LocalAxisAnchor::SelectedLocation : LocalAxisAnchor::ElementCenter;
    m_settings = bounded(s);
}

void AxisDisplayStyle::save() const
{
    QSettings q;
    q.setValue("Display/Axes/global", m_settings.globalAxes);
    q.setValue("Display/Axes/corner", m_settings.cornerIndicator);
    q.setValue("Display/Axes/local", m_settings.localAxes);
    q.setValue("Display/Axes/letters", m_settings.letters);
    q.setValue("Display/Axes/axisPixels", m_settings.axisPixels);
    q.setValue("Display/Axes/labelPixels", m_settings.labelPixels);
    q.setValue("Display/Axes/atSelectedLocation", m_settings.anchor == LocalAxisAnchor::SelectedLocation);
}

} // namespace TSA::Viewer
