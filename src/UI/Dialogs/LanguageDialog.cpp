#include "LanguageDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace TSA::UI
{

using TSA::Core::I18n::Language;
using TSA::Core::I18n::LanguageManager;

namespace
{
// Textes affichés dans la langue SÉLECTIONNÉE (LanguageManager::translate) : extraits par lupdate sous ce contexte.
constexpr const char* kContext = "TSA::UI::LanguageDialog";
const char* const kTitle = QT_TRANSLATE_NOOP("TSA::UI::LanguageDialog", "Langue de l'interface");
const char* const kNoteChange = QT_TRANSLATE_NOOP("TSA::UI::LanguageDialog",
    "La nouvelle langue s'applique au redémarrage de TSA. En redémarrant maintenant, les projets ouverts sont fermés "
    "(enregistrement proposé s'ils sont modifiés) puis rouverts.");
const char* const kNoteSame = QT_TRANSLATE_NOOP("TSA::UI::LanguageDialog", "C'est la langue actuelle de l'interface.");
const char* const kNoteUnits = QT_TRANSLATE_NOOP("TSA::UI::LanguageDialog",
    "La langue ne modifie ni les unités, ni les valeurs, ni les résultats de calcul.");
const char* const kRestart = QT_TRANSLATE_NOOP("TSA::UI::LanguageDialog", "Redémarrer maintenant");
const char* const kLater = QT_TRANSLATE_NOOP("TSA::UI::LanguageDialog", "Au prochain démarrage");
const char* const kCancel = QT_TRANSLATE_NOOP("TSA::UI::LanguageDialog", "Annuler");
const char* const kClose = QT_TRANSLATE_NOOP("TSA::UI::LanguageDialog", "Fermer");
} // namespace

LanguageDialog::LanguageDialog(Language current, QWidget* parent)
    : QDialog(parent), m_current(current)
{
    setObjectName("TSALanguageDialog");
    setMinimumWidth(460);

    auto* layout = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    m_list->setObjectName("TSALanguageList");
    for (const auto& info : LanguageManager::languages())
    {
        // Nom dans la langue elle-même, jamais un drapeau seul ; code ISO en complément
        auto* item = new QListWidgetItem(QStringLiteral("%1   (%2)").arg(info.nativeName, info.code.toUpper()), m_list);
        item->setData(Qt::UserRole, static_cast<int>(info.language));
        item->setData(Qt::AccessibleTextRole, info.nativeName);
        if (!LanguageManager::hasTranslation(info.language))
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        if (info.language == current) m_list->setCurrentItem(item);
    }
    m_list->setFixedHeight(m_list->sizeHintForRow(0) * m_list->count() + 2 * m_list->frameWidth() + 4);
    layout->addWidget(m_list);

    m_note = new QLabel(this);
    m_note->setObjectName("TSALanguageNote");
    m_note->setWordWrap(true);
    layout->addWidget(m_note);

    auto* buttons = new QDialogButtonBox(this);
    m_restart = buttons->addButton(QString(), QDialogButtonBox::AcceptRole);
    m_later = buttons->addButton(QString(), QDialogButtonBox::ApplyRole);
    m_cancel = buttons->addButton(QString(), QDialogButtonBox::RejectRole);
    m_restart->setObjectName("TSALanguageRestart");
    m_later->setObjectName("TSALanguageLater");
    layout->addWidget(buttons);

    connect(m_restart, &QPushButton::clicked, this, [this] { m_choice = Choice::RestartNow; accept(); });
    connect(m_later, &QPushButton::clicked, this, [this] { m_choice = Choice::Later; accept(); });
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_list, &QListWidget::currentItemChanged, this, [this] { refreshTexts(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] {
        if (selectedLanguage() != m_current) m_restart->click();
    });
    refreshTexts();
}

Language LanguageDialog::selectedLanguage() const
{
    const QListWidgetItem* item = m_list->currentItem();
    return item ? static_cast<Language>(item->data(Qt::UserRole).toInt()) : m_current;
}

void LanguageDialog::refreshTexts()
{
    const Language lang = selectedLanguage();
    const bool changed = lang != m_current;
    auto t = [lang](const char* source) { return LanguageManager::translate(lang, kContext, source); };
    setWindowTitle(t(kTitle));
    m_note->setText((changed ? t(kNoteChange) : t(kNoteSame)) + QStringLiteral("\n") + t(kNoteUnits));
    m_restart->setText(t(kRestart));
    m_later->setText(t(kLater));
    m_cancel->setText(changed ? t(kCancel) : t(kClose));
    m_restart->setVisible(changed);
    m_later->setVisible(changed);
    m_restart->setDefault(changed);
}

} // namespace TSA::UI
