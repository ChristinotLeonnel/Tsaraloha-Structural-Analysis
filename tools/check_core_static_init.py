#!/usr/bin/env python3
"""Contrôle : aucune initialisation statique dynamique dans le noyau <P>_Core (TSA Core, étape 1).

<P>_Core est une bibliothèque STATIQUE : l'éditeur de liens n'en garde que les objets référencés, si bien qu'un
enregistrement par initialisation statique (objet global à constructeur, Q_COREAPP_STARTUP_FUNCTION,
__attribute__((constructor)), init_priority...) y serait silencieusement perdu. Les enregistrements se font dans les
couches supérieures (bibliothèques OBJECT), comme le fournisseur des catalogues installés
(ExtensionSystem/LibraryRegistry.cpp).

Détection par SECTIONS (objdump -h), indépendante des noms de symboles : un objet est en infraction s'il contient une
section non vide lue par le démarrage ou l'arrêt du programme (script par défaut de GNU ld) :
  .init_array[.*]  .preinit_array[.*]  .ctors[.*]  .init     (initialisation)
  .fini_array[.*]  .dtors[.*]          .fini                 (terminaison)
Les variantes suffixées (.init_array.00200 : init_priority / constructor(200)) sont incluses.

Ce qui n'est PAS signalé (initialisation constante, aucune section ci-dessus) : constexpr, constinit, agrégats
constants, statiques locales de fonction (initialisées au premier appel), Q_LOGGING_CATEGORY.
Ce qui EST signalé, à juste titre : const std::string / QString / std::vector globaux (constructeur ou destructeur
non trivial = enregistrement dynamique), variables inline à initialisation dynamique.

Instrumentation : -fsanitize=* ou --coverage ajoutent un constructeur de module (.init_array) à CHAQUE objet ; le
contrôle les signalerait tous. Il vise les compilations ordinaires (Debug / Release) de <P>_Core.

Contrôle impossible (code 2, jamais confondu avec un constat) : outil absent ou en échec, bibliothèque vide ou
illisible, membre au format non reconnu (par ex. bitcode LLVM), objet LTO (sections .gnu.lto_* : le code, et donc
les sections d'initialisation, n'est produit qu'à l'édition de liens finale ; en LTO « slim », aucune .init_array
n'existe dans l'archive et nm, via le greffon LTO, ne montre pas _GLOBAL__sub_I_*).

Usage :
  check_core_static_init.py --objdump <objdump> <libP_Core.a> [...]
  check_core_static_init.py --objdump <objdump> --self-test <compilateur C++> [--ar <ar>]
Codes de sortie : 0 conforme ; 1 initialisation statique trouvée (ou contre-épreuve en échec) ; 2 contrôle impossible.
MSVC : non pris en charge (objets COFF : sections .CRT$XC*, symboles ??__E ; voir cmake/TSAProduct.cmake).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

OK, CONSTAT, IMPOSSIBLE = 0, 1, 2

# Noms exacts lus par le script par défaut de GNU ld (KEEP(*(.init_array.* ...)), .preinit_array, .ctors...).
INIT_SECTION_RE = re.compile(r"^\.(?:(?:init_array|fini_array|preinit_array|ctors|dtors)(?:\..+)?|init|fini)$")
LTO_SECTION_PREFIX = ".gnu.lto_"
MEMBER_RE = re.compile(r"^(?P<member>.+):\s+file format (?P<format>\S+)\s*$")
SECTION_RE = re.compile(r"^\s*\d+\s+(?P<name>\S+)\s+(?P<size>[0-9a-fA-F]+)\s")


class ControleImpossible(Exception):
    pass


def run_objdump(objdump, path):
    """objdump -h en locale C (les en-têtes « In archive » / « file format » sont traduits sinon)."""
    env = dict(os.environ, LC_ALL="C", LANG="C")
    env.pop("LANGUAGE", None)
    try:
        result = subprocess.run([objdump, "-h", path], capture_output=True, text=True, env=env, errors="replace")
    except OSError as exc:  # FileNotFoundError, PermissionError...
        raise ControleImpossible(f"outil objdump inutilisable « {objdump} » : {exc.strerror or exc}")
    if result.returncode != 0:
        detail = result.stderr.strip() or "aucun message"
        raise ControleImpossible(f"objdump a échoué (code {result.returncode}) sur {path} : {detail}")
    return result.stdout


def analyse(objdump, path):
    """Retourne [(membre, format, {section: taille})] ; lève ControleImpossible."""
    if not os.path.isfile(path):
        raise ControleImpossible(f"bibliothèque introuvable : {path}")
    members = []
    for line in run_objdump(objdump, path).splitlines():
        m = MEMBER_RE.match(line)
        if m:  # chaque membre est compté par position : les doublons de nom restent distincts
            members.append((m.group("member"), m.group("format"), {}))
            continue
        s = SECTION_RE.match(line)
        if s and members:
            members[-1][2][s.group("name")] = int(s.group("size"), 16)
    if not members:
        raise ControleImpossible(f"aucun objet lu dans {path} (bibliothèque vide ?)")
    return members


def verdict(objdump, path):
    """(code, lignes de message) pour une bibliothèque."""
    try:
        members = analyse(objdump, path)
    except ControleImpossible as exc:
        return IMPOSSIBLE, [f"CONTRÔLE IMPOSSIBLE : {exc}"]
    lto = [name for name, _, sections in members if any(n.startswith(LTO_SECTION_PREFIX) for n in sections)]
    if lto:
        return IMPOSSIBLE, [f"CONTRÔLE IMPOSSIBLE : objet(s) LTO dans {path} ({', '.join(lto[:5])}"
                            f"{'...' if len(lto) > 5 else ''}) ; compiler <P>_Core sans -flto / "
                            f"INTERPROCEDURAL_OPTIMIZATION pour ce contrôle"]
    lines, offenders = [], 0
    for index, (name, _, sections) in enumerate(members, 1):
        bad = sorted(f"{n} ({size} o)" for n, size in sections.items() if size > 0 and INIT_SECTION_RE.match(n))
        if bad:
            offenders += 1
            lines.append(f"INITIALISATION STATIQUE interdite dans le noyau : {name} [membre {index}] : {', '.join(bad)}")
    lines.append(f"check_core_static_init : {path} : {len(members)} objet(s), {offenders} avec initialisation "
                 f"ou terminaison statique")
    return (CONSTAT if offenders else OK), lines


# ---------------------------------------------------------------------------------------------------------------
# Contre-épreuve : fixtures compilées avec le compilateur du projet, chacune doit donner le verdict attendu.
# Les corps ont des effets de bord réels (puts) pour que l'optimiseur ne supprime pas l'initialisation.
PROLOGUE = "#include <cstdio>\nstruct Enregistreur { Enregistreur() { std::puts(\"enregistre\"); } };\n"
FIXTURES = {
    # nom : (attendu, source)
    "objet_global": (CONSTAT, PROLOGUE + "Enregistreur g_enregistreur;\n"),
    "attribut_constructor": (CONSTAT, "#include <cstdio>\n"
                             "__attribute__((constructor)) static void f() { std::puts(\"c\"); }\n"),
    "attribut_constructor_200": (CONSTAT, "#include <cstdio>\n"
                                 "__attribute__((constructor(200))) static void f() { std::puts(\"c\"); }\n"),
    "init_priority": (CONSTAT, PROLOGUE + "Enregistreur g_prio __attribute__((init_priority(200)));\n"),
    "variable_inline": (CONSTAT, PROLOGUE + "inline Enregistreur g_inline;\n"
                        "int utilise() { return (int)sizeof(&g_inline); }\n"),
    "const_string_global": (CONSTAT, "#include <string>\nconst std::string kNom = "
                            "\"une-chaine-assez-longue-pour-allouer-sur-le-tas\";\n"
                            "const std::string& nom() { return kNom; }\n"),
    "static_vector_global": (CONSTAT, "#include <vector>\nstatic std::vector<int> s_v{1, 2, 3};\n"
                             "int somme() { int s = 0; for (int v : s_v) s += v; return s; }\n"),
    "destructeur_seul": (CONSTAT, "#include <cstdio>\nstruct Fin { ~Fin() { std::puts(\"fin\"); } };\nFin g_fin;\n"),
    "attribut_destructor": (CONSTAT, "#include <cstdio>\n"
                            "__attribute__((destructor)) static void f() { std::puts(\"d\"); }\n"),
    "section_ctors": (CONSTAT, "#include <cstdio>\nstatic void f() { std::puts(\"c\"); }\n"
                      "__attribute__((section(\".ctors\"), used)) static void (*p)() = f;\n"),
    "section_preinit_array": (CONSTAT, "#include <cstdio>\nstatic void f() { std::puts(\"p\"); }\n"
                              "__attribute__((section(\".preinit_array\"), used)) static void (*p)() = f;\n"),
    "constexpr": (OK, "#include <array>\n#include <string_view>\nconstexpr int kN = 3;\n"
                  "constexpr std::array<double, 3> kC{1.0, 2.0, 3.0};\nconstexpr std::string_view kNom = \"noyau\";\n"
                  "double c(int i) { return kC[i % kN] + kNom.size(); }\n"),
    "statique_locale": (OK, "#include <string>\n#include <cstdio>\nconst std::string& nom() {\n"
                        "  static const std::string s = \"une-chaine-assez-longue-pour-allouer-sur-le-tas\";\n"
                        "  std::puts(\"x\"); return s; }\n"),
    "constinit": (OK, "#include <atomic>\nstruct C { constexpr C(int v) : n(v) {} int n; };\n"
                  "constinit C g_c{4};\nconstinit std::atomic<int> g_a{0};\n"
                  "int lire() { return g_c.n + g_a.load(); }\n"),
    "agregat_constant": (OK, "struct P { int a; double b; const char* nom; };\n"
                         "const P kP[] = {{1, 2.0, \"a\"}, {3, 4.0, \"b\"}};\nconst char* const kN[] = {\"x\", \"y\"};\n"
                         "double p(int i) { return kP[i & 1].b + kN[i & 1][0]; }\n"),
    "unite_vide": (OK, "\n"),
}
OPTIM = (["-O0"], ["-O2"])
# (nom, fixture, options, attendu, facultatif) : LTO refusé ; -ffat-lto-objects propre à GCC.
LTO_CASES = (("lto_slim", "objet_global", ["-O2", "-flto"], IMPOSSIBLE, False),
             ("lto_fat", "objet_global", ["-O2", "-flto", "-ffat-lto-objects"], IMPOSSIBLE, True))
NOMS = {OK: "conforme", CONSTAT: "constat", IMPOSSIBLE: "impossible"}


def self_test(objdump, cxx, ar):
    for tool, role in ((cxx, "compilateur C++"), (ar, "ar")):
        if not tool or not shutil.which(tool):
            print(f"CONTRÔLE IMPOSSIBLE : {role} introuvable : {tool}")
            return IMPOSSIBLE
    failures, skipped, total = 0, 0, 0
    with tempfile.TemporaryDirectory(prefix="core_static_init_") as tmp:

        def build(case, source_name, flags):
            src = os.path.join(tmp, f"{source_name}.cpp")
            with open(src, "w", encoding="utf-8") as f:
                f.write(FIXTURES[source_name][1])
            # Nom de membre volontairement hostile : espace et deux-points (découpage du format de sortie).
            obj = os.path.join(tmp, f"{case} : membre.o")
            lib = os.path.join(tmp, f"lib {case}.a")
            r = subprocess.run([cxx, "-std=c++20", *flags, "-c", src, "-o", obj], capture_output=True, text=True)
            if r.returncode != 0:
                return None, r.stderr.strip().splitlines()[-1:] or ["?"]
            subprocess.run([ar, "rcs", lib, obj], check=True, capture_output=True)
            return lib, None

        def check(case, lib, expected, extra=""):
            nonlocal failures, total
            total += 1
            code, lines = verdict(objdump, lib)
            good = code == expected
            failures += 0 if good else 1
            print(f"{'OK    ' if good else 'ÉCHEC '} {case:<34} attendu={NOMS[expected]:<10} obtenu={NOMS[code]}{extra}")
            if not good:
                for line in lines:
                    print(f"         {line}")

        for name, (expected, _) in FIXTURES.items():
            for flags in OPTIM:
                case = f"{name} {flags[0]}"
                lib, err = build(case.replace(" ", "_"), name, flags)
                if lib is None:
                    print(f"ÉCHEC  {case:<34} compilation impossible : {err[0]}")
                    failures += 1
                    total += 1
                    continue
                check(case, lib, expected)
        for case, source_name, flags, expected, optional in LTO_CASES:
            lib, err = build(case, source_name, flags)
            if lib is None:
                if optional:
                    skipped += 1
                    print(f"IGNORÉ {case:<34} option non prise en charge par {cxx} : {err[0]}")
                    continue
                print(f"ÉCHEC  {case:<34} compilation impossible : {err[0]}")
                failures += 1
                total += 1
                continue
            check(case, lib, expected)
        # Archive mixte : un seul membre fautif parmi des membres conformes, doublon de nom compris.
        mixte = os.path.join(tmp, "lib mixte.a")
        objs = []
        for i, name in enumerate(("constexpr", "objet_global", "constinit")):
            lib, _ = build(f"mixte{i}", name, ["-O2"])
            obj = os.path.join(tmp, f"mixte{i}", "dup.o")
            os.makedirs(os.path.dirname(obj), exist_ok=True)
            shutil.copy(os.path.join(tmp, f"mixte{i} : membre.o"), obj)
            objs.append(obj)
        subprocess.run([ar, "qc", mixte, *objs], check=True, capture_output=True)  # q : garde les 3 « dup.o »
        code, lines = verdict(objdump, mixte)
        exact = code == CONSTAT and sum("[membre 2]" in line for line in lines) == 1 and \
            sum(line.startswith("INITIALISATION") for line in lines) == 1
        total += 1
        failures += 0 if exact else 1
        print(f"{'OK    ' if exact else 'ÉCHEC '} {'archive_mixte (3 x dup.o)':<34} attendu=constat sur le membre 2 seul "
              f"obtenu={NOMS[code]}")
        # Membre non reconnu, archive vide, objdump absent : contrôle impossible, jamais « conforme ».
        texte = os.path.join(tmp, "pas_un_objet.o")
        with open(texte, "w", encoding="utf-8") as f:
            f.write("ceci n'est pas un objet\n")
        inconnu = os.path.join(tmp, "lib inconnu.a")
        subprocess.run([ar, "qc", inconnu, texte, objs[0]], check=True, capture_output=True)
        check("membre_non_reconnu", inconnu, IMPOSSIBLE)
        vide = os.path.join(tmp, "lib vide.a")
        with open(vide, "wb") as f:
            f.write(b"!<arch>\n")
        check("archive_vide", vide, IMPOSSIBLE)
        total += 1
        code, _ = verdict(os.path.join(tmp, "objdump-inexistant"), objs[0])
        failures += 0 if code == IMPOSSIBLE else 1
        print(f"{'OK    ' if code == IMPOSSIBLE else 'ÉCHEC '} {'objdump_absent':<34} attendu=impossible "
              f"obtenu={NOMS[code]}")
    print(f"check_core_static_init --self-test : {total} cas, {failures} échec(s), {skipped} ignoré(s) ({cxx})")
    return CONSTAT if failures else OK


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--objdump", required=True, help="objdump GNU (CMAKE_OBJDUMP)")
    parser.add_argument("--self-test", metavar="CXX", help="contre-épreuve avec ce compilateur C++")
    parser.add_argument("--ar", help="archiveur pour --self-test (défaut : ar du PATH)")
    parser.add_argument("libraries", nargs="*", help="bibliothèques statiques à contrôler")
    args = parser.parse_args()  # erreur d'usage : code 2
    if args.self_test:
        try:
            return self_test(args.objdump, args.self_test, args.ar or shutil.which("ar"))
        except (subprocess.CalledProcessError, OSError) as exc:  # ar, copie : outil en échec, jamais « constat »
            print(f"CONTRÔLE IMPOSSIBLE : contre-épreuve interrompue : {exc}")
            return IMPOSSIBLE
    if not args.libraries:
        parser.error("aucune bibliothèque à contrôler")
    worst = OK
    for library in args.libraries:
        code, lines = verdict(args.objdump, library)
        print("\n".join(lines))
        worst = max(worst, code)  # IMPOSSIBLE (2) l'emporte sur CONSTAT (1)
    return worst


if __name__ == "__main__":
    sys.exit(main())
