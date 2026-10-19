#include "SaveProjectTemplateDialog.h"

#include "Project/ProjectTemplates.h"

#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace TSA::UI
{

SaveProjectTemplateDialog::SaveProjectTemplateDialog(const TSA::Project::ProjectTemplateStore& store, const QString& suggestedName, QWidget* parent)
    : QDialog(parent)
    , m_store(store)
{
    setWindowTitle(tr("Enregistrer comme template"));
    setMinimumWidth(520);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(10);

    auto* intro = new QLabel(tr("Le template reprend tout le contenu du projet : grille et axes (coordonnées réelles), niveaux, "
                                "plans de travail, objets, cas de charge et réglages. Les résultats de calcul ne sont jamais "
                                "enregistrés. Les projets créés depuis ce template en sont des copies indépendantes."),
                             this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* form = new QFormLayout();
    m_name = new QLineEdit(suggestedName, this);
    m_name->setObjectName(QStringLiteral("TSATemplateName"));
    m_name->selectAll();
    form->addRow(tr("Nom :"), m_name);
    m_description = new QPlainTextEdit(this);
    m_description->setObjectName(QStringLiteral("TSATemplateDescription"));
    m_description->setPlaceholderText(tr("Description facultative (usage, normes, unités…)"));
    m_description->setFixedHeight(80);
    form->addRow(tr("Description :"), m_description);
    root->addLayout(form);

    auto* folder = new QLabel(tr("Dossier des templates : %1").arg(QDir::toNativeSeparators(store.directory())), this);
    folder->setStyleSheet("color: palette(mid); font-size: 11px;");
    folder->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(folder);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet("font-size: 11px;");
    root->addWidget(m_status);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* btnCancel = new QPushButton(tr("Annuler"), this);
    m_btnSave = new QPushButton(tr("Enregistrer"), this);
    m_btnSave->setObjectName(QStringLiteral("TSATemplateSave"));
    m_btnSave->setDefault(true);
    for (auto* b : { btnCancel, m_btnSave })
    {
        b->setMinimumWidth(96);
        buttons->addWidget(b);
    }
    root->addLayout(buttons);

    connect(m_name, &QLineEdit::textChanged, this, &SaveProjectTemplateDialog::validate);
    connect(m_btnSave, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    validate();
}

QString SaveProjectTemplateDialog::templateName() const
{
    return m_name->text().trimmed();
}

QString SaveProjectTemplateDialog::description() const
{
    return m_description->toPlainText().trimmed();
}

void SaveProjectTemplateDialog::validate()
{
    const QString name = templateName();
    if (TSA::Project::ProjectTemplateStore::fileBaseName(name).isEmpty())
    {
        m_status->setStyleSheet("color: #F85149; font-size: 11px;");
        m_status->setText(tr("Saisissez un nom (les caractères < > : \" / \\ | ? * sont remplacés)."));
        m_btnSave->setEnabled(false);
        return;
    }
    m_btnSave->setEnabled(true);
    if (const auto existing = m_store.find(name))
    {
        m_status->setStyleSheet("color: #D29922; font-size: 11px;");
        m_status->setText(tr("Un template « %1 » existe déjà : son remplacement vous sera demandé.").arg(existing->name));
    }
    else
    {
        m_status->clear();
    }
}

} // namespace TSA::UI
