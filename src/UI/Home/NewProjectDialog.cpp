#include "NewProjectDialog.h"
#include "App/ProductInfo.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace TSA::UI
{

namespace
{
const char* kLastDirectoryKey = "StartCenter/lastProjectDirectory";
}

QString NewProjectSettings::filePath() const
{
    return QDir(directory).filePath(name + TSA::Product::projectExtension());
}

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Nouveau projet"));
    setMinimumWidth(560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    auto* heading = new QLabel(tr("Nouveau projet"), this);
    heading->setStyleSheet("font-size: 16px; font-weight: 600;");
    root->addWidget(heading);

    auto* form = new QFormLayout();
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    const QString directory = defaultDirectory();
    m_name = new QLineEdit(uniqueName(directory), this);
    m_name->selectAll();
    form->addRow(tr("Nom :"), m_name);

    auto* dirRow = new QHBoxLayout();
    m_directory = new QLineEdit(QDir::toNativeSeparators(directory), this);
    auto* btnBrowse = new QPushButton(tr("Parcourir…"), this);
    dirRow->addWidget(m_directory, 1);
    dirRow->addWidget(btnBrowse);
    form->addRow(tr("Emplacement :"), dirRow);

    m_template = new QComboBox(this);
    m_template->addItem(tr("Structure générale (grille par défaut)"), static_cast<int>(ProjectTemplate::GeneralStructure));
    m_template->addItem(tr("Modèle vide (sans grille)"), static_cast<int>(ProjectTemplate::Empty));
    form->addRow(tr("Template :"), m_template);
    root->addLayout(form);

    m_target = new QLabel(this);
    m_target->setStyleSheet("color: palette(mid); font-size: 11px;");
    m_target->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_target);

    m_error = new QLabel(this);
    m_error->setStyleSheet("color: #F85149; font-size: 11px;");
    root->addWidget(m_error);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* btnCancel = new QPushButton(tr("Annuler"), this);
    m_btnCreate = new QPushButton(tr("Créer"), this);
    m_btnCreate->setDefault(true);
    for (auto* b : { btnCancel, m_btnCreate }) { b->setMinimumWidth(96); buttons->addWidget(b); }
    root->addLayout(buttons);

    connect(btnBrowse, &QPushButton::clicked, this, &NewProjectDialog::browse);
    connect(m_name, &QLineEdit::textChanged, this, &NewProjectDialog::validate);
    connect(m_directory, &QLineEdit::textChanged, this, &NewProjectDialog::validate);
    connect(m_btnCreate, &QPushButton::clicked, this, &NewProjectDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &NewProjectDialog::reject);
    validate();
}

NewProjectSettings NewProjectDialog::settings() const
{
    NewProjectSettings s;
    s.name = m_name->text().trimmed();
    if (s.name.endsWith(TSA::Product::projectExtension(), Qt::CaseInsensitive))
        s.name.chop(TSA::Product::projectExtension().size());
    s.directory = QDir::cleanPath(QDir::fromNativeSeparators(m_directory->text().trimmed()));
    s.projectTemplate = static_cast<ProjectTemplate>(m_template->currentData().toInt());
    return s;
}

void NewProjectDialog::browse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Emplacement du projet"), settings().directory);
    if (!dir.isEmpty()) m_directory->setText(QDir::toNativeSeparators(dir));
}

void NewProjectDialog::validate()
{
    const NewProjectSettings s = settings();
    QString error;
    static const QRegularExpression invalidChars(QStringLiteral(R"([<>:"/\\|?*])"));
    if (s.name.isEmpty())
        error = tr("Saisissez un nom de projet.");
    else if (s.name.contains(invalidChars))
        error = tr("Le nom ne doit pas contenir les caractères < > : \" / \\ | ? *");
    else if (s.directory.isEmpty() || QDir::isRelativePath(s.directory))
        error = tr("Choisissez un emplacement (chemin absolu).");
    else if (QFileInfo::exists(s.filePath()))
        error = tr("Un projet « %1%2 » existe déjà à cet emplacement.").arg(s.name, TSA::Product::projectExtension());

    m_target->setText(s.name.isEmpty() || s.directory.isEmpty()
                          ? QString()
                          : tr("Fichier : %1").arg(QDir::toNativeSeparators(s.filePath())));
    m_error->setText(error);
    m_btnCreate->setEnabled(error.isEmpty());
}

void NewProjectDialog::accept()
{
    validate();
    if (!m_error->text().isEmpty()) return;
    const NewProjectSettings s = settings();
    if (!QDir().mkpath(s.directory))
    {
        m_error->setText(tr("Impossible de créer le dossier %1").arg(QDir::toNativeSeparators(s.directory)));
        return;
    }
    QSettings().setValue(kLastDirectoryKey, s.directory);
    QDialog::accept();
}

QString NewProjectDialog::defaultDirectory()
{
    const QString last = QSettings().value(kLastDirectoryKey).toString();
    if (!last.isEmpty() && QFileInfo(last).isDir()) return last;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QString::fromLatin1(TSA::Product::kDocumentsFolder));
}

QString NewProjectDialog::uniqueName(const QString& directory)
{
    const QDir dir(directory);
    QString name = tr("Projet 1");
    for (int i = 2; QFileInfo::exists(dir.filePath(name + TSA::Product::projectExtension())); ++i)
        name = tr("Projet %1").arg(i);
    return name;
}

} // namespace TSA::UI
