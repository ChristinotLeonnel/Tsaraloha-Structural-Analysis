// Convertisseur du module d'exemple « sample.csvimport » (docs/SDK.md) : lit un fichier CSV décrivant des
// nœuds et des barres, écrit un fichier TSA3D. Exécuté par l'hôte (TSA, TSALab) dans un PROCESSUS SÉPARÉ :
// aucune dépendance (C++17 standard), aucune contrainte d'ABI, code de sortie 0 = succès.
//
//   sample_csv2tsa3d <entrée.csv> <sortie.tsa3d>
//
// Format CSV (séparateur « , » ou « ; », lignes « # » ignorées, unités : m, kN, Pa) :
//   material,<id>,<type>,<E>,<nu>,<densité>,<fk>
//   section,<id>,rectangular,<b>,<h>          ou  section,<id>,circular,<d>
//   node,<id>,<x>,<y>,<z>[,fixed|pinned|free]
//   member,<id>,<beam|column|truss>,<nœud 1>,<nœud 2>,<section>,<matériau>
//   case,<id>,<dead|live|wind|snow|...>[,selfweight]
//   nodal,<id>,<nœud>,<cas>,<Fx>,<Fy>,<Fz>
//   uniform,<id>,<barre>,<cas>,<q kN/m>            (direction gravité)
//
// Les erreurs sont signalées avec leur numéro de ligne (sortie d'erreur) et rien n'est écrit.

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{

std::string trim(const std::string& s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> split(const std::string& line)
{
    const char sep = line.find(';') != std::string::npos ? ';' : ',';
    std::vector<std::string> out;
    std::string cell;
    std::istringstream in(line);
    while (std::getline(in, cell, sep)) out.push_back(trim(cell));
    return out;
}

/// Chaîne JSON échappée.
std::string q(const std::string& s)
{
    std::string out = "\"";
    for (char c : s)
    {
        if (c == '"' || c == '\\') out += '\\';
        if (static_cast<unsigned char>(c) < 0x20) continue;
        out += c;
    }
    return out + "\"";
}

bool number(const std::string& s, double* v)
{
    try
    {
        size_t used = 0;
        *v = std::stod(s, &used);
        return used == s.size();
    }
    catch (...)
    {
        return false;
    }
}

std::string join(const std::vector<std::string>& items, const char* indent)
{
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) out += indent + items[i] + (i + 1 < items.size() ? ",\n" : "\n");
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "Usage : sample_csv2tsa3d <entrée.csv> <sortie.tsa3d>\n");
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in)
    {
        std::fprintf(stderr, "Lecture impossible : %s\n", argv[1]);
        return 1;
    }

    std::vector<std::string> materials, sections, nodes, members, cases, nodal, memberLoads, errors;
    std::set<std::string> ids;
    std::string line;
    int lineNo = 0;
    auto error = [&](const std::string& msg) { errors.push_back("ligne " + std::to_string(lineNo) + " : " + msg); };
    auto newId = [&](const std::string& id) {
        if (id.empty()) return error("identifiant vide"), false;
        if (!ids.insert(id).second) return error("identifiant « " + id + " » en double"), false;
        return true;
    };
    auto nums = [&](const std::vector<std::string>& c, size_t from, size_t count, std::vector<double>* out) {
        for (size_t i = from; i < from + count; ++i)
        {
            double v = 0.0;
            if (i >= c.size() || !number(c[i], &v)) return error("nombre attendu en colonne " + std::to_string(i + 1)), false;
            out->push_back(v);
        }
        return true;
    };
    auto fmt = [](double v) {
        std::ostringstream o;
        o.precision(12);
        o << v;
        return o.str();
    };

    while (std::getline(in, line))
    {
        ++lineNo;
        if (lineNo == 1 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto c = split(line);
        const std::string& kind = c[0];
        std::vector<double> v;
        if (kind == "material" && c.size() >= 7 && newId(c[1]) && nums(c, 3, 4, &v))
            materials.push_back("{ \"id\": " + q(c[1]) + ", \"type\": " + q(c[2]) + ", \"E\": " + fmt(v[0]) + ", \"nu\": " + fmt(v[1])
                                + ", \"density\": " + fmt(v[2]) + ", \"fk\": " + fmt(v[3]) + " }");
        else if (kind == "section" && c.size() >= 4 && c[2] == "rectangular" && newId(c[1]) && nums(c, 3, 2, &v))
            sections.push_back("{ \"id\": " + q(c[1]) + ", \"shape\": \"rectangular\", \"dimensions\": { \"b\": " + fmt(v[0]) + ", \"h\": " + fmt(v[1]) + " } }");
        else if (kind == "section" && c.size() >= 4 && c[2] == "circular" && newId(c[1]) && nums(c, 3, 1, &v))
            sections.push_back("{ \"id\": " + q(c[1]) + ", \"shape\": \"circular\", \"dimensions\": { \"d\": " + fmt(v[0]) + " } }");
        else if (kind == "node" && c.size() >= 5 && newId(c[1]) && nums(c, 2, 3, &v))
        {
            std::string n = "{ \"id\": " + q(c[1]) + ", \"position\": [" + fmt(v[0]) + ", " + fmt(v[1]) + ", " + fmt(v[2]) + "]";
            const std::string support = c.size() > 5 ? c[5] : "free";
            if (support == "fixed")
                n += ", \"support\": { \"tx\": \"fixed\", \"ty\": \"fixed\", \"tz\": \"fixed\", \"rx\": \"fixed\", \"ry\": \"fixed\", \"rz\": \"fixed\" }";
            else if (support == "pinned")
                n += ", \"support\": { \"tx\": \"fixed\", \"ty\": \"fixed\", \"tz\": \"fixed\" }";
            else if (support != "free" && !support.empty())
                error("appui « " + support + " » inconnu (fixed, pinned, free)");
            nodes.push_back(n + " }");
        }
        else if (kind == "member" && c.size() >= 7 && newId(c[1]))
            members.push_back("{ \"id\": " + q(c[1]) + ", \"type\": " + q(c[2]) + ", \"nodes\": [" + q(c[3]) + ", " + q(c[4]) + "], \"section\": " + q(c[5])
                              + ", \"material\": " + q(c[6]) + " }");
        else if (kind == "case" && c.size() >= 3 && newId(c[1]))
            cases.push_back("{ \"id\": " + q(c[1]) + ", \"category\": " + q(c[2]) + (c.size() > 3 && c[3] == "selfweight" ? ", \"selfWeight\": true" : "") + " }");
        else if (kind == "nodal" && c.size() >= 7 && newId(c[1]) && nums(c, 4, 3, &v))
            nodal.push_back("{ \"id\": " + q(c[1]) + ", \"node\": " + q(c[2]) + ", \"case\": " + q(c[3]) + ", \"force\": [" + fmt(v[0]) + ", " + fmt(v[1]) + ", "
                            + fmt(v[2]) + "] }");
        else if (kind == "uniform" && c.size() >= 5 && newId(c[1]) && nums(c, 4, 1, &v))
            memberLoads.push_back("{ \"id\": " + q(c[1]) + ", \"member\": " + q(c[2]) + ", \"case\": " + q(c[3])
                                  + ", \"kind\": \"uniform\", \"direction\": \"gravity\", \"q1\": " + fmt(v[0]) + " }");
        else if (errors.empty() || errors.back().rfind("ligne " + std::to_string(lineNo) + " ", 0) != 0)
            error("ligne non reconnue : « " + kind + " » (ou colonnes manquantes)");
    }
    // Les références (nœuds, sections, matériaux, cas) sont contrôlées par l'hôte à l'import TSA3D.
    if (nodes.empty()) errors.push_back("aucun nœud");
    if (!errors.empty())
    {
        for (const auto& e : errors) std::fprintf(stderr, "%s\n", e.c_str());
        return 1;
    }

    std::ostringstream j;
    j << "{\n  \"format\": \"TSA3D\",\n  \"version\": \"1.0\",\n";
    j << "  \"metadata\": { \"name\": \"Import CSV\",\"producer\": { \"name\": \"sample.csvimport\", \"version\": \"1.0.0\" } },\n";
    j << "  \"units\": { \"length\": \"m\", \"force\": \"kN\", \"stress\": \"Pa\" },\n";
    j << "  \"materials\": [\n" << join(materials, "    ") << "  ],\n";
    j << "  \"sections\": [\n" << join(sections, "    ") << "  ],\n";
    j << "  \"nodes\": [\n" << join(nodes, "    ") << "  ],\n";
    j << "  \"members\": [\n" << join(members, "    ") << "  ],\n";
    j << "  \"loads\": {\n    \"cases\": [\n" << join(cases, "      ") << "    ],\n";
    j << "    \"nodal\": [\n" << join(nodal, "      ") << "    ],\n";
    j << "    \"member\": [\n" << join(memberLoads, "      ") << "    ]\n  }\n}\n";

    std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
    const std::string text = j.str();
    if (!out || !out.write(text.data(), static_cast<std::streamsize>(text.size())))
    {
        std::fprintf(stderr, "Écriture impossible : %s\n", argv[2]);
        return 1;
    }
    std::printf("%s : %zu nœud(s), %zu barre(s)\n", argv[2], nodes.size(), members.size());
    return 0;
}
