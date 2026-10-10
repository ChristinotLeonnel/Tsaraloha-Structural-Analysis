#pragma once

// Champ « Couleur » des propriétés d'un élément porteur d'un matériau.
// Couleur vide = apparence du matériau (couleur, aspect, texture). Le panneau n'enregistre une couleur
// que si l'utilisateur en choisit une ; « Matériau » revient à l'apparence du matériau. Auparavant,
// valider le panneau pour un autre réglage enregistrait une couleur par défaut du type d'élément
// (poutre bleue, poteau orange…), qui masquait le matériau.

#include "../../Model/Material.h"

#include <QColor>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QToolButton>
#include <QWidget>

#include <functional>
#include <string>

namespace TSA::UI::ElementColor
{

/// Couleur affichée par le bouton : celle de l'utilisateur, sinon celle du matériau.
inline QString shown(const QString& userHex, const TSA::Model::Material& material)
{
    if (!userHex.isEmpty()) return userHex;
    const QColor c(QString::fromStdString(material.visual.baseColor));
    return c.isValid() ? c.name() : QStringLiteral("#9EA0A2");
}

/// Met à jour le bouton (couleur, infobulle) et retient la couleur affichée pour le sélecteur.
inline void showOn(QPushButton* button, const QString& userHex, const TSA::Model::Material& material)
{
    const QString hex = shown(userHex, material);
    button->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(hex));
    button->setProperty("shownColor", hex);
    button->setToolTip(userHex.isEmpty()
                           ? QObject::tr("Apparence du matériau « %1 ». Cliquer pour choisir une couleur propre à cet élément.")
                                 .arg(QString::fromStdString(material.name))
                           : QObject::tr("Couleur choisie pour cet élément (%1).").arg(userHex));
}

/// Couleur de départ du sélecteur.
inline QColor initial(const QPushButton* button)
{
    return QColor(button->property("shownColor").toString());
}

/// Ligne « [couleur] [Matériau] » : le second bouton efface la couleur propre à l'élément.
inline QWidget* row(QPushButton* colorButton, QWidget* parent, std::function<void()> onUseMaterial)
{
    auto* container = new QWidget(parent);
    auto* lay = new QHBoxLayout(container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    colorButton->setParent(container);
    lay->addWidget(colorButton, 1);
    auto* reset = new QToolButton(container);
    reset->setObjectName(QStringLiteral("useMaterialColorButton"));
    reset->setText(QObject::tr("Matériau"));
    reset->setToolTip(QObject::tr("Revenir à l'apparence du matériau (couleur et texture)"));
    QObject::connect(reset, &QToolButton::clicked, container, std::move(onUseMaterial));
    lay->addWidget(reset);
    return container;
}

} // namespace TSA::UI::ElementColor
