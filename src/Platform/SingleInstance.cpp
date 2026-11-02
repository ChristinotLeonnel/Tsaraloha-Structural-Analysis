#include "SingleInstance.h"

#include <ProductIdentity.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QThread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace TSA::Platform
{
namespace
{
constexpr qint64 kMaxMessage = 1024 * 1024;   ///< liste de fichiers plafonnée : le canal n'alloue pas sans limite
constexpr int kConnectMs = 300;
constexpr int kReplyMs = 3000;

QByteArray helloLine()
{
    return "TSA-INSTANCE 1 " + QByteArray::number(QCoreApplication::applicationPid()) + "\n";
}

/// Lit une ligne complète (sans la fin de ligne) ; vide si le délai expire.
QByteArray readLine(QLocalSocket& socket, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!socket.canReadLine())
    {
        const int left = timeoutMs - static_cast<int>(timer.elapsed());
        if (left <= 0 || !socket.waitForReadyRead(left)) break;
    }
    return socket.canReadLine() ? socket.readLine().trimmed() : QByteArray();
}
} // namespace

SingleInstance::SingleInstance(const QString& name, const QString& lockDirectory, QObject* parent)
    : QObject(parent), m_name(name)
{
    QString dir = lockDirectory;
    if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    QDir().mkpath(dir);
    m_lockPath = QDir(dir).filePath(name + QStringLiteral(".instance.lock"));
}

SingleInstance::~SingleInstance()
{
    stop();
}

QString SingleInstance::defaultName()
{
    QString scope = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
#ifdef _WIN32
    // Les tubes nommés sont communs à toute la machine : autre session (bureau à distance) = autre fenêtre, autre instance
    DWORD session = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &session)) scope += QLatin1Char('-') + QString::number(session);
#endif
    const QByteArray hash = QCryptographicHash::hash(scope.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    return QStringLiteral("tsaraloha-%1-%2").arg(QString::fromLatin1(TSA::Product::kName).toLower(), QString::fromLatin1(hash));
}

SingleInstance::Role SingleInstance::acquire(const QStringList& files, int timeoutMs)
{
    QStringList absolute;
    for (const QString& file : files) absolute << QFileInfo(file).absoluteFilePath();

    if (!m_lock)
    {
        m_lock = std::make_unique<QLockFile>(m_lockPath);
        m_lock->setStaleLockTime(0);   // jamais périmé à l'ancienneté : seul un processus disparu libère le verrou
    }
    QElapsedTimer timer;
    timer.start();
    do
    {
        if (m_lock->tryLock(0)) return listen() ? Role::Primary : Role::Standalone;
        // Verrou tenu : instance en cours, ou en train de démarrer (canal pas encore ouvert) ou de se fermer (verrou bientôt libre)
        if (forward(absolute)) return Role::Forwarded;
        QThread::msleep(50);
    } while (timer.elapsed() < timeoutMs);
    return Role::Standalone;
}

bool SingleInstance::forward(const QStringList& files) const
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(kConnectMs)) return false;
    const QByteArray hello = readLine(socket, kReplyMs);
    if (!hello.startsWith("TSA-INSTANCE 1 ")) return false;
#ifdef _WIN32
    // Sans ce droit, Windows refuse le premier plan à l'instance en cours : sa fenêtre clignoterait dans la barre des tâches
    AllowSetForegroundWindow(static_cast<DWORD>(hello.mid(15).toULong()));
#endif
    socket.write(QJsonDocument(QJsonArray::fromStringList(files)).toJson(QJsonDocument::Compact) + "\n");
    if (!socket.waitForBytesWritten(kReplyMs)) return false;
    return readLine(socket, kReplyMs) == "OK";
}

bool SingleInstance::listen()
{
    m_thread = new QThread;
    m_thread->setObjectName(QStringLiteral("SingleInstance"));
    m_worker = new QObject;
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);   // serveur et connexions détruits dans leur fil
    m_thread->start();

    bool listening = false;
    QMetaObject::invokeMethod(
        m_worker,
        [this, &listening] {
            QLocalServer::removeServer(m_name);   // Unix : fichier laissé par une instance plantée (le verrou prouve qu'elle est morte)
            auto* server = new QLocalServer(m_worker);
            server->setSocketOptions(QLocalServer::UserAccessOption);   // compte de l'utilisateur seulement
            if (!server->listen(m_name))
            {
                delete server;
                return;
            }
            listening = true;
            connect(server, &QLocalServer::newConnection, m_worker, [this, server] {
                while (QLocalSocket* socket = server->nextPendingConnection())
                {
                    socket->setParent(m_worker);
                    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                    connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                        if (socket->bytesAvailable() > kMaxMessage)
                        {
                            socket->abort();
                            return;
                        }
                        if (!socket->canReadLine()) return;
                        const QJsonDocument doc = QJsonDocument::fromJson(socket->readLine());
                        if (!doc.isArray())
                        {
                            socket->abort();
                            return;
                        }
                        QStringList files;
                        for (const auto& value : doc.array()) files << value.toString();
                        socket->write("OK\n");
                        socket->flush();
                        socket->disconnectFromServer();
                        // Livré au fil de l'interface, qui le traitera quand il sera libre ; l'instance suivante est déjà acquittée
                        QMetaObject::invokeMethod(this, [this, files] { emit filesReceived(files); }, Qt::QueuedConnection);
                    });
                    socket->write(helloLine());
                    socket->flush();
                }
            });
        },
        Qt::BlockingQueuedConnection);
    return listening;
}

void SingleInstance::stop()
{
    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
        m_worker = nullptr;
    }
    if (m_lock) m_lock->unlock();
}

} // namespace TSA::Platform
