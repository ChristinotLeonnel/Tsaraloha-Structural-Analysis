#include "BlueprintObjectViewer.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include <map>

namespace TSA::UI
{

namespace
{
std::map<std::string, ObjectViewerFactory>& viewers()
{
    static std::map<std::string, ObjectViewerFactory> map;
    return map;
}

QPlainTextEdit* monospaceText(const QString& text, QWidget* parent, bool wrap = false)
{
    auto* edit = new QPlainTextEdit(text, parent);
    edit->setReadOnly(true);
    edit->setLineWrapMode(wrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    return edit;
}

QDialog* toolWindow(const QString& title, QWidget* parent, const QSize& size)
{
    auto* dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(title);
    dlg->resize(size);
    return dlg;
}
} // namespace

void registerObjectViewer(const std::string& typeName, ObjectViewerFactory factory)
{
    viewers()[typeName] = std::move(factory);
}

bool hasObjectViewer(const std::string& typeName)
{
    return viewers().count(typeName) > 0;
}

void showObjectViewer(const TSA::Blueprint::ObjectRef& object, const QString& title, QWidget* parent)
{
    if (!object) return;
    const QString name = QString::fromStdString(object->typeName());
    auto* dlg = toolWindow(QStringLiteral("%1 — %2").arg(title, name), parent, QSize(900, 620));
    auto* lay = new QVBoxLayout(dlg);
    auto* head = new QLabel(QString::fromStdString(object->summary()), dlg);
    head->setWordWrap(true);
    head->setStyleSheet(QStringLiteral("font-weight: 600;"));
    lay->addWidget(head);
    const QString text = QString::fromStdString(object->details());
    const auto it = viewers().find(object->typeName());
    QWidget* custom = it != viewers().end() && it->second ? it->second(object, dlg) : nullptr;
    if (custom)
    {
        // Vue propre au type et, à côté, le contenu texte (copiable)
        auto* tabs = new QTabWidget(dlg);
        tabs->addTab(custom, QObject::tr("Vue"));
        tabs->addTab(monospaceText(text, tabs), QObject::tr("Texte"));
        lay->addWidget(tabs, 1);
    }
    else
        lay->addWidget(monospaceText(text, dlg), 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    auto* copy = buttons->addButton(QObject::tr("Copier le texte"), QDialogButtonBox::ActionRole);
    QObject::connect(copy, &QPushButton::clicked, dlg, [text] { QApplication::clipboard()->setText(text); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    lay->addWidget(buttons);
    dlg->show();
}

QString resolveSourcePath(const std::string& relativePath)
{
    if (relativePath.empty()) return {};
    QDir dir(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 7; ++level)
    {
        const QString candidate = dir.filePath(QString::fromStdString(relativePath));
        if (QFileInfo::exists(candidate)) return QFileInfo(candidate).absoluteFilePath();
        if (!dir.cdUp()) break;
    }
    return {};
}

void showNodeDocumentation(const TSA::Blueprint::NodeDefinition& d, QWidget* parent)
{
    using TSA::Blueprint::NodeStatus;
    auto* dlg = toolWindow(QObject::tr("Documentation — %1").arg(QString::fromStdString(d.title)), parent, QSize(860, 640));
    auto* lay = new QVBoxLayout(dlg);
    QString status;
    switch (d.status)
    {
    case NodeStatus::Validated: status = QObject::tr("Composant validé : sa logique de calcul est testée contre des solutions de référence."); break;
    case NodeStatus::Experimental: status = QObject::tr("Composant expérimental : non validé, à comparer à une référence avant tout usage."); break;
    case NodeStatus::Standard: status = QObject::tr("Nœud standard de l'éditeur."); break;
    }
    QString pins;
    auto pinList = [&](const std::vector<TSA::Blueprint::PinSpec>& list, const QString& title) {
        QString rows;
        for (const auto& p : list)
        {
            if (p.kind == TSA::Blueprint::PinKind::Exec) continue;
            rows += QStringLiteral("<li><b>%1</b> (%2)</li>").arg(QString::fromStdString(p.label).toHtmlEscaped(),
                                                                QString::fromStdString(TSA::Blueprint::pinTypeName(p)).toHtmlEscaped());
        }
        if (!rows.isEmpty()) pins += QStringLiteral("<p><b>%1</b></p><ul>%2</ul>").arg(title, rows);
    };
    pinList(d.inputs, QObject::tr("Entrées"));
    pinList(d.outputs, QObject::tr("Sorties"));
    auto* head = new QLabel(QStringLiteral("<h3>%1</h3><p>%2</p><p><i>%3</i></p><p>Identifiant : <code>%4</code> — version %5</p>%6")
                                .arg(QString::fromStdString(d.title).toHtmlEscaped(), QString::fromStdString(d.description).toHtmlEscaped(), status,
                                     QString::fromStdString(d.id).toHtmlEscaped())
                                .arg(d.version)
                                .arg(pins),
                            dlg);
    head->setWordWrap(true);
    head->setTextFormat(Qt::RichText);
    lay->addWidget(head);
    auto* tabs = new QTabWidget(dlg);
    tabs->addTab(monospaceText(d.documentation.empty() ? QObject::tr("(pas de documentation détaillée)") : QString::fromStdString(d.documentation), tabs, true),
                 QObject::tr("Formulation, hypothèses, limites"));
    if (!d.source.empty())
    {
        const QString path = resolveSourcePath(d.source);
        QString code;
        if (!path.isEmpty())
        {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly)) code = QString::fromUtf8(f.readAll());
        }
        const QString header = QObject::tr("Implémentation : %1").arg(QString::fromStdString(d.source));
        tabs->addTab(monospaceText(code.isEmpty() ? header + QObject::tr("\n\nSource absente de ce poste (paquet distribué) : consulter le dépôt.")
                                                  : header + QStringLiteral("\n") + path + QStringLiteral("\n\n") + code,
                                   tabs),
                     QObject::tr("Code source"));
    }
    lay->addWidget(tabs, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    lay->addWidget(buttons);
    dlg->show();
}

} // namespace TSA::UI
