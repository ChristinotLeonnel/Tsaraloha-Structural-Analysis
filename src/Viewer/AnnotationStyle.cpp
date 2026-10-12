#include "AnnotationStyle.h"

#include <QSettings>

#include <algorithm>
#include <cmath>

namespace TSA::Viewer
{

AnnotationStyle& AnnotationStyle::instance()
{
    static AnnotationStyle s;
    return s;
}

AnnotationStyle::AnnotationStyle()
{
    load();
}

void AnnotationStyle::commit(const AnnotationSettings& s)
{
    if (s == m_settings) return;
    m_settings = s;
    save();
    emit changed();
}

void AnnotationStyle::setMode(AnnotationSizeMode mode)
{
    auto s = m_settings;
    s.mode = mode;
    commit(s);
}

void AnnotationStyle::setBasePixels(double px)
{
    auto s = m_settings;
    s.basePixels = std::clamp(std::isfinite(px) ? px : kDefaultPixels, kMinPixels, kMaxPixels);
    commit(s);
}

void AnnotationStyle::setModelHeight(double metres)
{
    auto s = m_settings;
    s.modelHeight = (std::isfinite(metres) && metres > 0.0) ? metres : 0.20;
    commit(s);
}

void AnnotationStyle::setDevicePixelRatio(double ratio)
{
    auto s = m_settings;
    s.devicePixelRatio = (std::isfinite(ratio) && ratio > 0.0) ? ratio : 1.0;
    // Facteur d'écran : propriété du poste, non enregistrée ; pas de reconstruction si inchangé
    if (s == m_settings) return;
    m_settings = s;
    emit changed();
}

void AnnotationStyle::reset()
{
    AnnotationSettings s;
    s.devicePixelRatio = m_settings.devicePixelRatio;
    commit(s);
}

void AnnotationStyle::adaptTo(int viewHeightPx, double modelSpan)
{
    auto s = m_settings;
    // 12 px pour une vue d'environ 800 px de haut ; proportionnel, borné
    if (viewHeightPx > 0) s.basePixels = std::clamp(12.0 * viewHeightPx / 800.0, 10.0, 18.0);
    // Mode lié au modèle : texte de base ≈ 1/60 de l'envergure
    if (modelSpan > 0.0 && std::isfinite(modelSpan)) s.modelHeight = std::clamp(modelSpan / 60.0, 0.02, 5.0);
    commit(s);
}

double AnnotationStyle::roleFactor(AnnotationRole role)
{
    switch (role)
    {
    case AnnotationRole::NodeLabel: return 1.0;
    case AnnotationRole::SupportLabel: return 1.0;
    case AnnotationRole::LoadValue: return 1.0;
    case AnnotationRole::DiagramValue: return 1.0;
    case AnnotationRole::ReactionValue: return 1.0;
    case AnnotationRole::Extremum: return 1.25;
    case AnnotationRole::Legend: return 13.0 / 12.0;
    case AnnotationRole::GridLabel: return 13.0 / 12.0;
    case AnnotationRole::GridLabelBold: return 15.0 / 12.0;
    }
    return 1.0;
}

double AnnotationStyle::height(AnnotationRole role) const
{
    const double f = roleFactor(role);
    if (m_settings.mode == AnnotationSizeMode::ModelLinked) return m_settings.modelHeight * f;
    return m_settings.basePixels * f * m_settings.devicePixelRatio;
}

void AnnotationStyle::apply(const Handle(AIS_TextLabel)& label, AnnotationRole role) const
{
    if (label.IsNull()) return;
    label->SetZoomable(m_settings.mode == AnnotationSizeMode::ModelLinked);
    label->SetHeight(height(role));
}

void AnnotationStyle::load()
{
    QSettings q;
    m_settings.mode = q.value("Display/Annotations/modelLinked", false).toBool() ? AnnotationSizeMode::ModelLinked : AnnotationSizeMode::ScreenConstant;
    m_settings.basePixels = std::clamp(q.value("Display/Annotations/basePixels", kDefaultPixels).toDouble(), kMinPixels, kMaxPixels);
    m_settings.modelHeight = std::max(0.001, q.value("Display/Annotations/modelHeight", 0.20).toDouble());
}

void AnnotationStyle::save() const
{
    QSettings q;
    q.setValue("Display/Annotations/modelLinked", m_settings.mode == AnnotationSizeMode::ModelLinked);
    q.setValue("Display/Annotations/basePixels", m_settings.basePixels);
    q.setValue("Display/Annotations/modelHeight", m_settings.modelHeight);
}

} // namespace TSA::Viewer
