#pragma once

// File des fichiers à ouvrir dans l'instance en cours : ouverts un par un, dans l'ordre, jamais pendant qu'une boîte de dialogue
// modale attend l'utilisateur (mot de passe, récupération, erreur). Des fichiers arrivent de l'Explorateur à tout moment,
// y compris pendant l'ouverture d'un autre (boucle d'événements imbriquée d'une boîte de dialogue) : sans cette file,
// une ouverture démarrerait au milieu de la précédente.

#include <QObject>
#include <QStringList>
#include <QTimer>

#include <functional>

namespace TSA::Platform
{

class OpenFileQueue : public QObject
{
    Q_OBJECT

public:
    /// `open` : ouvre un fichier (peut afficher une boîte de dialogue) ; `busy` : vrai tant qu'il faut attendre.
    OpenFileQueue(std::function<void(const QString&)> open, std::function<bool()> busy, QObject* parent = nullptr);

    /// Ajoute (sans doublon dans la file) ; le premier fichier s'ouvre tout de suite si rien n'empêche, les suivants après
    /// retour à la boucle d'événements.
    void enqueue(const QStringList& files);
    /// Abandonne les fichiers en attente (fermeture de l'application).
    void clear();
    int pendingCount() const { return m_pending.size(); }

private:
    void process();

    std::function<void(const QString&)> m_open;
    std::function<bool()> m_busy;
    QStringList m_pending;
    QTimer m_retry;
    bool m_running = false;
};

} // namespace TSA::Platform
