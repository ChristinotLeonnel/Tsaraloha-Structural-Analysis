#pragma once
// Garde-molette de l'application (TSA, TSALab) : la molette ne modifie jamais un champ numérique ni une liste déroulante
// fermée. Faire défiler un panneau ou une boîte de dialogue dont un champ passe sous le curseur changeait sa valeur sans
// que l'utilisateur l'ait voulu (modification non demandée du modèle). Le défilement est transmis à la zone qui contient
// le champ ; une liste déroulante ouverte se fait défiler normalement.
#include <QObject>

class QWidget;

namespace TSA::UI
{

class WheelGuard : public QObject
{
    Q_OBJECT
public:
    /// Installe le garde sur l'application (une seule fois, sans effet ensuite). Appelé par EcosystemApplication.
    static void install();

    /// Champ protégé (QAbstractSpinBox, QComboBox) visé par la molette en widget, ou nul : le widget lui-même ou l'un de
    /// ses parents dans la même fenêtre (le champ texte interne d'un QSpinBox compte). Exposé pour les tests.
    static QWidget* protectedField(QWidget* widget);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit WheelGuard(QObject* parent);
};

} // namespace TSA::UI
