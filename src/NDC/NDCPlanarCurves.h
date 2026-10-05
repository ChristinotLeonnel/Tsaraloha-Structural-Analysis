#pragma once

// Chapitre « Courbes RDM par barre » de la note de calcul, pour un calcul plan (moteur 2D) :
// méthode et conventions, tableau des valeurs caractéristiques, puis pour chaque barre les
// courbes N(x), V(x), M(x) et la déformée v(x) avec leurs valeurs lues sur les courbes.

#include "NDCDocumentModel.h"

#include <QImage>

namespace TSA::Model
{
class Model;
}
namespace TSA::Analysis
{
class ResultsModel;
struct PlanarMemberCurves;
}

namespace TSA::NDC
{

/// Ajoute le chapitre si les résultats contiennent des courbes planes ; ne fait rien sinon.
void appendPlanarCurvesChapter(NDCDocument& doc, const TSA::Analysis::ResultsModel& results,
                               int& chapterNumber, int& tableNumber, int& figureNumber);

/// Diagrammes N, V, M et déformée d'une barre (image intégrée à la note).
QImage renderPlanarMemberCurves(const TSA::Analysis::PlanarMemberCurves& curves, const QString& title);

} // namespace TSA::NDC
