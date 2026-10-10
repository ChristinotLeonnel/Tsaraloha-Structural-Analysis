#include "ReportTemplatesDialog.h"

#include "../../Reports/DocumentRenderer.h"
#include "../../Templates/TemplateRepository.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace TSA::UI
{

using namespace TSA::Templates;

namespace
{
constexpr int kKeyRole = Qt::UserRole;
constexpr int kReportRole = Qt::UserRole + 1;

TemplateRepository& repo() { return TemplateRepository::instance(); }
} // namespace

ReportTemplatesDialog::ReportTemplatesDialog(std::function<QJsonObject(const QString&)> dataProvider, QWidget* parent)
    : QDialog(parent)
    , m_dataProvider(std::move(dataProvider))
{
    setWindowTitle(tr("Documents et templates"));
    resize(1200, 760);
    auto* layout = new QVBoxLayout(this);
    auto* split = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(split, 1);

    // --- Gauche : dépôt et options --------------------------------------------------------------------
    auto* left = new QWidget(split);
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    m_tree = new QTreeWidget(left);
    m_tree->setHeaderLabels({ tr("Template / rapport"), tr("Origine"), tr("Version"), tr("État") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ll->addWidget(m_tree, 3);
    m_info = new QLabel(left);
    m_info->setWordWrap(true);
    m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ll->addWidget(m_info);
    auto* manage = new QHBoxLayout;
    auto* importBtn = new QPushButton(tr("Importer..."), left);
    auto* exportTplBtn = new QPushButton(tr("Exporter le template..."), left);
    m_customize = new QPushButton(tr("Personnaliser"), left);
    m_customize->setToolTip(tr("Enregistrer les options actuelles comme valeurs par défaut d'une copie personnalisée"));
    m_edit = new QPushButton(tr("Modifier un fichier..."), left);
    m_edit->setToolTip(tr("Modifier le texte d'un fichier du template (copie personnalisée, validée avant enregistrement)"));
    m_remove = new QPushButton(tr("Supprimer"), left);
    for (auto* b : { importBtn, exportTplBtn, m_customize, m_edit, m_remove }) manage->addWidget(b);
    ll->addLayout(manage);
    auto* optGroup = new QGroupBox(tr("Options du document"), left);
    m_optionsForm = new QFormLayout(optGroup);
    m_optionsBox = optGroup;
    ll->addWidget(optGroup, 2);
    split->addWidget(left);

    // --- Droite : aperçu et diagnostics -----------------------------------------------------------------
    auto* right = new QWidget(split);
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    m_preview = new QTextBrowser(right);
    m_preview->setOpenLinks(false);
    rl->addWidget(m_preview, 4);
    m_diagnostics = new QPlainTextEdit(right);
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setMaximumHeight(130);
    rl->addWidget(m_diagnostics, 1);
    auto* out = new QHBoxLayout;
    auto* previewBtn = new QPushButton(tr("Actualiser l'aperçu"), right);
    m_pageSize = new QComboBox(right);
    m_pageSize->addItems({ tr("A4 portrait"), tr("A4 paysage"), tr("A3 portrait"), tr("A3 paysage") });
    auto* htmlBtn = new QPushButton(tr("Exporter HTML..."), right);
    auto* pdfBtn = new QPushButton(tr("Exporter PDF..."), right);
    out->addWidget(previewBtn);
    out->addStretch();
    out->addWidget(new QLabel(tr("Page PDF :"), right));
    out->addWidget(m_pageSize);
    out->addWidget(htmlBtn);
    out->addWidget(pdfBtn);
    rl->addLayout(out);
    split->addWidget(right);
    split->setStretchFactor(1, 2);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(box);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &ReportTemplatesDialog::onSelectionChanged);
    connect(previewBtn, &QPushButton::clicked, this, &ReportTemplatesDialog::preview);
    connect(htmlBtn, &QPushButton::clicked, this, [this] { exportDocument(false); });
    connect(pdfBtn, &QPushButton::clicked, this, [this] { exportDocument(true); });
    connect(importBtn, &QPushButton::clicked, this, &ReportTemplatesDialog::importTemplate);
    connect(exportTplBtn, &QPushButton::clicked, this, &ReportTemplatesDialog::exportTemplate);
    connect(m_customize, &QPushButton::clicked, this, &ReportTemplatesDialog::customize);
    connect(m_edit, &QPushButton::clicked, this, &ReportTemplatesDialog::editFile);
    connect(m_remove, &QPushButton::clicked, this, &ReportTemplatesDialog::removeTemplate);
    populate();
}

void ReportTemplatesDialog::populate(const QString& selectKey, const QString& selectReport)
{
    m_tree->blockSignals(true);
    m_tree->clear();
    QTreeWidgetItem* toSelect = nullptr;
    for (const auto& e : repo().entries())
    {
        const auto& m = e.package.manifest();
        const auto* eff = repo().effective(m.id);
        QString state = e.modified ? tr("modifié") : tr("d'origine");
        if (e.origin != TemplateOrigin::Archived && eff != &e) state += tr(" · remplacé");
        if (e.origin == TemplateOrigin::Module) state += tr(" · module %1").arg(e.moduleId);
        auto* item = new QTreeWidgetItem(m_tree, { m.name, templateOriginName(e.origin), m.version, state });
        item->setData(0, kKeyRole, e.key());
        item->setToolTip(0, QStringLiteral("%1\n%2\n%3").arg(m.id, m.description, e.path));
        for (const auto& r : m.reports)
        {
            auto* child = new QTreeWidgetItem(item, { r.name });
            child->setData(0, kKeyRole, e.key());
            child->setData(0, kReportRole, r.id);
            child->setToolTip(0, r.description);
            if (e.key() == selectKey && (selectReport.isEmpty() || selectReport == r.id) && !toSelect) toSelect = child;
        }
        item->setExpanded(e.origin != TemplateOrigin::Archived);
    }
    if (!toSelect && m_tree->topLevelItemCount() && m_tree->topLevelItem(0)->childCount()) toSelect = m_tree->topLevelItem(0)->child(0);
    m_tree->blockSignals(false);
    if (toSelect) m_tree->setCurrentItem(toSelect);
    QStringList problems;
    for (const auto& p : repo().problems()) problems << tr("Template refusé : %1 — %2").arg(p.path, p.errors.join(QStringLiteral(" ; ")));
    if (!problems.isEmpty()) m_diagnostics->setPlainText(problems.join('\n'));
}

QString ReportTemplatesDialog::currentKey() const
{
    return m_tree->currentItem() ? m_tree->currentItem()->data(0, kKeyRole).toString() : QString();
}

QString ReportTemplatesDialog::currentReport() const
{
    auto* item = m_tree->currentItem();
    if (!item) return {};
    if (!item->data(0, kReportRole).toString().isEmpty()) return item->data(0, kReportRole).toString();
    return item->childCount() ? item->child(0)->data(0, kReportRole).toString() : QString();
}

void ReportTemplatesDialog::onSelectionChanged()
{
    const auto* e = repo().entryByKey(currentKey());
    m_customize->setEnabled(e && e->origin != TemplateOrigin::Archived);
    m_edit->setEnabled(e && e->origin != TemplateOrigin::Archived);
    m_remove->setEnabled(e && e->origin != TemplateOrigin::BuiltIn && e->origin != TemplateOrigin::Module);
    if (e)
    {
        const auto& m = e->package.manifest();
        m_info->setText(tr("<b>%1</b> %2 — origine : %3%4<br>Données : %5 · licence : %6 · auteur : %7%8")
                            .arg(m.id.toHtmlEscaped(), m.version.toHtmlEscaped(), templateOriginName(e->origin), e->modified ? tr(" (modifié)") : QString(),
                                 m.dataSchema, m.license.toHtmlEscaped(), m.author.toHtmlEscaped(),
                                 m.basedOn.isEmpty() ? QString() : tr("<br>Personnalisation de %1").arg(m.basedOn.toHtmlEscaped())));
    }
    rebuildOptions();
    preview();
}

void ReportTemplatesDialog::rebuildOptions()
{
    while (m_optionsForm->rowCount()) m_optionsForm->removeRow(0);
    m_optionWidgets.clear();
    m_imageValues.clear();
    const auto* e = repo().entryByKey(currentKey());
    if (!e) return;
    for (const auto& o : e->package.manifest().options)
    {
        QWidget* w = nullptr;
        if (o.type == "bool")
        {
            auto* c = new QCheckBox(m_optionsBox);
            c->setChecked(o.defaultValue.toBool());
            connect(c, &QCheckBox::toggled, this, &ReportTemplatesDialog::preview);
            w = c;
        }
        else if (o.type == "number")
        {
            auto* s = new QDoubleSpinBox(m_optionsBox);
            s->setRange(-1e9, 1e9);
            s->setValue(o.defaultValue.toDouble());
            w = s;
        }
        else if (o.type == "choice")
        {
            auto* c = new QComboBox(m_optionsBox);
            c->addItems(o.choices);
            c->setCurrentText(o.defaultValue.toString());
            connect(c, &QComboBox::currentTextChanged, this, &ReportTemplatesDialog::preview);
            w = c;
        }
        else if (o.type == "image")
        {
            auto* holder = new QWidget(m_optionsBox);
            auto* h = new QHBoxLayout(holder);
            h->setContentsMargins(0, 0, 0, 0);
            auto* label = new QLabel(o.defaultValue.toString().isEmpty() ? tr("aucun") : tr("image du template"), holder);
            auto* pick = new QPushButton(tr("Choisir..."), holder);
            auto* clear = new QPushButton(tr("Retirer"), holder);
            h->addWidget(label, 1);
            h->addWidget(pick);
            h->addWidget(clear);
            m_imageValues[o.id] = o.defaultValue.toString();
            const QString id = o.id;
            connect(pick, &QPushButton::clicked, this, [this, id, label] {
                const QString file = QFileDialog::getOpenFileName(this, tr("Choisir une image"), QString(), tr("Images (*.png *.jpg *.jpeg *.svg)"));
                if (file.isEmpty()) return;
                QFile f(file);
                if (!f.open(QIODevice::ReadOnly) || f.size() > 4 * 1024 * 1024)
                {
                    QMessageBox::warning(this, tr("Image"), tr("Image illisible ou trop volumineuse (4 Mo au plus)."));
                    return;
                }
                const QString ext = QFileInfo(file).suffix().toLower();
                const QString mime = ext == "svg" ? "image/svg+xml" : ext == "png" ? "image/png" : "image/jpeg";
                m_imageValues[id] = QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(f.readAll().toBase64()));   // document autonome
                label->setText(QFileInfo(file).fileName());
                preview();
            });
            connect(clear, &QPushButton::clicked, this, [this, id, label] {
                m_imageValues[id].clear();
                label->setText(tr("aucun"));
                preview();
            });
            w = holder;
        }
        else
        {
            auto* le = new QLineEdit(o.defaultValue.toString(), m_optionsBox);
            if (o.type == "color") le->setPlaceholderText(QStringLiteral("#rrggbb"));
            connect(le, &QLineEdit::editingFinished, this, &ReportTemplatesDialog::preview);
            w = le;
            if (o.type == "color")
            {
                auto* holder = new QWidget(m_optionsBox);
                auto* h = new QHBoxLayout(holder);
                h->setContentsMargins(0, 0, 0, 0);
                auto* pick = new QPushButton(tr("..."), holder);
                le->setParent(holder);
                h->addWidget(le, 1);
                h->addWidget(pick);
                connect(pick, &QPushButton::clicked, this, [this, le] {
                    const QColor c = QColorDialog::getColor(QColor(le->text()), this);
                    if (c.isValid()) le->setText(c.name()), preview();
                });
                m_optionWidgets[o.id] = le;
                m_optionsForm->addRow(o.label.isEmpty() ? o.id : o.label, holder);
                continue;
            }
        }
        m_optionWidgets[o.id] = w;
        m_optionsForm->addRow(o.label.isEmpty() ? o.id : o.label, w);
    }
    if (e->package.manifest().options.empty()) m_optionsForm->addRow(new QLabel(tr("Ce template n'a pas d'option."), m_optionsBox));
}

QJsonObject ReportTemplatesDialog::optionValues() const
{
    QJsonObject values;
    const auto* e = repo().entryByKey(currentKey());
    if (!e) return values;
    for (const auto& o : e->package.manifest().options)
    {
        if (o.type == "image")
        {
            values.insert(o.id, m_imageValues.count(o.id) ? m_imageValues.at(o.id) : QString());
            continue;
        }
        auto it = m_optionWidgets.find(o.id);
        if (it == m_optionWidgets.end()) continue;
        if (auto* c = qobject_cast<QCheckBox*>(it->second)) values.insert(o.id, c->isChecked());
        else if (auto* s = qobject_cast<QDoubleSpinBox*>(it->second)) values.insert(o.id, s->value());
        else if (auto* cb = qobject_cast<QComboBox*>(it->second)) values.insert(o.id, cb->currentText());
        else if (auto* le = qobject_cast<QLineEdit*>(it->second)) values.insert(o.id, le->text());
    }
    return values;
}

QString ReportTemplatesDialog::renderCurrent(bool showErrors)
{
    const auto* e = repo().entryByKey(currentKey());
    if (!e) return {};
    const QString schema = e->package.manifest().dataSchema;
    if (!m_dataCache.count(schema)) m_dataCache[schema] = m_dataProvider ? m_dataProvider(schema) : QJsonObject();
    const QJsonObject& reportData = m_dataCache[schema];
    QStringList diag;
    if (reportData.isEmpty())
    {
        diag << tr("Aucune donnée « %1 » disponible pour ce projet.").arg(schema);
        m_diagnostics->setPlainText(diag.join('\n'));
        return {};
    }
    const auto r = TSA::Reports::DocumentRenderer::render(e->package, currentReport(), reportData, optionValues());
    for (const auto& x : r.errors) diag << tr("ERREUR : %1").arg(x);
    for (const auto& x : r.warnings) diag << tr("Avertissement : %1").arg(x);
    for (const auto& x : r.missingVariables) diag << tr("Donnée absente (affichée vide) : %1").arg(x);
    if (r.ok())
        for (const auto& x : TSA::Reports::DocumentRenderer::externalReferences(r.output)) diag << tr("Ressource externe (document non autonome) : %1").arg(x);
    if (diag.isEmpty()) diag << tr("Rendu sans erreur — document autonome (%1 caractères).").arg(r.output.size());
    m_diagnostics->setPlainText(diag.join('\n'));
    if (!r.ok() && showErrors) QMessageBox::warning(this, tr("Rendu du document"), r.errors.join('\n'));
    return r.ok() ? r.output : QString();
}

void ReportTemplatesDialog::preview()
{
    m_lastHtml = renderCurrent(false);
    m_preview->setHtml(m_lastHtml.isEmpty() ? tr("<p><i>Aperçu indisponible : voir les diagnostics.</i></p>") : m_lastHtml);
}

void ReportTemplatesDialog::exportDocument(bool pdf)
{
    const QString html = renderCurrent(true);
    if (html.isEmpty()) return;
    QString path = QFileDialog::getSaveFileName(this, pdf ? tr("Exporter en PDF") : tr("Exporter en HTML"), currentReport() + (pdf ? ".pdf" : ".html"),
                                                pdf ? tr("PDF (*.pdf)") : tr("HTML (*.html *.htm)"));
    if (path.isEmpty()) return;
    QString error;
    TSA::Reports::PageSetup page;
    page.size = m_pageSize->currentIndex() >= 2 ? QStringLiteral("A3") : QStringLiteral("A4");
    page.landscape = m_pageSize->currentIndex() % 2 == 1;
    const bool ok = pdf ? TSA::Reports::DocumentRenderer::writePdf(html, path, page, &error) : TSA::Reports::DocumentRenderer::writeHtml(html, path, &error);
    if (ok) QMessageBox::information(this, tr("Export"), tr("Document enregistré :\n%1").arg(path));
    else QMessageBox::critical(this, tr("Export"), error);
}

void ReportTemplatesDialog::importTemplate()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Importer un template"), QString(), tr("Templates (*.tsatemplate)"));
    if (path.isEmpty()) return;
    QString key;
    QStringList errors;
    if (!repo().importPackage(path, &key, &errors))
    {
        QMessageBox::critical(this, tr("Importer un template"), tr("Template refusé :\n\n%1").arg(errors.join('\n')));
        return;
    }
    populate(key);
    QMessageBox::information(this, tr("Importer un template"),
                             tr("Template importé et validé (aucun code exécutable). Une version précédente du même template a, le cas échéant, été "
                                "conservée dans « ancienne version »."));
}

void ReportTemplatesDialog::exportTemplate()
{
    const auto* e = repo().entryByKey(currentKey());
    if (!e) return;
    const auto& m = e->package.manifest();
    const QString path = QFileDialog::getSaveFileName(this, tr("Exporter le template"), m.id + "-" + m.version + ".tsatemplate", tr("Templates (*.tsatemplate)"));
    if (path.isEmpty()) return;
    QString error;
    if (!repo().exportPackage(e->key(), path, &error)) QMessageBox::critical(this, tr("Exporter le template"), error);
}

void ReportTemplatesDialog::customize()
{
    const auto* e = repo().entryByKey(currentKey());
    if (!e) return;
    TemplatePackage pkg = e->package;
    const QJsonObject values = optionValues();
    for (auto& o : pkg.manifest().options)
        if (values.contains(o.id)) o.defaultValue = values.value(o.id);
    QString key;
    QStringList errors;
    const QString report = currentReport();
    if (!repo().saveCustomized(e->key(), pkg, &key, &errors))
    {
        QMessageBox::critical(this, tr("Personnaliser"), errors.join('\n'));
        return;
    }
    populate(key, report);
}

void ReportTemplatesDialog::editFile()
{
    const auto* e = repo().entryByKey(currentKey());
    if (!e) return;
    QStringList textFiles;
    for (const QString& f : e->package.files())
        if (QStringList { "html", "htm", "css", "txt", "md", "svg", "json" }.contains(QFileInfo(f).suffix().toLower())) textFiles << f;
    bool ok = false;
    const QString file = QInputDialog::getItem(this, tr("Modifier un fichier"), tr("Fichier du template :"), textFiles, 0, false, &ok);
    if (!ok || file.isEmpty()) return;

    QDialog editor(this);
    editor.setWindowTitle(tr("%1 — %2").arg(e->package.manifest().id, file));
    editor.resize(900, 650);
    auto* l = new QVBoxLayout(&editor);
    auto* note = new QLabel(tr("Syntaxe : {{variable}}, {{{html}}}, {{#section}}…{{/section}}, {{^absent}}…{{/absent}}, {{> partiel}}. "
                               "Aucun script ni ressource externe n'est accepté. Une copie personnalisée est créée ; l'original est conservé."),
                            &editor);
    note->setWordWrap(true);
    l->addWidget(note);
    auto* text = new QPlainTextEdit(e->package.fileText(file), &editor);
    text->setFont(QFont(QStringLiteral("Consolas"), 10));
    l->addWidget(text, 1);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &editor);
    l->addWidget(bb);
    const QString baseKey = e->key(), report = currentReport();
    TemplatePackage base = e->package;
    connect(bb, &QDialogButtonBox::rejected, &editor, &QDialog::reject);
    connect(bb, &QDialogButtonBox::accepted, &editor, [&] {
        TemplatePackage pkg = base;
        pkg.setFile(file, text->toPlainText().toUtf8());
        QString key;
        QStringList errors;
        if (!repo().saveCustomized(baseKey, pkg, &key, &errors))
        {
            QMessageBox::critical(&editor, tr("Modifier un fichier"), tr("Modification refusée :\n\n%1").arg(errors.join('\n')));
            return;
        }
        editor.accept();
        populate(key, report);
    });
    editor.exec();
}

void ReportTemplatesDialog::removeTemplate()
{
    const auto* e = repo().entryByKey(currentKey());
    if (!e) return;
    if (QMessageBox::question(this, tr("Supprimer"), tr("Supprimer « %1 » (%2) ?\n%3").arg(e->package.manifest().name, templateOriginName(e->origin), e->path))
        != QMessageBox::Yes)
        return;
    QString error;
    if (!repo().remove(e->key(), &error)) QMessageBox::critical(this, tr("Supprimer"), error);
    populate();
}

} // namespace TSA::UI
