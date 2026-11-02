#pragma once

// Une seule fenêtre de l'application par utilisateur et par session Windows, comme AutoCAD : un lancement suivant (double-clic sur
// un fichier, plusieurs fichiers sélectionnés dans l'Explorateur, raccourci) confie ses fichiers à l'instance en cours et se
// termine ; ils s'ouvrent dans ses onglets.
//
// Élection : QLockFile (le système le libère si le processus disparaît : une instance plantée ne bloque rien). L'Explorateur lance
// un processus par fichier sélectionné, tous en même temps : un seul obtient le verrou, les autres attendent son canal
// puis lui envoient leurs fichiers ; rien n'est perdu ni ouvert deux fois.
// Canal : QLocalServer réservé au compte de l'utilisateur, dans un fil à part : une instance occupée (chargement d'un gros
// projet, boîte de dialogue) reçoit et acquitte quand même, les fichiers sont livrés au fil de l'interface quand il est libre.
// Protocole, une ligne par message : serveur « TSA-INSTANCE 1 <pid> » ; client tableau JSON des chemins absolus ; serveur « OK ».

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QLockFile;
class QThread;

namespace TSA::Platform
{

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    enum class Role
    {
        Primary,     ///< cette instance garde la fenêtre et reçoit les fichiers des suivantes
        Forwarded,   ///< une instance tourne déjà, elle a pris les fichiers : se terminer sans fenêtre
        Standalone   ///< canal impossible (instance figée, canal refusé) : fonctionne seule, plutôt que perdre les fichiers
    };

    /// `name` : canal et verrou (defaultName() en production) ; `lockDirectory` vide : dossier local de l'application.
    explicit SingleInstance(const QString& name = defaultName(), const QString& lockDirectory = QString(), QObject* parent = nullptr);
    ~SingleInstance() override;

    /// Nom propre au produit, à l'utilisateur et à la session Windows.
    static QString defaultName();

    /// Devient l'instance principale, ou confie `files` (chemins rendus absolus) à celle en cours. Attend une instance en
    /// train de démarrer ou de se fermer jusqu'à `timeoutMs`.
    Role acquire(const QStringList& files, int timeoutMs = 15000);

    /// Cesse de recevoir (fermeture de l'application) ; le verrou est rendu par stop.
    void stop();

signals:
    /// Dans le fil de cet objet. `files` vide : simple lancement, mettre la fenêtre au premier plan.
    void filesReceived(const QStringList& files);

private:
    bool forward(const QStringList& files) const;
    bool listen();

    QString m_name;
    QString m_lockPath;
    std::unique_ptr<QLockFile> m_lock;
    QThread* m_thread = nullptr;
    QObject* m_worker = nullptr;   ///< vit dans m_thread, parent du serveur et de ses connexions
};

} // namespace TSA::Platform
