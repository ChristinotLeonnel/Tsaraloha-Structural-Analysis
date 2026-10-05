#pragma once

// Panneaux d'options propres à un moteur d'analyse. La fenêtre Analysis ne connaît que cette
// interface : un moteur qui a des options enregistre une fabrique de panneau ; un moteur sans
// option n'enregistre rien (la fenêtre l'indique). Les réglages transitent en JSON
// (AnalysisContext::engineSettings), interprétés par le seul adaptateur du moteur.

#include "../../Analysis/Engine/AnalysisContext.h"

#include <QJsonObject>
#include <QWidget>

#include <functional>
#include <map>

namespace TSA::UI
{

class AnalysisEngineOptionsWidget : public QWidget
{
public:
    using QWidget::QWidget;
    ~AnalysisEngineOptionsWidget() override = default;

    virtual void loadSettings(const QJsonObject& settings) = 0;
    virtual QJsonObject saveSettings() const = 0;
    /// Réglages communs modifiés (type d'analyse…) : afficher / masquer les groupes concernés.
    virtual void setAnalysisContext(const TSA::Analysis::AnalysisContext& /*context*/) {}
};

class AnalysisEngineOptionsRegistry
{
public:
    using Factory = std::function<AnalysisEngineOptionsWidget*(QWidget* parent)>;

    void registerFactory(const TSA::Analysis::EngineId& id, Factory factory) { m_factories[id] = std::move(factory); }
    bool hasOptions(const TSA::Analysis::EngineId& id) const { return m_factories.count(id) > 0; }
    /// nullptr si le moteur n'a pas d'options propres. Le panneau appartient à parent.
    AnalysisEngineOptionsWidget* create(const TSA::Analysis::EngineId& id, QWidget* parent) const
    {
        auto it = m_factories.find(id);
        return it != m_factories.end() ? it->second(parent) : nullptr;
    }

private:
    std::map<TSA::Analysis::EngineId, Factory> m_factories;
};

/// Panneaux d'options des moteurs intégrés. SEUL endroit UI à modifier pour un nouveau moteur.
void registerBuiltInEngineOptions(AnalysisEngineOptionsRegistry& registry);

} // namespace TSA::UI
