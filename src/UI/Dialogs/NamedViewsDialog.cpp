#include "NamedViewsDialog.h"

#include "../../Model/Model.h"
#include "../../Viewer/OccView.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace TSA::UI
{

namespace
{
QString describe(const TSA::Model::NamedView& v)
{
    static const QMap<QString, QString> faces = {
        { "top", QObject::tr("plan (dessus)") }, { "bottom", QObject::tr("plan (dessous)") },
        { "front", QObject::tr("élévation avant") }, { "back", QObject::tr("élévation arrière") },
        { "left", QObject::tr("élévation gauche") }, { "right", QObject::tr("élévation droite") } };
    if (v.mode2D)
        return QObject::tr("2D — %1").arg(faces.value(QString::fromStdString(v.planFace), QObject::tr("plan de travail")));
    return v.orthographic ? QObject::tr("3D — orthographique") : QObject::tr("3D — perspective");
}
} // namespace

NamedViewsDialog::NamedViewsDialog(TSA::Model::Model* model, OccView* view, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_view(view)
{
    setWindowTitle(tr("Vues nommées"));
    setMinimumWidth(420);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Vues enregistrées dans ce projet (double-clic : restaurer)"), this));
    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto* row = new QHBoxLayout();
    auto* btnSave = new QPushButton(tr("Enregistrer la vue actuelle…"), this);
    m_btnRestore = new QPushButton(tr("Restaurer"), this);
    m_btnRename = new QPushButton(tr("Renommer…"), this);
    m_btnDelete = new QPushButton(tr("Supprimer"), this);
    row->addWidget(btnSave);
    row->addWidget(m_btnRestore);
    row->addWidget(m_btnRename);
    row->addWidget(m_btnDelete);
    layout->addLayout(row);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(box);

    connect(btnSave, &QPushButton::clicked, this, &NamedViewsDialog::onSave);
    connect(m_btnRestore, &QPushButton::clicked, this, [this] { restoreView(selectedName()); });
    connect(m_btnRename, &QPushButton::clicked, this, &NamedViewsDialog::onRename);
    connect(m_btnDelete, &QPushButton::clicked, this, &NamedViewsDialog::onDelete);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { restoreView(selectedName()); });
    connect(m_list, &QListWidget::currentRowChanged, this, &NamedViewsDialog::updateButtons);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refreshList();
}

QStringList NamedViewsDialog::viewNames() const
{
    QStringList names;
    if (m_model)
        for (const auto& v : m_model->namedViews().views)
            names << QString::fromStdString(v.name);
    return names;
}

void NamedViewsDialog::refreshList(const QString& select)
{
    m_list->clear();
    if (m_model)
    {
        for (const auto& v : m_model->namedViews().views)
        {
            auto* item = new QListWidgetItem(QStringLiteral("%1   (%2)").arg(QString::fromStdString(v.name), describe(v)), m_list);
            item->setData(Qt::UserRole, QString::fromStdString(v.name));
            if (QString::fromStdString(v.name) == select)
                m_list->setCurrentItem(item);
        }
    }
    if (!m_list->currentItem() && m_list->count() > 0)
        m_list->setCurrentRow(0);
    updateButtons();
}

QString NamedViewsDialog::selectedName() const
{
    const QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void NamedViewsDialog::updateButtons()
{
    const bool any = m_list->currentItem() != nullptr;
    m_btnRestore->setEnabled(any);
    m_btnRename->setEnabled(any);
    m_btnDelete->setEnabled(any);
}

bool NamedViewsDialog::saveCurrentView(const QString& name)
{
    const QString n = name.trimmed();
    if (!m_model || !m_view || n.isEmpty())
        return false;
    TSA::Model::NamedViewSet set = m_model->namedViews();
    set.upsert(m_view->captureNamedView(n.toStdString()));
    m_model->setNamedViews(set);
    refreshList(n);
    emit namedViewsChanged();
    return true;
}

bool NamedViewsDialog::restoreView(const QString& name)
{
    if (!m_model || !m_view)
        return false;
    const int i = m_model->namedViews().indexOf(name.toStdString());
    if (i < 0 || !m_view->restoreNamedView(m_model->namedViews().views[static_cast<size_t>(i)]))
        return false;
    emit viewRestored(name);
    return true;
}

bool NamedViewsDialog::renameView(const QString& from, const QString& to)
{
    if (!m_model)
        return false;
    TSA::Model::NamedViewSet set = m_model->namedViews();
    if (!set.rename(from.toStdString(), to.trimmed().toStdString()))
        return false;
    m_model->setNamedViews(set);
    refreshList(to.trimmed());
    emit namedViewsChanged();
    return true;
}

bool NamedViewsDialog::deleteView(const QString& name)
{
    if (!m_model)
        return false;
    TSA::Model::NamedViewSet set = m_model->namedViews();
    if (!set.remove(name.toStdString()))
        return false;
    m_model->setNamedViews(set);
    refreshList();
    emit namedViewsChanged();
    return true;
}

void NamedViewsDialog::onSave()
{
    const QString proposed = tr("Vue %1").arg(m_model ? static_cast<int>(m_model->namedViews().views.size()) + 1 : 1);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Enregistrer la vue actuelle"), tr("Nom de la vue :"), QLineEdit::Normal, proposed, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    if (viewNames().contains(name)
        && QMessageBox::question(this, tr("Vues nommées"), tr("La vue « %1 » existe déjà. La remplacer par la vue actuelle ?").arg(name))
               != QMessageBox::Yes)
        return;
    saveCurrentView(name);
}

void NamedViewsDialog::onRename()
{
    const QString from = selectedName();
    if (from.isEmpty())
        return;
    bool ok = false;
    const QString to = QInputDialog::getText(this, tr("Renommer la vue"), tr("Nouveau nom :"), QLineEdit::Normal, from, &ok).trimmed();
    if (!ok || to.isEmpty() || to == from)
        return;
    if (!renameView(from, to))
        QMessageBox::warning(this, tr("Vues nommées"), tr("Une vue nommée « %1 » existe déjà.").arg(to));
}

void NamedViewsDialog::onDelete()
{
    const QString name = selectedName();
    if (!name.isEmpty() && QMessageBox::question(this, tr("Vues nommées"), tr("Supprimer la vue « %1 » ?").arg(name)) == QMessageBox::Yes)
        deleteView(name);
}

} // namespace TSA::UI
