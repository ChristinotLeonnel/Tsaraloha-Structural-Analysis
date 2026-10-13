#pragma once

// Affichage des objets produits par un Blueprint (composant partagé, couche widgets).
//
// Un module enregistre une vue par type d'objet (TSALab : tableau d'une matrice, diagrammes d'un élément, portique et
// déformée d'un résultat) ; à défaut, le contenu texte de l'objet (ObjectValue::details) est affiché. L'éditeur de
// Blueprints ouvre ces vues depuis les valeurs produites (double-clic). Documentation et source d'un nœud pré-codé :
// showNodeDocumentation.

#include "Blueprint/BlueprintTypes.h"

#include <QString>

#include <functional>
#include <string>

class QWidget;

namespace TSA::UI
{

using ObjectViewerFactory = std::function<QWidget*(const TSA::Blueprint::ObjectRef& object, QWidget* parent)>;

/// Vue d'un type d'objet (« Matrix »…) ; remplace une vue déjà enregistrée pour ce type.
void registerObjectViewer(const std::string& typeName, ObjectViewerFactory factory);
bool hasObjectViewer(const std::string& typeName);

/// Fenêtre non modale affichant l'objet : vue enregistrée pour son type, sinon son contenu texte (copiable).
void showObjectViewer(const TSA::Blueprint::ObjectRef& object, const QString& title, QWidget* parent);

/// Chemin absolu d'un fichier source cité par NodeDefinition::source (relatif à la racine des dépôts), cherché à partir
/// du dossier de l'application et de ses parents ; vide si les sources ne sont pas présentes (paquet distribué).
QString resolveSourcePath(const std::string& relativePath);

/// Fenêtre de documentation d'un nœud : description, statut, documentation détaillée, implémentation (avec le code
/// source quand il est présent sur le poste).
void showNodeDocumentation(const TSA::Blueprint::NodeDefinition& definition, QWidget* parent);

} // namespace TSA::UI
