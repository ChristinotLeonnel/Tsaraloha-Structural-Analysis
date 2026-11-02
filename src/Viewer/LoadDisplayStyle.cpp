#include "LoadDisplayStyle.h"

#include <QLocale>
#include <QSettings>

#include <algorithm>
#include <array>
#include <cmath>

namespace TSA::Viewer
{

LoadDisplayStyle::LoadDisplayStyle()
{
    load();
}

LoadDisplayStyle& LoadDisplayStyle::instance()
{
    static LoadDisplayStyle style;
    return style;
}

LoadDisplaySettings LoadDisplayStyle::bounded(LoadDisplaySettings s)
{
    s.textHeight = std::isfinite(s.textHeight) ? std::max(0.0, s.textHeight) : 0.0;
    s.textPixels = std::clamp(std::isfinite(s.textPixels) ? s.textPixels : 12.0, kMinTextPixels, kMaxTextPixels);
    s.textMinPixels = std::clamp(std::isfinite(s.textMinPixels) ? s.textMinPixels : 9.0, kMinTextPixels, kMaxTextPixels);
    s.textMaxPixels = std::clamp(std::isfinite(s.textMaxPixels) ? s.textMaxPixels : 16.0, s.textMinPixels, kMaxTextPixels);
    s.decimals = std::clamp(s.decimals, 0, 4);
    s.arrowWidth = std::clamp(std::isfinite(s.arrowWidth) ? s.arrowWidth : 1.0, kMinArrowWidth, kMaxArrowWidth);
    if (!s.singleColor.isValid()) s.singleColor = QColor(0xD0, 0x30, 0x30);
    return s;
}

void LoadDisplayStyle::setSettings(const LoadDisplaySettings& settings)
{
    const LoadDisplaySettings s = bounded(settings);
    if (s == m_settings) return;
    m_settings = s;
    save();
    emit changed();
}

void LoadDisplayStyle::reset()
{
    setSettings(LoadDisplaySettings {});
}

bool LoadDisplayStyle::caseShown(int caseId, int activeCaseId) const noexcept
{
    return m_settings.cases == LoadCaseFilter::AllCases || caseId == activeCaseId;
}

QColor LoadDisplayStyle::caseColor(int caseRank)
{
    // Couleurs distinctes et lisibles sur fond clair comme sombre (charges permanentes d'abord en brun-rouge).
    static const std::array<QColor, 10> palette { QColor(0xC0, 0x39, 0x2B), QColor(0x29, 0x80, 0xB9), QColor(0x27, 0xAE, 0x60),
                                                  QColor(0xE6, 0x7E, 0x22), QColor(0x8E, 0x44, 0xAD), QColor(0x16, 0xA0, 0x85),
                                                  QColor(0xD3, 0x54, 0x00), QColor(0x2C, 0x3E, 0x9E), QColor(0xB0, 0x3A, 0x8C),
                                                  QColor(0x7F, 0x8C, 0x0D) };
    const int n = static_cast<int>(palette.size());
    return palette[static_cast<std::size_t>(((caseRank % n) + n) % n)];
}

QColor LoadDisplayStyle::intensityColor(double ratio)
{
    const double t = std::clamp(std::isfinite(ratio) ? ratio : 0.0, 0.0, 1.0);
    // bleu (0,40,200) → vert (30,170,60) → orange (240,150,20) → rouge (210,30,30)
    struct Stop { double t; int r, g, b; };
    static const std::array<Stop, 4> stops { Stop { 0.0, 40, 90, 200 }, Stop { 0.35, 30, 170, 60 }, Stop { 0.7, 240, 150, 20 },
                                             Stop { 1.0, 210, 30, 30 } };
    for (std::size_t i = 1; i < stops.size(); ++i)
        if (t <= stops[i].t)
        {
            const Stop& a = stops[i - 1];
            const Stop& b = stops[i];
            const double u = (t - a.t) / (b.t - a.t);
            return QColor(int(std::lround(a.r + u * (b.r - a.r))), int(std::lround(a.g + u * (b.g - a.g))),
                          int(std::lround(a.b + u * (b.b - a.b))));
        }
    return QColor(stops.back().r, stops.back().g, stops.back().b);
}

QColor LoadDisplayStyle::familyColor(TSA::Geometry::LoadFamily family)
{
    using F = TSA::Geometry::LoadFamily;
    switch (family)
    {
    case F::Force: return QColor(0xD0, 0x30, 0x30);
    case F::Moment: return QColor(0xB0, 0x50, 0xE0);
    case F::LineForce: return QColor(0x00, 0x86, 0xB3);
    case F::LineMoment: return QColor(0x8E, 0x44, 0xAD);
    case F::Pressure: return QColor(0x27, 0xAE, 0x60);
    default: return QColor(0x80, 0x80, 0x80);
    }
}

QColor LoadDisplayStyle::loadColor(int caseRank, double ratio, TSA::Geometry::LoadFamily family) const
{
    switch (m_settings.colors)
    {
    case LoadColorMode::ByCase: return caseColor(caseRank);
    case LoadColorMode::ByIntensity: return intensityColor(ratio);
    case LoadColorMode::ByFamily: return familyColor(family);
    case LoadColorMode::Single: return m_settings.singleColor;
    }
    return m_settings.singleColor;
}

QString LoadDisplayStyle::formatValue(const QString& symbol, double value, const QString& unit, const QString& caseName) const
{
    QString text = QString::number(value, 'f', m_settings.decimals);
    if (m_settings.showSymbol && !symbol.isEmpty()) text = symbol + QStringLiteral(" = ") + text;
    if (m_settings.showUnits && !unit.isEmpty()) text += QLatin1Char(' ') + unit;
    if (m_settings.showCaseName && !caseName.isEmpty()) text += QStringLiteral(" [%1]").arg(caseName);
    return text;
}

double LoadDisplayStyle::modelTextHeight(double referenceLength) const
{
    if (m_settings.textHeight > 0.0) return m_settings.textHeight;
    const double ref = std::isfinite(referenceLength) && referenceLength > 0.0 ? referenceLength : 1.0;
    return std::clamp(0.25 * ref, 0.02, 2.0);
}

double LoadDisplayStyle::labelPixels(double pixelsPerMeter, double referenceLength, double dpr) const
{
    const double f = std::isfinite(dpr) && dpr > 0.0 ? dpr : 1.0;
    if (m_settings.textSize == LoadTextSize::ScreenConstant) return m_settings.textPixels * f;
    const double scale = std::isfinite(pixelsPerMeter) && pixelsPerMeter > 0.0 ? pixelsPerMeter : 0.0;
    return std::clamp(modelTextHeight(referenceLength) * scale, m_settings.textMinPixels * f, m_settings.textMaxPixels * f);
}

void LoadDisplayStyle::load()
{
    QSettings q;
    LoadDisplaySettings s;
    s.cases = q.value("Display/Loads/allCases", false).toBool() ? LoadCaseFilter::AllCases : LoadCaseFilter::ActiveCase;
    s.colors = static_cast<LoadColorMode>(std::clamp(q.value("Display/Loads/colorMode", 0).toInt(), 0, 3));
    s.singleColor = QColor(q.value("Display/Loads/singleColor", s.singleColor.name()).toString());
    s.textSize = q.value("Display/Loads/textScreenConstant", false).toBool() ? LoadTextSize::ScreenConstant : LoadTextSize::ModelLinked;
    s.textHeight = q.value("Display/Loads/textHeight", 0.0).toDouble();
    s.textPixels = q.value("Display/Loads/textPixels", 12.0).toDouble();
    s.textMinPixels = q.value("Display/Loads/textMinPixels", 9.0).toDouble();
    s.textMaxPixels = q.value("Display/Loads/textMaxPixels", 16.0).toDouble();
    s.declutter = q.value("Display/Loads/declutter", true).toBool();
    s.decimals = q.value("Display/Loads/decimals", 2).toInt();
    s.showSymbol = q.value("Display/Loads/showSymbol", true).toBool();
    s.showUnits = q.value("Display/Loads/showUnits", true).toBool();
    s.showCaseName = q.value("Display/Loads/showCaseName", false).toBool();
    s.textBackground = q.value("Display/Loads/textBackground", true).toBool();
    s.arrowWidth = q.value("Display/Loads/arrowWidth", 1.0).toDouble();
    m_settings = bounded(s);
}

void LoadDisplayStyle::save() const
{
    QSettings q;
    q.setValue("Display/Loads/allCases", m_settings.cases == LoadCaseFilter::AllCases);
    q.setValue("Display/Loads/colorMode", static_cast<int>(m_settings.colors));
    q.setValue("Display/Loads/singleColor", m_settings.singleColor.name());
    q.setValue("Display/Loads/textScreenConstant", m_settings.textSize == LoadTextSize::ScreenConstant);
    q.setValue("Display/Loads/textHeight", m_settings.textHeight);
    q.setValue("Display/Loads/textPixels", m_settings.textPixels);
    q.setValue("Display/Loads/textMinPixels", m_settings.textMinPixels);
    q.setValue("Display/Loads/textMaxPixels", m_settings.textMaxPixels);
    q.setValue("Display/Loads/declutter", m_settings.declutter);
    q.setValue("Display/Loads/decimals", m_settings.decimals);
    q.setValue("Display/Loads/showSymbol", m_settings.showSymbol);
    q.setValue("Display/Loads/showUnits", m_settings.showUnits);
    q.setValue("Display/Loads/showCaseName", m_settings.showCaseName);
    q.setValue("Display/Loads/textBackground", m_settings.textBackground);
    q.setValue("Display/Loads/arrowWidth", m_settings.arrowWidth);
}

} // namespace TSA::Viewer
