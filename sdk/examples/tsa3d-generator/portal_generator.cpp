// Exemple de programme EXTERNE produisant un modèle TSA3D, sans Qt ni bibliothèque TSA (C++17 standard).
// Génère un portique plan à N travées (poteaux encastrés, traverses, charge répartie, combinaison ELU).
//
//   portal_generator <travées> <portée m> <hauteur m> <fichier.tsa3d>
//   puis : tsa3d-validate <fichier.tsa3d>  et  TSA > Fichier > Importer TSA3D
//
// Points à respecter (docs/TSA3D.md) : identifiants uniques dans tout le document ; unités déclarées ;
// références par identifiant ; données propres au programme dans « extensions » (préfixe de domaine).

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
    const int bays = argc > 1 ? std::atoi(argv[1]) : 3;
    const double span = argc > 2 ? std::atof(argv[2]) : 6.0;
    const double height = argc > 3 ? std::atof(argv[3]) : 4.0;
    const char* out = argc > 4 ? argv[4] : "portique.tsa3d";
    if (bays < 1 || span <= 0.0 || height <= 0.0)
    {
        std::fprintf(stderr, "Usage : portal_generator <travées ≥ 1> <portée > 0> <hauteur > 0> <fichier>\n");
        return 2;
    }

    std::ostringstream j;
    j << "{\n  \"format\": \"TSA3D\",\n  \"version\": \"1.0\",\n";
    j << "  \"metadata\": { \"name\": \"Portique " << bays << " travée(s)\", \"producer\": { \"name\": \"portal_generator\", \"version\": \"1.0\" } },\n";
    j << "  \"units\": { \"length\": \"m\", \"force\": \"kN\", \"stress\": \"Pa\" },\n";
    j << "  \"materials\": [ { \"id\": \"S355\", \"type\": \"steel\", \"E\": 210e9, \"nu\": 0.3, \"density\": 7850, \"fk\": 355e6 } ],\n";
    j << "  \"sections\": [\n"
         "    { \"id\": \"HEB240\", \"shape\": \"i\", \"dimensions\": { \"b\": 0.24, \"h\": 0.24, \"tw\": 0.01, \"tf\": 0.017 } },\n"
         "    { \"id\": \"IPE360\", \"shape\": \"i\", \"dimensions\": { \"b\": 0.17, \"h\": 0.36, \"tw\": 0.008, \"tf\": 0.0127 } }\n  ],\n";
    j << "  \"nodes\": [\n";
    const char* fixed = ", \"support\": { \"tx\": \"fixed\", \"ty\": \"fixed\", \"tz\": \"fixed\", \"rx\": \"fixed\", \"ry\": \"fixed\", \"rz\": \"fixed\" }";
    for (int i = 0; i <= bays; ++i)
    {
        j << "    { \"id\": \"P" << i << "\", \"position\": [" << i * span << ", 0, 0]" << fixed << " },\n";
        j << "    { \"id\": \"H" << i << "\", \"position\": [" << i * span << ", 0, " << height << "] }" << (i < bays ? "," : "") << "\n";
    }
    j << "  ],\n  \"members\": [\n";
    for (int i = 0; i <= bays; ++i)
        j << "    { \"id\": \"col" << i << "\", \"type\": \"column\", \"nodes\": [\"P" << i << "\", \"H" << i << "\"], \"section\": \"HEB240\", \"material\": \"S355\" },\n";
    for (int i = 0; i < bays; ++i)
        j << "    { \"id\": \"beam" << i << "\", \"type\": \"beam\", \"nodes\": [\"H" << i << "\", \"H" << i + 1
          << "\"], \"section\": \"IPE360\", \"material\": \"S355\", \"extensions\": { \"com.exemple.generator\": { \"bay\": " << i + 1 << " } } }"
          << (i + 1 < bays ? "," : "") << "\n";
    j << "  ],\n  \"loads\": {\n    \"cases\": [ { \"id\": \"G\", \"category\": \"dead\", \"selfWeight\": true }, { \"id\": \"Q\", \"category\": \"live\" } ],\n";
    j << "    \"member\": [\n";
    for (int i = 0; i < bays; ++i)
        j << "      { \"id\": \"q" << i << "\", \"member\": \"beam" << i << "\", \"case\": \"Q\", \"kind\": \"uniform\", \"direction\": \"gravity\", \"q1\": 12 }"
          << (i + 1 < bays ? "," : "") << "\n";
    j << "    ],\n    \"combinations\": [ { \"id\": \"ELU\", \"type\": \"ulsFundamental\", \"factors\": [ { \"case\": \"G\", \"factor\": 1.35 }, { \"case\": \"Q\", \"factor\": 1.5 } ] } ]\n";
    j << "  }\n}\n";

    std::FILE* f = std::fopen(out, "wb");
    if (!f)
    {
        std::fprintf(stderr, "Écriture impossible : %s\n", out);
        return 1;
    }
    const std::string text = j.str();
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    std::printf("%s écrit (%d travée(s))\n", out, bays);
    return 0;
}
