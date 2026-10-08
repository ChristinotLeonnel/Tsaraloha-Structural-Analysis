#pragma once

// Points d'extension du produit compilé (TSA, TSALab).
// Déclarés ici (base commune), définis par chaque produit dans product/ProductHooks.cpp.

namespace TSA::UI
{
class AppShell;
}

namespace TSA::Product
{

/// Appelé une fois au lancement, après la construction de la fenêtre et avant son affichage :
/// panneau de lancement du Start Center, page du workspace, actions propres au produit.
void configureShell(TSA::UI::AppShell& shell);

} // namespace TSA::Product
