#include "BlueprintSubgraphDialog.h"

#include "Blueprint/BlueprintRuntime.h"
#include "Blueprint/BlueprintSubgraph.h"

#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <set>

namespace TSA::UI
{

using namespace TSA::Blueprint;

namespace
{
enum Column
{
    kName = 0,
    kLabel,
    kType,
    kObject,
    kDefault,
    kRequired
};

struct TypeChoice
{
    const char* id;
    const char* label;
};
// Libellés traduits à l'affichage par tr(t.label)
constexpr TypeChoice kTypes[] = { { "real", QT_TRANSLATE_NOOP("TSA::UI::BlueprintSubgraphDialog", "Réel") },
                                  { "integer", QT_TRANSLATE_NOOP("TSA::UI::BlueprintSubgraphDialog", "Entier") },
                                  { "bool", QT_TRANSLATE_NOOP("TSA::UI::BlueprintSubgraphDialog", "Booléen") },
                                  { "text", QT_TRANSLATE_NOOP("TSA::UI::BlueprintSubgraphDialog", "Texte") },
                                  { "point", QT_TRANSLATE_NOOP("TSA::UI::BlueprintSubgraphDialog", "Point (x, y, z)") },
                                  { "ids", QT_TRANSLATE_NOOP("TSA::UI::BlueprintSubgraphDialog", "Identifiants") },
                                  { "object", QT_TRANSLATE_NOOP("TSA::UI::BlueprintSubgraphDialog", "Objet") },
                                  { "any", QT_TRANSLATE_NOOP("TSA::UI::BlueprintSubgraphDialog", "Générique") } };

QString typeIdOf(const PinSpec& p)
{
    return p.any ? QStringLiteral("any") : QString::fromLatin1(valueTypeId(p.type));
}

double parseReal(QString text, bool& ok)
{
    return QLocale::c().toDouble(text.trimmed().replace(QLatin1Char(','), QLatin1Char('.')), &ok);
}
} // namespace

BlueprintSubgraphDialog::BlueprintSubgraphDialog(const SubgraphDefinition& d, const QStringList& objectTypes, QWidget* parent)
    : QDialog(parent)
    , m_original(d)
    , m_result(d)
    , m_objectTypes(objectTypes)
{
    setWindowTitle(tr("Propriétés du sous-graphe « %1 »").arg(QString::fromStdString(d.name.empty() ? d.id : d.name)));
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    auto* id = new QLabel(QStringLiteral("%1   (version %2)").arg(QString::fromStdString(d.id)).arg(d.version), this);
    id->setToolTip(tr("Identifiant stable du sous-graphe (fichiers, instances « sub.%1 ») : non modifiable").arg(QString::fromStdString(d.id)));
    form->addRow(tr("Identifiant"), id);
    m_name = new QLineEdit(QString::fromStdString(d.name), this);
    form->addRow(tr("Nom"), m_name);
    m_category = new QLineEdit(QString::fromStdString(d.category), this);
    m_category->setToolTip(tr("Catégorie de la palette où apparaissent les instances"));
    form->addRow(tr("Catégorie"), m_category);
    m_description = new QLineEdit(QString::fromStdString(d.description), this);
    form->addRow(tr("Description"), m_description);
    m_documentation = new QPlainTextEdit(QString::fromStdString(d.documentation), this);
    m_documentation->setPlaceholderText(tr("Formulation, hypothèses, unités, limites (affichée par F1 sur une instance)"));
    m_documentation->setMaximumHeight(110);
    form->addRow(tr("Documentation"), m_documentation);
    root->addLayout(form);

    auto* tabs = new QTabWidget(this);
    auto page = [&](const std::vector<PinSpec>& pins, bool inputs) {
        auto* w = new QWidget(tabs);
        auto* lay = new QVBoxLayout(w);
        QTableWidget* table = makeTable(pins, inputs);
        lay->addWidget(table, 1);
        auto* buttons = new QHBoxLayout();
        auto* add = new QPushButton(inputs ? tr("Ajouter une entrée") : tr("Ajouter une sortie"), w);
        auto* remove = new QPushButton(tr("Supprimer"), w);
        auto* up = new QPushButton(tr("Monter"), w);
        auto* down = new QPushButton(tr("Descendre"), w);
        buttons->addWidget(add);
        buttons->addWidget(remove);
        buttons->addWidget(up);
        buttons->addWidget(down);
        buttons->addStretch();
        lay->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, [this, table, inputs] {
            PinSpec p;
            p.name = (inputs ? "entree" : "sortie") + std::to_string(table->rowCount() + 1);
            p.label = p.name;
            addRow(table, p, inputs);
            table->setCurrentCell(table->rowCount() - 1, kName);
        });
        connect(remove, &QPushButton::clicked, this, [table] {
            if (table->currentRow() >= 0) table->removeRow(table->currentRow());
        });
        // Monter / descendre : lecture de la ligne, suppression, réinsertion (les éditeurs de cellule suivent)
        auto move = [this, table, inputs](int delta) {
            const int row = table->currentRow();
            const int target = row + delta;
            if (row < 0 || target < 0 || target >= table->rowCount()) return;
            std::vector<PinSpec> pins;
            QString error;
            if (!readPins(table, inputs, pins, error))
            {
                QMessageBox::warning(this, windowTitle(), error);
                return;
            }
            std::swap(pins[static_cast<std::size_t>(row)], pins[static_cast<std::size_t>(target)]);
            table->setRowCount(0);
            for (const auto& p : pins) addRow(table, p, inputs);
            table->setCurrentCell(target, kName);
        };
        connect(up, &QPushButton::clicked, this, [move] { move(-1); });
        connect(down, &QPushButton::clicked, this, [move] { move(1); });
        tabs->addTab(w, inputs ? tr("Entrées (%1)").arg(pins.size()) : tr("Sorties (%1)").arg(pins.size()));
        return table;
    };
    m_inputs = page(d.inputs, true);
    m_outputs = page(d.outputs, false);
    root->addWidget(tabs, 1);
    auto* note = new QLabel(tr("Nom de broche : lettres non accentuées, chiffres et « _ », commençant par une lettre (identifiant stable des "
                               "liens) ; le libellé est libre. Les liens des instances vers une broche supprimée ou dont le type change "
                               "sont retirés au retour au graphe parent (et signalés)."),
                            this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: palette(mid);"));
    root->addWidget(note);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &BlueprintSubgraphDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
    resize(760, 560);
}

QTableWidget* BlueprintSubgraphDialog::makeTable(const std::vector<PinSpec>& pins, bool inputs)
{
    auto* table = new QTableWidget(0, inputs ? 6 : 4, this);
    QStringList headers { tr("Nom"), tr("Libellé"), tr("Type"), tr("Objet") };
    if (inputs) headers << tr("Valeur par défaut") << tr("Obligatoire");
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    for (const auto& p : pins) addRow(table, p, inputs);
    table->resizeColumnsToContents();
    return table;
}

void BlueprintSubgraphDialog::addRow(QTableWidget* table, const PinSpec& p, bool inputs)
{
    const int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, kName, new QTableWidgetItem(QString::fromStdString(p.name)));
    table->setItem(row, kLabel, new QTableWidgetItem(QString::fromStdString(p.label)));
    auto* type = new QComboBox(table);
    for (const auto& t : kTypes) type->addItem(tr(t.label), QString::fromLatin1(t.id));
    type->setCurrentIndex(std::max(0, type->findData(typeIdOf(p))));
    table->setCellWidget(row, kType, type);
    auto* object = new QLineEdit(QString::fromStdString(p.objectType), table);
    object->setPlaceholderText(tr("tout objet"));
    auto* completer = new QCompleter(m_objectTypes, object);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    object->setCompleter(completer);
    table->setCellWidget(row, kObject, object);
    auto sync = [type, object] { object->setEnabled(type->currentData().toString() == QLatin1String("object")); };
    connect(type, &QComboBox::currentIndexChanged, object, sync);
    sync();
    if (inputs)
    {
        table->setItem(row, kDefault, new QTableWidgetItem(defaultText(p)));
        auto* required = new QTableWidgetItem();
        required->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        required->setCheckState(p.required ? Qt::Checked : Qt::Unchecked);
        table->setItem(row, kRequired, required);
    }
}

QString BlueprintSubgraphDialog::defaultText(const PinSpec& p)
{
    if (std::holds_alternative<std::monostate>(p.defaultValue)) return {};
    if (const auto* r = std::get_if<double>(&p.defaultValue)) return QString::number(*r, 'g', 15);
    return QString::fromStdString(toText(p.defaultValue));
}

bool BlueprintSubgraphDialog::parseDefault(const QString& raw, const PinSpec& pin, Value& out)
{
    out = Value {};
    const QString text = raw.trimmed();
    if (text.isEmpty()) return true;
    if (pin.any || pin.type == ValueType::Object) return false;   // un objet est toujours relié, jamais saisi
    switch (pin.type)
    {
    case ValueType::Real:
    {
        bool ok = false;
        const double v = parseReal(text, ok);
        if (ok) out = v;
        return ok;
    }
    case ValueType::Integer:
    {
        bool ok = false;
        const long long v = text.toLongLong(&ok);
        if (ok) out = v;
        return ok;
    }
    case ValueType::Bool:
    {
        const QString t = text.toLower();
        if (t == QLatin1String("vrai") || t == QLatin1String("true") || t == QLatin1String("oui") || t == QLatin1String("1")) out = true;
        else if (t == QLatin1String("faux") || t == QLatin1String("false") || t == QLatin1String("non") || t == QLatin1String("0")) out = false;
        else return false;
        return true;
    }
    case ValueType::Text: out = raw.toStdString(); return true;
    case ValueType::Point3:
    {
        // « x; y; z » (virgule décimale admise) ou « x, y, z » (point décimal)
        const QStringList parts = text.contains(QLatin1Char(';')) ? text.split(QLatin1Char(';')) : text.split(QLatin1Char(','));
        if (parts.size() != 3) return false;
        Point3 p {};
        for (int k = 0; k < 3; ++k)
        {
            bool ok = false;
            p[static_cast<std::size_t>(k)] = parseReal(parts[k], ok);
            if (!ok) return false;
        }
        out = p;
        return true;
    }
    case ValueType::IdList:
    {
        std::vector<int> ids;
        for (const QString& part : text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts))
        {
            bool ok = false;
            ids.push_back(part.toInt(&ok));
            if (!ok) return false;
        }
        out = ids;
        return true;
    }
    case ValueType::Object: return false;
    }
    return false;
}

bool BlueprintSubgraphDialog::readPins(QTableWidget* table, bool inputs, std::vector<PinSpec>& out, QString& error) const
{
    out.clear();
    std::set<std::string> names;
    const auto& previous = inputs ? m_original.inputs : m_original.outputs;
    for (int row = 0; row < table->rowCount(); ++row)
    {
        PinSpec p;
        p.kind = PinKind::Data;
        p.name = table->item(row, kName) ? table->item(row, kName)->text().trimmed().toStdString() : std::string();
        p.label = table->item(row, kLabel) ? table->item(row, kLabel)->text().trimmed().toStdString() : std::string();
        if (p.label.empty()) p.label = p.name;
        const QString side = inputs ? tr("entrée") : tr("sortie");
        if (!isValidPinName(p.name))
        {
            error = tr("%1 %2 : nom « %3 » invalide (lettres non accentuées, chiffres et « _ », commençant par une lettre ; ni « exec » ni « then »)")
                        .arg(side).arg(row + 1).arg(QString::fromStdString(p.name));
            return false;
        }
        if (!names.insert(p.name).second)
        {
            error = tr("%1 « %2 » en double").arg(side, QString::fromStdString(p.name));
            return false;
        }
        const auto* type = qobject_cast<QComboBox*>(table->cellWidget(row, kType));
        const std::string typeId = type ? type->currentData().toString().toStdString() : std::string("real");
        if (typeId == "any") p.any = true;
        else valueTypeFromId(typeId, p.type);
        if (const auto* object = qobject_cast<QLineEdit*>(table->cellWidget(row, kObject)); object && p.type == ValueType::Object && !p.any)
            p.objectType = object->text().trimmed().toStdString();
        // Grandeur (unité affichée) conservée pour une broche existante de même type
        for (const auto& old : previous)
            if (old.name == p.name && old.type == p.type && old.any == p.any) p.quantity = old.quantity;
        if (inputs)
        {
            const QString text = table->item(row, kDefault) ? table->item(row, kDefault)->text() : QString();
            if (!parseDefault(text, p, p.defaultValue))
            {
                error = tr("entrée « %1 » : valeur par défaut « %2 » illisible pour le type %3%4")
                            .arg(QString::fromStdString(p.name), text, QString::fromStdString(pinTypeName(p)),
                                 p.any || p.type == ValueType::Object ? tr(" (un objet se relie, il ne se saisit pas)") : QString());
                return false;
            }
            p.required = table->item(row, kRequired) && table->item(row, kRequired)->checkState() == Qt::Checked;
        }
        out.push_back(p);
    }
    return true;
}

void BlueprintSubgraphDialog::accept()
{
    SubgraphDefinition d = m_original;
    d.name = m_name->text().trimmed().toStdString();
    if (d.name.empty())
    {
        QMessageBox::warning(this, windowTitle(), tr("Donner un nom au sous-graphe."));
        return;
    }
    d.category = m_category->text().trimmed().toStdString();
    if (d.category.empty()) d.category = "Sous-graphes";
    d.description = m_description->text().trimmed().toStdString();
    d.documentation = m_documentation->toPlainText().trimmed().toStdString();
    QString error;
    if (!readPins(m_inputs, true, d.inputs, error) || !readPins(m_outputs, false, d.outputs, error))
    {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    m_result = d;
    QDialog::accept();
}

} // namespace TSA::UI
