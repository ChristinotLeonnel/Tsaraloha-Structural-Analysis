#pragma once

#include <QDialog>
#include <QString>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QWidget;

namespace TSA::UI
{

class StandardDialogHeader;

/// Catégories de tailles standardisées pour les dialogues TSA (norme ergonomique BIM/CAO)
enum class DialogSizeCategory
{
    Compact,    /// Min 420x300, Défaut 460x360 (Création nœuds, niveaux, cotes, prompts simples)
    Standard,   /// Min 520x480, Défaut 560x600 (Création barres, câbles, dalles, charges)
    Large,      /// Min 740x540, Défaut 860x640 (Cas de charge, combinaisons, presets structures, grilles)
    Studio      /// Min 960x620, Défaut 1080x700 (Gestionnaire d'extensions, analyse multi-moteurs, rapports)
};

/// Applique les dimensions minimales et par défaut normalisées selon la catégorie choisie.
void applyStandardDialogSizing(QDialog* dialog, DialogSizeCategory category);

/// Crée et insère un en-tête standard en haut du layout principal d'un dialogue.
StandardDialogHeader* addStandardDialogHeader(QVBoxLayout* mainLayout,
                                              const QString& title,
                                              const QString& subtitle = QString(),
                                              const QString& iconPath = QString());

/// Crée une ligne de boutons standardisée :
/// [Aide]? [Widget extra gauche]? [Stretch] [Annuler/Fermer] [Action Primaire]
QHBoxLayout* createStandardButtonRow(QDialog* dialog,
                                     QPushButton* primaryBtn,
                                     QPushButton* cancelBtn,
                                     const QString& helpTopic = QString(),
                                     QWidget* extraLeftWidget = nullptr);

/// Classe de base prête à l'emploi pour les boîtes de dialogue standardisées de TSA.
class StandardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StandardDialog(DialogSizeCategory category, QWidget* parent = nullptr);
    ~StandardDialog() override = default;

    /// Définit l'en-tête standard de la fenêtre
    void setDialogHeader(const QString& title,
                         const QString& subtitle = QString(),
                         const QString& iconPath = QString());

    /// Accès au layout de contenu principal (marges 16px, espacement 12px)
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

    /// Configure la rangée de boutons inférieure standard
    QHBoxLayout* setStandardButtons(QPushButton* primaryBtn,
                                    QPushButton* cancelBtn,
                                    const QString& helpTopic = QString(),
                                    QWidget* extraLeftWidget = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void initBase(DialogSizeCategory category);

private:
    QVBoxLayout* m_mainLayout = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    StandardDialogHeader* m_header = nullptr;
    QHBoxLayout* m_buttonLayout = nullptr;
};

} // namespace TSA::UI
