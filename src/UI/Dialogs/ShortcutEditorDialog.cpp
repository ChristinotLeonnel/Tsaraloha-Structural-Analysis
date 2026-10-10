#include "ShortcutEditorDialog.h"

#include "../Shortcuts/ShortcutManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace TSA::UI
{

using namespace TSA::UI::Shortcuts;

namespace
{
enum Column
{
    ColName,
    ColShortcut,
    ColDefault,
    ColState,
    ColCategory,
    ColDescription,
    ColCount
};
} // namespace

ShortcutEditorDialog::ShortcutEditorDialog(ShortcutManager& manager, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("Raccourcis clavier"));
    setObjectName(QStringLiteral("shortcutEditorDialog"));
    resize(980, 640);
    auto* root = new QVBoxLayout(this);

    auto* filters = new QHBoxLayout();
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("shortcutSearch"));
    m_search->setPlaceholderText(tr("Rechercher : nom, description, identifiant ou raccourci (ex. « Ctrl+S », « F5 »)"));
    m_search->setClearButtonEnabled(true);
    filters->addWidget(m_search, 1);
    m_category = new QComboBox(this);
    filters->addWidget(m_category);
    root->addLayout(filters);

    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels({ tr("Commande"), tr("Raccourci"), tr("Par défaut"), tr("État"), tr("Catégorie"), tr("Description") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColDescription, QHeaderView::Stretch);
    m_table->setSortingEnabled(false);
    root->addWidget(m_table, 1);

    auto* editBox = new QGroupBox(tr("Commande sélectionnée"), this);
    auto* edit = new QVBoxLayout(editBox);
    m_selectedLabel = new QLabel(tr("Sélectionnez une commande."), editBox);
    m_selectedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    edit->addWidget(m_selectedLabel);
    auto* rec = new QHBoxLayout();
    rec->addWidget(new QLabel(tr("Nouvelle combinaison :"), editBox));
    m_recorder = new QKeySequenceEdit(editBox);
    m_recorder->setMaximumSequenceLength(1); // une combinaison complète, jamais une touche en cours de saisie
    m_recorder->setToolTip(tr("Cliquez puis appuyez sur la combinaison voulue (Ctrl, Alt, Maj compris)"));
    rec->addWidget(m_recorder, 1);
    auto* replaceBtn = new QPushButton(tr("Remplacer"), editBox);
    auto* aliasBtn = new QPushButton(tr("Ajouter comme alias"), editBox);
    rec->addWidget(replaceBtn);
    rec->addWidget(aliasBtn);
    edit->addLayout(rec);
    auto* textRow = new QHBoxLayout();
    textRow->addWidget(new QLabel(tr("Raccourci(s) :"), editBox));
    m_sequenceText = new QLineEdit(editBox);
    m_sequenceText->setObjectName(QStringLiteral("shortcutText"));
    m_sequenceText->setPlaceholderText(tr("ex. « Ctrl+S ; F7 », suite de touches « D, A » ; vide : aucun"));
    textRow->addWidget(m_sequenceText, 1);
    m_enabled = new QCheckBox(tr("Actif"), editBox);
    textRow->addWidget(m_enabled);
    auto* clearBtn = new QPushButton(tr("Effacer"), editBox);
    auto* resetBtn = new QPushButton(tr("Rétablir la valeur par défaut"), editBox);
    textRow->addWidget(clearBtn);
    textRow->addWidget(resetBtn);
    edit->addLayout(textRow);
    m_conflictLabel = new QLabel(editBox);
    m_conflictLabel->setWordWrap(true);
    m_conflictLabel->setStyleSheet(QStringLiteral("color:#FF5252;"));
    edit->addWidget(m_conflictLabel);
    root->addWidget(editBox);

    auto* bottom = new QHBoxLayout();
    auto* resetAllBtn = new QPushButton(tr("Tout rétablir par défaut"), this);
    auto* openBtn = new QPushButton(tr("Ouvrir shortcut.txt"), this);
    openBtn->setToolTip(QDir::toNativeSeparators(m_manager.configPath()));
    auto* reloadBtn = new QPushButton(tr("Recharger le fichier"), this);
    bottom->addWidget(resetAllBtn);
    bottom->addWidget(openBtn);
    bottom->addWidget(reloadBtn);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    bottom->addWidget(m_status, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    bottom->addWidget(buttons);
    root->addLayout(bottom);

    m_work = m_manager.settings();
    populate();

    connect(m_search, &QLineEdit::textChanged, this, &ShortcutEditorDialog::refreshFilter);
    connect(m_category, &QComboBox::currentIndexChanged, this, &ShortcutEditorDialog::refreshFilter);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ShortcutEditorDialog::refreshEditor);
    connect(replaceBtn, &QPushButton::clicked, this, [this] {
        if (!m_recorder->keySequence().isEmpty()) setSelectedShortcutText(m_recorder->keySequence().toString(QKeySequence::PortableText));
        m_recorder->clear();
    });
    connect(aliasBtn, &QPushButton::clicked, this, [this] {
        const QKeySequence seq = m_recorder->keySequence();
        if (seq.isEmpty() || selectedId().isEmpty()) return;
        QList<QKeySequence> list = workingSetting(selectedId()).sequences;
        if (!list.contains(seq)) list << seq;
        setSelectedShortcutText(sequenceListText(list));
        m_recorder->clear();
    });
    connect(m_sequenceText, &QLineEdit::editingFinished, this, [this] {
        if (!m_updating && !selectedId().isEmpty()) setSelectedShortcutText(m_sequenceText->text());
    });
    connect(m_enabled, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_updating) setSelectedEnabled(on);
    });
    connect(clearBtn, &QPushButton::clicked, this, [this] { setSelectedShortcutText(QString()); });
    connect(resetBtn, &QPushButton::clicked, this, &ShortcutEditorDialog::resetSelected);
    connect(resetAllBtn, &QPushButton::clicked, this, &ShortcutEditorDialog::resetAll);
    connect(openBtn, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_manager.configPath())); });
    connect(reloadBtn, &QPushButton::clicked, this, [this] {
        if (m_manager.reload())
        {
            m_work = m_manager.settings();
            refreshAllRows();
            m_status->setStyleSheet(QString());
            m_status->setText(tr("Fichier relu et appliqué."));
        }
        else
        {
            m_status->setStyleSheet(QStringLiteral("color:#FF5252;"));
            m_status->setText(m_manager.lastErrors().join(QLatin1Char('\n')));
        }
    });
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] { apply(); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (apply()) accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ShortcutEditorDialog::populate()
{
    m_defs = m_manager.definitions();
    m_category->blockSignals(true);
    m_category->clear();
    m_category->addItem(tr("Toutes les catégories"), QString());
    QStringList cats;
    for (const auto& d : m_defs)
        if (!cats.contains(d.category)) cats << d.category;
    for (const auto& c : cats) m_category->addItem(c, c);
    m_category->blockSignals(false);

    m_table->setRowCount(m_defs.size());
    for (int r = 0; r < m_defs.size(); ++r)
    {
        for (int c = 0; c < ColCount; ++c) m_table->setItem(r, c, new QTableWidgetItem());
        m_table->item(r, ColName)->setData(Qt::UserRole, m_defs[r].id);
        m_table->item(r, ColName)->setText(m_defs[r].name);
        m_table->item(r, ColName)->setToolTip(m_defs[r].id);
        m_table->item(r, ColDefault)->setText(sequenceListNativeText(m_defs[r].defaults));
        m_table->item(r, ColCategory)->setText(m_defs[r].category);
        m_table->item(r, ColDescription)->setText(m_defs[r].description);
        refreshRow(r);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(ColDescription, QHeaderView::Stretch);
    refreshConflicts();
}

ShortcutSetting ShortcutEditorDialog::workingSetting(const QString& id) const
{
    const auto it = m_work.find(id);
    if (it != m_work.end()) return *it;
    for (const auto& d : m_defs)
        if (d.id == id) return { d.defaults, true };
    return {};
}

void ShortcutEditorDialog::refreshRow(int row)
{
    const auto& d = m_defs[row];
    const ShortcutSetting s = workingSetting(d.id);
    const bool custom = !(s.sequences == d.defaults && s.enabled);
    m_table->item(row, ColShortcut)->setText(sequenceListNativeText(s.sequences));
    m_table->item(row, ColState)->setText(s.enabled ? tr("actif") : tr("désactivé"));
    bool available = false;
    for (QAction* a : m_manager.actions(d.id)) available |= a->isEnabled();
    m_table->item(row, ColState)->setToolTip(available ? QString() : tr("Commande indisponible dans le contexte actuel"));
    QFont f = m_table->font();
    f.setBold(custom);
    for (int c = 0; c < ColCount; ++c) m_table->item(row, c)->setFont(f);
}

void ShortcutEditorDialog::refreshAllRows()
{
    for (int r = 0; r < m_table->rowCount(); ++r) refreshRow(r);
    refreshConflicts();
    refreshEditor();
}

void ShortcutEditorDialog::refreshFilter()
{
    const QString needle = m_search->text().trimmed();
    const QString cat = m_category->currentData().toString();
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        const auto& d = m_defs[r];
        bool match = cat.isEmpty() || d.category == cat;
        if (match && !needle.isEmpty())
        {
            const ShortcutSetting s = workingSetting(d.id);
            const QString hay = QStringList{ d.name, d.description, d.id, sequenceListNativeText(s.sequences), sequenceListText(s.sequences) }.join(QLatin1Char(' '));
            match = hay.contains(needle, Qt::CaseInsensitive);
        }
        m_table->setRowHidden(r, !match);
    }
}

void ShortcutEditorDialog::setSearchText(const QString& text)
{
    m_search->setText(text);
}

int ShortcutEditorDialog::visibleRowCount() const
{
    int n = 0;
    for (int r = 0; r < m_table->rowCount(); ++r) n += m_table->isRowHidden(r) ? 0 : 1;
    return n;
}

int ShortcutEditorDialog::rowOf(const QString& id) const
{
    for (int r = 0; r < m_defs.size(); ++r)
        if (m_defs[r].id == id) return r;
    return -1;
}

bool ShortcutEditorDialog::selectCommand(const QString& id)
{
    const int r = rowOf(id);
    if (r < 0) return false;
    m_table->selectRow(r);
    refreshEditor();
    return true;
}

QString ShortcutEditorDialog::selectedId() const
{
    const auto items = m_table->selectedItems();
    if (items.isEmpty()) return {};
    return m_table->item(items.first()->row(), ColName)->data(Qt::UserRole).toString();
}

void ShortcutEditorDialog::refreshEditor()
{
    const QString id = selectedId();
    m_updating = true;
    if (id.isEmpty())
    {
        m_selectedLabel->setText(tr("Sélectionnez une commande."));
        m_sequenceText->clear();
    }
    else
    {
        const int r = rowOf(id);
        const ShortcutSetting s = workingSetting(id);
        m_selectedLabel->setText(QStringLiteral("<b>%1</b> — <code>%2</code><br>%3")
                                     .arg(m_defs[r].name.toHtmlEscaped(), id, m_defs[r].description.toHtmlEscaped()));
        m_sequenceText->setText(sequenceListText(s.sequences));
        m_enabled->setChecked(s.enabled);
    }
    m_updating = false;
    refreshConflicts();
}

bool ShortcutEditorDialog::setSelectedShortcutText(const QString& text)
{
    const QString id = selectedId();
    if (id.isEmpty()) return false;
    QList<QKeySequence> seqs;
    QString err;
    if (!parseSequenceList(text, &seqs, &err))
    {
        m_status->setStyleSheet(QStringLiteral("color:#FF5252;"));
        m_status->setText(tr("Raccourci refusé : %1").arg(err));
        return false;
    }
    ShortcutSetting s = workingSetting(id);
    s.sequences = seqs;
    m_work[id] = s;
    refreshRow(rowOf(id));
    refreshEditor();
    m_status->setStyleSheet(QString());
    m_status->setText(tr("Modifié — non appliqué."));
    return true;
}

void ShortcutEditorDialog::setSelectedEnabled(bool enabled)
{
    const QString id = selectedId();
    if (id.isEmpty()) return;
    ShortcutSetting s = workingSetting(id);
    s.enabled = enabled;
    m_work[id] = s;
    refreshRow(rowOf(id));
    refreshEditor();
    m_status->setStyleSheet(QString());
    m_status->setText(tr("Modifié — non appliqué."));
}

void ShortcutEditorDialog::resetSelected()
{
    const QString id = selectedId();
    if (id.isEmpty()) return;
    m_work.remove(id);
    refreshRow(rowOf(id));
    refreshEditor();
}

void ShortcutEditorDialog::resetAll()
{
    m_work.clear();
    refreshAllRows();
    m_status->setStyleSheet(QString());
    m_status->setText(tr("Valeurs par défaut rétablies — non appliqué."));
}

QStringList ShortcutEditorDialog::conflicts() const
{
    QStringList out;
    for (const auto& c : findConflicts(m_defs, m_work)) out << c.message();
    return out;
}

void ShortcutEditorDialog::refreshConflicts()
{
    const auto list = findConflicts(m_defs, m_work);
    QStringList lines;
    const QString id = selectedId();
    for (const auto& c : list)
        if (id.isEmpty() || c.idA == id || c.idB == id) lines << QStringLiteral("⚠ ") + c.message();
    if (lines.isEmpty() && !list.isEmpty()) lines << tr("⚠ %1 conflit(s) ailleurs dans la liste : « Appliquer » sera refusé.").arg(list.size());
    m_conflictLabel->setText(lines.join(QLatin1Char('\n')));
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        bool inConflict = false;
        for (const auto& c : list) inConflict |= c.idA == m_defs[r].id || c.idB == m_defs[r].id;
        m_table->item(r, ColShortcut)->setForeground(inConflict ? QColor(QStringLiteral("#FF5252")) : m_table->palette().text().color());
    }
}

bool ShortcutEditorDialog::apply(QStringList* errors)
{
    QStringList errs;
    const bool ok = m_manager.saveSettings(m_work, &errs);
    if (errors) *errors = errs;
    if (ok)
    {
        m_work = m_manager.settings();
        refreshAllRows();
        m_status->setStyleSheet(QStringLiteral("color:#4CAF50;"));
        m_status->setText(tr("Appliqué et enregistré dans %1").arg(QDir::toNativeSeparators(m_manager.configPath())));
    }
    else
    {
        m_status->setStyleSheet(QStringLiteral("color:#FF5252;"));
        m_status->setText(tr("Non appliqué : %1").arg(errs.join(QStringLiteral(" ; "))));
    }
    return ok;
}

} // namespace TSA::UI
