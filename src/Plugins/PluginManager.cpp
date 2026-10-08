#include "PluginManager.h"

#include "../Blueprint/BlueprintRuntime.h"
#include "../Project/ProjectSession.h"

#include <ProductIdentity.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>

#include <algorithm>

namespace TSA::Plugins
{

namespace
{
using TSA::Automation::Arguments;
using TSA::Automation::CommandResult;

class CommandContext final : public ICommandContext
{
public:
    explicit CommandContext(TSA::Project::ProjectSession& session) : m_session(session) {}
    CommandResult execute(const std::string& id, const Arguments& args) override
    {
        return TSA::Automation::CommandRegistry::builtIn().execute(m_session, id, args);
    }

private:
    TSA::Project::ProjectSession& m_session;
};

class NodeContext final : public INodeContext
{
public:
    explicit NodeContext(TSA::Blueprint::ExecutionContext& ctx) : m_ctx(ctx)
    {
        if (m_ctx.session()) m_commands = std::make_unique<CommandContext>(*m_ctx.session());
    }
    TSA::Blueprint::Value input(const std::string& pin) override { return m_ctx.input(pin); }
    void setOutput(const std::string& pin, const TSA::Blueprint::Value& value) override { m_ctx.setOutput(pin, value); }
    bool fail(const std::string& message) override { return m_ctx.fail(message); }
    void log(const std::string& line) override { m_ctx.log(line); }
    ICommandContext* commands() override { return m_commands.get(); }

private:
    TSA::Blueprint::ExecutionContext& m_ctx;
    std::unique_ptr<CommandContext> m_commands;
};

class Host final : public IPluginHost
{
public:
    explicit Host(LoadedPlugin& record) : m_record(record) {}

    bool addCommand(const TSA::Automation::CommandSpec& spec, CommandHandler handler) override
    {
        if (!handler) return false;
        TSA::Automation::CommandSpec s = spec;
        // Une commande de plugin compose les commandes existantes, chacune dans sa propre transaction :
        // pas de transaction englobante (pas d'imbrication).
        s.modifiesModel = false;
        auto& registry = TSA::Automation::CommandRegistry::global();
        const bool ok = registry.add(s, [handler](TSA::Project::ProjectSession& session, const Arguments& args) {
            CommandContext ctx(session);
            return handler(ctx, args);
        });
        if (!ok) return false;
        TSA::Blueprint::registerCommandNode(TSA::Blueprint::NodeLibrary::global(), registry, s.id);
        m_record.commands.push_back(s.id);
        return true;
    }

    bool addNode(const TSA::Blueprint::NodeDefinition& definition, NodeHandler handler) override
    {
        using namespace TSA::Blueprint;
        if (!handler || definition.id.empty()) return false;
        NodeDefinition d = definition;
        if (!d.pure)
        {
            if (!d.input("exec")) d.inputs.insert(d.inputs.begin(), PinSpec { "exec", "", PinKind::Exec });
            if (!d.output("then")) d.outputs.insert(d.outputs.begin(), PinSpec { "then", "", PinKind::Exec });
        }
        const bool pure = d.pure;
        const bool ok = NodeLibrary::global().add(std::move(d), [handler, pure](ExecutionContext& c) {
            NodeContext ctx(c);
            if (!handler(ctx)) return c.failed() ? false : c.fail("échec du nœud de plugin");
            if (c.failed()) return false;
            return pure ? true : c.fire("then");
        });
        if (ok) m_record.nodes.push_back(definition.id);
        return ok;
    }

    void log(const std::string& line) override { m_record.log << QString::fromStdString(line); }
    std::string applicationName() const override { return TSA::Product::kName; }

private:
    LoadedPlugin& m_record;
};
} // namespace

PluginManager& PluginManager::instance()
{
    static PluginManager manager;
    return manager;
}

QString PluginManager::defaultDirectory()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
}

int PluginManager::loadDirectory(const QString& directory)
{
    int count = 0;
    const QDir dir(directory);
    if (!dir.exists()) return 0;
#ifdef _WIN32
    const QStringList filters { QStringLiteral("*.dll") };
#else
    const QStringList filters { QStringLiteral("*.so"), QStringLiteral("*.dylib") };
#endif
    for (const QFileInfo& f : dir.entryInfoList(filters, QDir::Files, QDir::Name))
        if (loadFile(f.absoluteFilePath())) ++count;
    return count;
}

bool PluginManager::loadFile(const QString& path, QString* errorOut)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    const auto existing = std::find_if(m_plugins.begin(), m_plugins.end(), [&](const LoadedPlugin& p) { return p.path == canonical; });
    if (existing != m_plugins.end())
    {
        if (errorOut) *errorOut = existing->error;
        return existing->loaded;
    }
    m_plugins.push_back({});
    LoadedPlugin& record = m_plugins.back();
    record.path = canonical.isEmpty() ? path : canonical;
    auto fail = [&](const QString& e) {
        record.error = e;
        if (errorOut) *errorOut = e;
        return false;
    };

    auto* library = new QLibrary(record.path);   // jamais déchargée
    if (!library->load()) return fail(QStringLiteral("chargement impossible : %1").arg(library->errorString()));
    using VersionFn = int (*)();
    using CreateFn = IPlugin* (*)();
    const auto version = reinterpret_cast<VersionFn>(library->resolve("tsaraloha_plugin_api_version"));
    const auto create = reinterpret_cast<CreateFn>(library->resolve("tsaraloha_create_plugin"));
    if (!version || !create) return fail(QStringLiteral("points d'entrée tsaraloha_plugin_api_version / tsaraloha_create_plugin absents"));
    if (version() != kApiVersion)
        return fail(QStringLiteral("API de plugin %1, l'application attend la version %2").arg(version()).arg(kApiVersion));
    std::unique_ptr<IPlugin> plugin(create());
    if (!plugin) return fail(QStringLiteral("le plugin n'a pas été créé"));
    record.info = plugin->info();
    Host host(record);
    std::string error;
    if (!plugin->initialize(host, &error))
        return fail(QStringLiteral("initialisation refusée : %1").arg(QString::fromStdString(error)));
    record.loaded = true;
    m_instances.push_back(std::move(plugin));
    return true;
}

} // namespace TSA::Plugins
