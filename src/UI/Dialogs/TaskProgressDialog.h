#pragma once

// Travail long mené dans un thread de travail, l'interface restant réactive (import et export IFC, installation d'un
// moteur d'analyse) : fenêtre de progression modale, avancement publié par le travail, arrêt demandé par l'utilisateur.

#include <QProgressDialog>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

namespace TSA::UI
{

/// Progression d'un travail mené dans un thread de travail. « Annuler » (bouton, Échap, fermeture) ne fait que demander
/// l'arrêt : la fenêtre reste affichée, et la fenêtre principale bloquée, jusqu'à la fin effective du travail (sinon une
/// seconde commande pourrait démarrer dans la boucle locale de la première). Sans bouton (setCancelButton(nullptr)) :
/// travail non interruptible.
class TaskProgressDialog : public QProgressDialog
{
    Q_OBJECT
public:
    TaskProgressDialog(const QString& title, const QString& label, QWidget* parent = nullptr);
    bool cancelRequested() const { return m_cancelRequested; }
    /// Étape en cours (ignorée une fois l'annulation demandée : le libellé « Annulation… » reste affiché).
    void setStage(int percent, const QString& stage);

protected:
    void reject() override;
    void closeEvent(QCloseEvent* event) override;

private:
    void requestCancel();

    bool m_cancelRequested = false;
};

/// Progression publiée par le thread de travail, relevée par l'interface (minuterie) ; l'annulation est demandée par
/// l'interface et constatée par le travail à sa prochaine étape.
struct TaskProgress
{
    std::atomic<int> percent { 0 };
    std::atomic<bool> cancel { false };

    void publish(int p, const std::string& s);
    QString currentStage();

private:
    std::mutex m_mutex;
    std::string m_stage;
};

/// Exécute `work` dans un thread de travail ; boucle locale (fenêtre de progression modale) jusqu'à sa fin effective.
/// `work` ne touche à aucun widget. Une exception échappée du travail est rendue en message (jamais de fin anormale du
/// programme) ; chaîne vide si le travail s'est terminé normalement.
std::string runTaskInWorker(TaskProgressDialog& dialog, TaskProgress& progress, const std::function<void()>& work);

} // namespace TSA::UI
