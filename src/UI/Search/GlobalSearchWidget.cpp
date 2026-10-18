#include "GlobalSearchWidget.h"
#include "UI/Theme/ThemedIcon.h"

#include "../Theme/ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

namespace TSA::UI
{

using TSA::Commands::SearchEntry;
using TSA::Commands::SearchKind;

namespace
{
enum Role
{
    IdRole = Qt::UserRole,
    DescriptionRole,
    LocationRole,
    ShortcutRole,
    KindRole
};
constexpr int kRowHeight = 46;
constexpr int kMaxResults = 12;
constexpr int kDebounceMs = 50;

/// Ligne de résultat : icône | libellé (gras) … raccourci / nature · emplacement — description (atténué).
class ResultDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), kRowHeight);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        p->save();
        const bool selected = option.state & QStyle::State_Selected;
        const bool enabled = index.flags() & Qt::ItemIsEnabled;
        const QPalette& pal = option.palette;
        if (selected) p->fillRect(option.rect, pal.highlight());
        const QColor text = selected ? pal.highlightedText().color() : pal.text().color();
        QColor muted = text;
        muted.setAlphaF(selected ? 0.85 : 0.62);

        QRect r = option.rect.adjusted(10, 4, -10, -4);
        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (!icon.isNull())
        {
            const QRect ir(r.left(), r.top() + (r.height() - 22) / 2, 22, 22);
            icon.paint(p, ir, Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
            r.setLeft(ir.right() + 10);
        }

        QFont bold = option.font;
        bold.setBold(true);
        const QString title = index.data(Qt::DisplayRole).toString();
        const QString shortcut = index.data(ShortcutRole).toString();
        const QFontMetrics fmBold(bold), fm(option.font);
        int shortcutWidth = 0;
        if (!shortcut.isEmpty())
        {
            shortcutWidth = fm.horizontalAdvance(shortcut) + 12;
            p->setPen(muted);
            p->setFont(option.font);
            p->drawText(QRect(r.right() - shortcutWidth, r.top(), shortcutWidth, r.height() / 2), Qt::AlignRight | Qt::AlignVCenter, shortcut);
        }
        p->setPen(enabled ? text : muted);
        p->setFont(bold);
        p->drawText(QRect(r.left(), r.top(), r.width() - shortcutWidth, r.height() / 2), Qt::AlignLeft | Qt::AlignVCenter,
                    fmBold.elidedText(title, Qt::ElideRight, r.width() - shortcutWidth));

        QStringList second;
        const QString kind = index.data(KindRole).toString();
        const QString location = index.data(LocationRole).toString();
        const QString description = index.data(DescriptionRole).toString();
        if (!kind.isEmpty()) second << kind;
        if (!location.isEmpty()) second << location;
        QString line = second.join(QStringLiteral(" · "));
        if (!description.isEmpty()) line += (line.isEmpty() ? QString() : QStringLiteral(" — ")) + description;
        p->setPen(muted);
        p->setFont(option.font);
        p->drawText(QRect(r.left(), r.top() + r.height() / 2, r.width(), r.height() / 2), Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(line, Qt::ElideRight, r.width()));
        p->restore();
    }
};
} // namespace

GlobalSearchWidget::GlobalSearchWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("TSAGlobalSearch");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName("TSAGlobalSearchField");
    m_edit->setClearButtonEnabled(true);
    m_edit->addAction(TSA::UI::themedIcon(QStringLiteral(":/icons/common/search.svg")), QLineEdit::LeadingPosition);
    m_edit->setAccessibleName(tr("Rechercher une commande"));
    m_edit->setAccessibleDescription(tr("Saisir le nom d'une commande, d'un panneau ou d'un paramètre, en anglais, en français ou en malgache"));
    m_edit->installEventFilter(this);
    layout->addWidget(m_edit);
    setShortcutText(QString());

    // Liste des résultats : cadre enfant de la fenêtre (placé au premier plan sous le champ), le champ garde le focus
    m_popup = new QFrame(this);
    m_popup->setObjectName("TSAGlobalSearchPopup");
    m_popup->setFrameShape(QFrame::StyledPanel);
    m_popup->hide();
    auto* popupLayout = new QVBoxLayout(m_popup);
    popupLayout->setContentsMargins(1, 1, 1, 1);
    popupLayout->setSpacing(0);
    m_list = new QListWidget(m_popup);
    m_list->setObjectName("TSAGlobalSearchResults");
    m_list->setAccessibleName(tr("Résultats de la recherche"));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setItemDelegate(new ResultDelegate(m_list));
    m_list->setUniformItemSizes(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setMouseTracking(true);
    popupLayout->addWidget(m_list);
    m_hint = new QLabel(m_popup);
    m_hint->setObjectName("TSAGlobalSearchHint");
    m_hint->setText(tr("↑ ↓ choisir · Entrée exécuter · Échap fermer"));
    m_hint->setContentsMargins(10, 3, 10, 4);
    popupLayout->addWidget(m_hint);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &GlobalSearchWidget::updateResults);
    connect(m_edit, &QLineEdit::textEdited, this, [this] { m_debounce->start(); });
    connect(m_edit, &QLineEdit::textChanged, this, [this](const QString& t) {
        if (t.isEmpty()) hidePopup();   // bouton d'effacement
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { activateRow(m_list->row(item)); });
    connect(m_list, &QListWidget::itemEntered, this, [this](QListWidgetItem* item) {
        if (item->flags() & Qt::ItemIsSelectable) m_list->setCurrentItem(item);
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &GlobalSearchWidget::applyTheme);
    applyTheme(ThemeManager::instance().isDarkMode());
}

void GlobalSearchWidget::setEntriesProvider(EntriesProvider provider)
{
    m_provider = std::move(provider);
    m_indexStale = true;
}

void GlobalSearchWidget::setIconProvider(IconProvider provider)
{
    m_iconProvider = std::move(provider);
}

void GlobalSearchWidget::setShortcutText(const QString& text)
{
    m_shortcutText = text;
    m_edit->setPlaceholderText(text.isEmpty() ? tr("Rechercher une commande…") : tr("Rechercher une commande… (%1)").arg(text));
    m_edit->setToolTip(m_edit->placeholderText());
}

void GlobalSearchWidget::focusSearch()
{
    m_indexStale = true;
    m_edit->setFocus(Qt::ShortcutFocusReason);
    m_edit->selectAll();
    if (!m_edit->text().isEmpty()) updateResults();
}

void GlobalSearchWidget::showFeedback(const QString& message)
{
    m_lastFeedback = message;
    if (message.isEmpty()) return;
    QToolTip::showText(m_edit->mapToGlobal(QPoint(0, m_edit->height() + 2)), message, m_edit, QRect(), 7000);
}

bool GlobalSearchWidget::isPopupVisible() const
{
    return m_popup->isVisible();
}

void GlobalSearchWidget::refreshIndex()
{
    m_index.clear();
    if (m_provider)
        for (const SearchEntry& e : m_provider()) m_index.add(e);
    m_indexStale = false;
}

void GlobalSearchWidget::runSearchNow()
{
    m_debounce->stop();
    updateResults();
}

QStringList GlobalSearchWidget::resultIds() const
{
    QStringList ids;
    for (int i = 0; i < m_list->count(); ++i)
    {
        const QString id = m_list->item(i)->data(IdRole).toString();
        if (!id.isEmpty()) ids << id;
    }
    return ids;
}

QString GlobalSearchWidget::kindLabel(SearchKind kind) const
{
    switch (kind)
    {
    case SearchKind::Command: return tr("Commande");
    case SearchKind::Panel: return tr("Panneau");
    case SearchKind::Setting: return tr("Paramètre");
    case SearchKind::ConsoleCommand: return tr("Console");
    }
    return QString();
}

void GlobalSearchWidget::updateResults()
{
    if (m_indexStale) refreshIndex();
    m_list->clear();
    const QString query = m_edit->text().trimmed();
    if (query.isEmpty())
    {
        hidePopup();
        return;
    }
    const auto hits = m_index.search(query, kMaxResults);
    for (const auto& hit : hits)
    {
        const SearchEntry& e = m_index.entries()[hit.entry];
        auto* item = new QListWidgetItem(e.title, m_list);
        item->setData(IdRole, e.id);
        item->setData(DescriptionRole, e.description);
        item->setData(LocationRole, e.location);
        item->setData(ShortcutRole, e.shortcut);
        item->setData(KindRole, kindLabel(e.kind));
        QIcon icon = m_iconProvider ? m_iconProvider(e) : QIcon();
        if (icon.isNull() && !e.iconPath.isEmpty()) icon = TSA::UI::themedIcon(e.iconPath);
        item->setIcon(icon);
        item->setData(Qt::AccessibleTextRole, e.location.isEmpty() ? e.title : e.title + QStringLiteral(", ") + e.location);
        item->setData(Qt::AccessibleDescriptionRole, e.description);
        item->setToolTip(e.description);
    }
    if (hits.isEmpty())
    {
        auto* item = new QListWidgetItem(tr("Aucun résultat pour « %1 »").arg(query), m_list);
        item->setData(DescriptionRole, tr("Essayez un autre mot, en anglais, en français ou en malgache"));
        item->setFlags(Qt::ItemIsEnabled);
    }
    else
        m_list->setCurrentRow(0);
    showPopup();
}

void GlobalSearchWidget::activateRow(int row)
{
    if (row < 0 || row >= m_list->count()) return;
    const QString id = m_list->item(row)->data(IdRole).toString();
    if (id.isEmpty()) return;
    hidePopup();
    m_lastFeedback.clear();
    emit activated(id);
}

void GlobalSearchWidget::moveSelection(int delta)
{
    const int n = static_cast<int>(resultIds().size());
    if (n == 0) return;
    int row = m_list->currentRow();
    row = row < 0 ? (delta > 0 ? 0 : n - 1) : std::clamp(row + delta, 0, n - 1);
    m_list->setCurrentRow(row);
}

void GlobalSearchWidget::showPopup()
{
    QWidget* host = window();
    if (m_popup->parentWidget() != host)
    {
        m_popup->setParent(host);
        host->installEventFilter(this);   // suivre les redimensionnements de la fenêtre
    }
    positionPopup();
    m_popup->show();
    m_popup->raise();
}

void GlobalSearchWidget::hidePopup()
{
    m_popup->hide();
}

void GlobalSearchWidget::positionPopup()
{
    QWidget* host = m_popup->parentWidget();
    if (!host) return;
    const int rows = std::max(1, m_list->count());
    const int width = std::max(m_edit->width(), 480);
    const int height = rows * kRowHeight + 2 * m_list->frameWidth() + m_hint->sizeHint().height() + 2;
    QPoint pos = m_edit->mapTo(host, QPoint(0, m_edit->height() + 2));
    pos.setX(std::clamp(pos.x() + m_edit->width() - width, 4, std::max(4, host->width() - width - 4)));
    m_popup->setGeometry(pos.x(), pos.y(), width, std::min(height, host->height() - pos.y() - 8));
}

bool GlobalSearchWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_edit)
    {
        // Touches de la recherche réservées au champ : sans cela, Échap déclencherait « Mode sélection » (raccourci de
        // la fenêtre) et Entrée ou les flèches d'autres commandes
        if (event->type() == QEvent::ShortcutOverride)
        {
            const int k = static_cast<QKeyEvent*>(event)->key();
            if (k == Qt::Key_Escape || k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Up || k == Qt::Key_Down
                || k == Qt::Key_PageUp || k == Qt::Key_PageDown)
            {
                event->accept();
                return true;
            }
        }
        if (event->type() == QEvent::KeyPress)
        {
            auto* key = static_cast<QKeyEvent*>(event);
            switch (key->key())
            {
            case Qt::Key_Down:
                // Liste fermée (Échap) : la flèche la rouvre ; ouverte : résultat suivant
                if (!isPopupVisible()) { if (!m_edit->text().trimmed().isEmpty()) updateResults(); }
                else moveSelection(1);
                return true;
            case Qt::Key_Up: moveSelection(-1); return true;
            case Qt::Key_PageDown: moveSelection(5); return true;
            case Qt::Key_PageUp: moveSelection(-5); return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (m_debounce->isActive() || !isPopupVisible()) runSearchNow();
                activateRow(m_list->currentRow());
                return true;
            case Qt::Key_Escape:
                if (isPopupVisible()) hidePopup();
                else if (!m_edit->text().isEmpty()) m_edit->clear();
                else m_edit->clearFocus();   // le focus revient à la fenêtre (vue de modélisation)
                return true;
            default: break;
            }
        }
        else if (event->type() == QEvent::FocusIn)
        {
            m_indexStale = true;   // document, extensions ou raccourcis ont pu changer
        }
        else if (event->type() == QEvent::FocusOut)
        {
            // Un clic dans la liste ne prend pas le focus (NoFocus) : la liste se ferme sur toute autre sortie
            hidePopup();
        }
    }
    else if (event->type() == QEvent::Resize && watched == m_popup->parentWidget() && isPopupVisible())
    {
        positionPopup();
    }
    return QWidget::eventFilter(watched, event);
}

void GlobalSearchWidget::applyTheme(bool dark)
{
    const QString border = dark ? QStringLiteral("#3f3f46") : QStringLiteral("#cbd5e1");
    const QString bg = dark ? QStringLiteral("#1f2329") : QStringLiteral("#ffffff");
    const QString field = dark ? QStringLiteral("#2a2f37") : QStringLiteral("#f1f5f9");
    const QString fg = dark ? QStringLiteral("#e5e7eb") : QStringLiteral("#0f172a");
    const QString focus = dark ? QStringLiteral("#38bdf8") : QStringLiteral("#0284c7");
    m_edit->setStyleSheet(QStringLiteral("QLineEdit#TSAGlobalSearchField { background: %1; color: %2; border: 1px solid %3; border-radius: 4px;"
                                         " padding: 2px 6px; min-height: 22px; }"
                                         "QLineEdit#TSAGlobalSearchField:focus { border-color: %4; }")
                              .arg(field, fg, border, focus));
    m_popup->setStyleSheet(QStringLiteral("QFrame#TSAGlobalSearchPopup { background: %1; border: 1px solid %2; border-radius: 4px; }"
                                          "QListWidget#TSAGlobalSearchResults { background: %1; color: %3; border: none; outline: none; }"
                                          "QLabel#TSAGlobalSearchHint { color: %3; background: %1; border-top: 1px solid %2; font-size: 11px; }")
                               .arg(bg, border, fg));
    QPalette pal = m_list->palette();
    pal.setColor(QPalette::Highlight, dark ? QColor(0x0e, 0x4f, 0x75) : QColor(0xdb, 0xea, 0xfe));
    pal.setColor(QPalette::HighlightedText, dark ? QColor(Qt::white) : QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::Text, QColor(fg));
    m_list->setPalette(pal);
}

} // namespace TSA::UI
