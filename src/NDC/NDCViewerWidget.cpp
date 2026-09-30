#include "NDCViewerWidget.h"
#include "NDCGenerator.h"
#include "NDCExporter.h"
#include "../Model/Model.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>

namespace TSA::NDC
{

NDCViewerWidget::NDCViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void NDCViewerWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Barre d'outils supérieure
    auto* toolbar = new QWidget(this);
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(4, 2, 4, 2);
    tbLayout->setSpacing(6);

    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setPlaceholderText(tr("Rechercher dans la note..."));
    m_searchEdit->setMaximumWidth(220);

    m_btnFindPrev = new QPushButton(tr("▲"), toolbar);
    m_btnFindPrev->setToolTip(tr("Occurence précédente"));
    m_btnFindNext = new QPushButton(tr("▼"), toolbar);
    m_btnFindNext->setToolTip(tr("Occurence suivante"));

    m_btnZoomIn = new QPushButton(tr("A+"), toolbar);
    m_btnZoomIn->setToolTip(tr("Agrandir le texte"));
    m_btnZoomOut = new QPushButton(tr("A-"), toolbar);
    m_btnZoomOut->setToolTip(tr("Réduire le texte"));

    m_btnExportPdf = new QPushButton(tr("Exporter PDF..."), toolbar);
    m_btnExportPdf->setStyleSheet("font-weight: 600; color: #1a56db;");
    m_btnExportHtml = new QPushButton(tr("Exporter HTML..."), toolbar);
    m_btnRefresh = new QPushButton(tr("Actualiser"), toolbar);

    tbLayout->addWidget(new QLabel(tr("Recherche :"), toolbar));
    tbLayout->addWidget(m_searchEdit);
    tbLayout->addWidget(m_btnFindPrev);
    tbLayout->addWidget(m_btnFindNext);
    tbLayout->addWidget(m_btnZoomIn);
    tbLayout->addWidget(m_btnZoomOut);
    tbLayout->addStretch();
    tbLayout->addWidget(m_btnRefresh);
    tbLayout->addWidget(m_btnExportHtml);
    tbLayout->addWidget(m_btnExportPdf);

    mainLayout->addWidget(toolbar);

    // Splitter central (Sommaire à gauche, Document à droite)
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_tocTree = new QTreeWidget(m_splitter);
    m_tocTree->setHeaderLabel(tr("Sommaire de la Note"));
    m_tocTree->setMinimumWidth(220);
    m_tocTree->setMaximumWidth(360);

    m_browser = new QTextBrowser(m_splitter);
    m_browser->setOpenExternalLinks(false);

    m_splitter->addWidget(m_tocTree);
    m_splitter->addWidget(m_browser);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(m_splitter, 1);

    connect(m_tocTree, &QTreeWidget::itemClicked, this, &NDCViewerWidget::onTocItemClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &NDCViewerWidget::onSearchTextChanged);
    connect(m_btnFindPrev, &QPushButton::clicked, this, &NDCViewerWidget::onFindPrevious);
    connect(m_btnFindNext, &QPushButton::clicked, this, &NDCViewerWidget::onFindNext);
    connect(m_btnZoomIn, &QPushButton::clicked, this, &NDCViewerWidget::onZoomIn);
    connect(m_btnZoomOut, &QPushButton::clicked, this, &NDCViewerWidget::onZoomOut);
    connect(m_btnExportPdf, &QPushButton::clicked, this, &NDCViewerWidget::onExportPdf);
    connect(m_btnExportHtml, &QPushButton::clicked, this, &NDCViewerWidget::onExportHtml);
    connect(m_btnRefresh, &QPushButton::clicked, this, &NDCViewerWidget::refreshDocument);
}

void NDCViewerWidget::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshDocument();
}

void NDCViewerWidget::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_results = results;
    refreshDocument();
}

void NDCViewerWidget::refreshDocument()
{
    if (!m_model) return;

    m_document = NDCGenerator::generate(*m_model, m_results);
    m_browser->setHtml(m_document.toHtml());
    populateToc();
}

void NDCViewerWidget::populateToc()
{
    m_tocTree->clear();
    for (const auto& ch : m_document.chapters)
    {
        auto* chItem = new QTreeWidgetItem(m_tocTree);
        chItem->setText(0, QString("%1. %2").arg(ch.number).arg(ch.title));
        chItem->setData(0, Qt::UserRole, QString("chap_%1").arg(ch.number));
        chItem->setExpanded(true);

        for (size_t s = 0; s < ch.sections.size(); ++s)
        {
            const auto& sec = ch.sections[s];
            if (sec.title.isEmpty()) continue;
            auto* sItem = new QTreeWidgetItem(chItem);
            sItem->setText(0, QString("%1.%2 %3").arg(ch.number).arg(s + 1).arg(sec.title));
            sItem->setData(0, Qt::UserRole, QString("sec_%1_%2").arg(ch.number).arg(s + 1));
        }
    }
}

void NDCViewerWidget::onTocItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    QString anchor = item->data(0, Qt::UserRole).toString();
    if (!anchor.isEmpty())
    {
        m_browser->scrollToAnchor(anchor);
    }
}

void NDCViewerWidget::onSearchTextChanged(const QString& text)
{
    if (text.isEmpty()) return;
    m_browser->find(text);
}

void NDCViewerWidget::onFindNext()
{
    QString txt = m_searchEdit->text();
    if (!txt.isEmpty())
    {
        m_browser->find(txt);
    }
}

void NDCViewerWidget::onFindPrevious()
{
    QString txt = m_searchEdit->text();
    if (!txt.isEmpty())
    {
        m_browser->find(txt, QTextDocument::FindBackward);
    }
}

void NDCViewerWidget::onZoomIn()
{
    m_browser->zoomIn(1);
}

void NDCViewerWidget::onZoomOut()
{
    m_browser->zoomOut(1);
}

void NDCViewerWidget::onResetZoom()
{
    m_browser->setHtml(m_document.toHtml());
}

void NDCViewerWidget::onExportPdf()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Exporter la Note de Calcul au format PDF"),
        "Note_de_Calcul_TSA.pdf",
        tr("Documents PDF (*.pdf)")
    );

    if (fileName.isEmpty()) return;

    QString error;
    if (NDCExporter::exportToPdf(m_document, fileName, &error))
    {
        QMessageBox::information(
            this,
            tr("Export PDF Réussi"),
            tr("La note de calcul a été exportée avec succès sous :\n%1").arg(fileName)
        );
    }
    else
    {
        QMessageBox::critical(
            this,
            tr("Échec de l'export PDF"),
            tr("Une erreur est survenue lors de l'export PDF :\n%1").arg(error)
        );
    }
}

void NDCViewerWidget::onExportHtml()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Exporter la Note de Calcul au format HTML"),
        "Note_de_Calcul_TSA.html",
        tr("Fichiers Web HTML (*.html)")
    );

    if (fileName.isEmpty()) return;

    QString error;
    if (NDCExporter::exportToHtml(m_document, fileName, &error))
    {
        QMessageBox::information(
            this,
            tr("Export HTML Réussi"),
            tr("La note de calcul a été exportée avec succès sous :\n%1").arg(fileName)
        );
    }
    else
    {
        QMessageBox::critical(
            this,
            tr("Échec de l'export HTML"),
            tr("Une erreur est survenue lors de l'export HTML :\n%1").arg(error)
        );
    }
}

} // namespace TSA::NDC
