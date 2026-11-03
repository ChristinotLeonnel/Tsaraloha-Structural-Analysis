#include "TaskProgressDialog.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QEventLoop>
#include <QPushButton>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <exception>
#include <new>

namespace TSA::UI
{

TaskProgressDialog::TaskProgressDialog(const QString& title, const QString& label, QWidget* parent)
    : QProgressDialog(label, tr("Annuler"), 0, 100, parent)
{
    setWindowTitle(title);
    setWindowModality(Qt::WindowModal);
    setMinimumDuration(0);
    setAutoClose(false);
    setAutoReset(false);
    // Liaison par défaut canceled() → cancel() : elle masque la fenêtre, remplacée par une simple demande d'arrêt.
    // (nullptr : retire la liaison quelle que soit sa forme, chaîne SIGNAL/SLOT ou pointeur de fonction.)
    disconnect(this, &QProgressDialog::canceled, this, nullptr);
    connect(this, &QProgressDialog::canceled, this, &TaskProgressDialog::requestCancel);
    setValue(0);
}

void TaskProgressDialog::setStage(int percent, const QString& stage)
{
    setValue(std::clamp(percent, 0, 99));   // 100 seulement à la fin effective
    if (!m_cancelRequested && !stage.isEmpty()) setLabelText(stage);
}

void TaskProgressDialog::reject()
{
    emit canceled();   // Échap (QDialog::reject masquerait la fenêtre sans émettre canceled)
}

void TaskProgressDialog::closeEvent(QCloseEvent* event)
{
    event->ignore();
    emit canceled();
}

void TaskProgressDialog::requestCancel()
{
    auto* button = findChild<QPushButton*>();
    if (m_cancelRequested || !button) return;   // sans bouton (setCancelButton(nullptr)) : travail non interruptible
    m_cancelRequested = true;
    setLabelText(tr("Annulation en cours… (fin de l'étape en cours)"));
    button->setEnabled(false);
}

void TaskProgress::publish(int p, const std::string& s)
{
    percent = p;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stage = s;
}

QString TaskProgress::currentStage()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return QString::fromStdString(m_stage);
}

std::string runTaskInWorker(TaskProgressDialog& dialog, TaskProgress& progress, const std::function<void()>& work)
{
    std::string failure;
    QEventLoop loop;
    QThread* worker = QThread::create([&work, &failure] {
        try
        {
            work();
        }
        catch (const std::bad_alloc&)
        {
            failure = QCoreApplication::translate("MainWindow", "mémoire insuffisante").toStdString();
        }
        catch (const std::exception& e)
        {
            failure = e.what();
        }
        catch (...)
        {
            failure = QCoreApplication::translate("MainWindow", "erreur interne inconnue").toStdString();
        }
    });
    QObject::connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
    QObject::connect(&dialog, &QProgressDialog::canceled, &loop, [&progress] { progress.cancel = true; });
    QTimer poll;
    poll.setInterval(100);
    QObject::connect(&poll, &QTimer::timeout, &dialog, [&dialog, &progress] { dialog.setStage(progress.percent, progress.currentStage()); });
    worker->start();
    poll.start();
    loop.exec();
    poll.stop();
    worker->wait();
    delete worker;
    dialog.setValue(dialog.maximum());
    dialog.hide();
    return failure;
}

} // namespace TSA::UI
