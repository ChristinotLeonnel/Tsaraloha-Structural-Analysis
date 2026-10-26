#pragma once

// Textes à rectangle de fond (Aspect_TODT_SUBTITLE) et contextes OpenGL sans fonctions OpenGL 2.0 (BUG-122).
//
// OCCT 8.0.1 dessine le rectangle de fond d'un texte par OpenGl_Text::drawRect, qui appelle
// theCtx->core20fwd->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4) sans vérifier que la table core20fwd existe
// (OpenGl_Text.cxx:649). Avec un contexte inférieur à OpenGL 2.0 (pilote logiciel « GDI Generic » de Windows : postes
// virtuels, bureau à distance, runners de CI), core20fwd est nul : violation d'accès au premier texte de ce type
// (marqueur d'accrochage, cotations). Sans core20fwd, ces textes passent en Aspect_TODT_DEKALE : texte entouré de la
// couleur du fond (lisible), dessiné par drawText, qui n'utilise que core11fwd (OpenGl_Text.cxx:520).

#include <Aspect_TypeOfDisplayText.hxx>

namespace TSA::Viewer
{

/// Type d'affichage d'un texte « sur fond » : rectangle de fond si le contexte OpenGL actif fournit la table
/// core20fwd, contour sinon.
inline Aspect_TypeOfDisplayText backgroundTextDisplayType(bool contextHasCore20)
{
    return contextHasCore20 ? Aspect_TODT_SUBTITLE : Aspect_TODT_DEKALE;
}

} // namespace TSA::Viewer
