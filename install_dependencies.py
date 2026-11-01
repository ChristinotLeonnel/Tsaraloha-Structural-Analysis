#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Télécharge et installe en une seule fois toutes les dépendances de TSA (et de TSALab) dans thirdparty/.

Usage (depuis n'importe quel dossier) :
    python3 install_dependencies.py                  # tout ce qui manque
    python3 install_dependencies.py --configure      # tout ce qui manque, puis cmake --preset (prêt à compiler)
    python3 install_dependencies.py --configure ninja-release   # idem avec un autre preset (CMakePresets.json)
    python3 install_dependencies.py --list           # état de chaque dépendance, rien n'est modifié
    python3 install_dependencies.py --only qt,occt   # seulement ces composants
    python3 install_dependencies.py --skip ifcopenshell
    python3 install_dependencies.py --force opensees # réinstaller un composant déjà présent
    python3 install_dependencies.py --no-system-packages   # Linux : ne pas lancer « sudo apt-get install »

Composants, dans l'ordre d'installation :
    system        Linux (Debian/Ubuntu) : compilateur, CMake, Ninja, en-têtes d'OCCT, bibliothèques X11 de Qt (apt)
    tsalab        dépôt voisin TSALab (cœur scientifique, obligatoire) : ../TSALab ou
                  ../Tsaraloha-Structural-Analysis-Virtual-Laboratory, sinon cloné (git) dans ../TSALab
    qt            Qt 6.12.0 (dépôt officiel download.qt.io)          -> thirdparty/Qt/6.12.0/<kit>
    occt          OpenCASCADE 8.0.1 : SDK officiel + 3rdparty (Windows), compilé depuis les sources (Linux)
    gmsh          SDK Gmsh 4.15.2 (gmsh.info)                         -> thirdparty/gmsh-4.15.2-<plateforme>64-sdk
    eigen         Eigen 3.4.1, en-têtes (moteur fem3d)                -> thirdparty/eigen-3.4.1
    opensees      OpenSees 3.8.0 (conda-forge, paquets épinglés)      -> thirdparty/OpenSees
    ifcopenshell  IfcOpenShell 0.9.0post1, compilé contre l'OCCT de TSA par scripts/linux/build-ifcopenshell.sh
                  ou scripts/build_ifcopenshell.ps1 (long : Boost puis IfcOpenShell)

Versions, adresses et empreintes SHA-256 : cmake/dependencies.json (lu aussi par cmake/SetupDependencies.cmake).
Toute archive est vérifiée avant extraction ; une empreinte différente arrête l'installation de ce composant.
Un composant déjà présent (dans thirdparty/ ou à un emplacement que CMake trouve) n'est pas réinstallé.
Ancienne disposition (opencascade-*-vc14-64 et 3rdparty-vc14-64 à la racine) : déplacée dans thirdparty/.
Windows : Visual Studio 2026 (C++ x64) est contrôlé, et --configure lance un preset Ninja dans l'environnement de
vcvars64.bat (inutile d'ouvrir l'invite « x64 Native Tools »).
Python 3.8 ou plus récent, bibliothèque standard seule ; CMake est utilisé pour extraire les archives 7z et zstd.
Documentation : docs/DEPENDENCIES.md, docs/LINUX_BUILD.md.
"""

import argparse
import glob
import hashlib
import json
import os
import platform
import re
import shutil
import socket
import subprocess
import sys
import tarfile
import time
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

if sys.version_info < (3, 8):
    sys.exit("install_dependencies.py : Python 3.8 ou plus récent est requis.")
for _stream in (sys.stdout, sys.stderr):   # sortie redirigée sous Windows (page de code 1252) : aucun plantage
    try:
        _stream.reconfigure(errors="replace")
    except (AttributeError, ValueError):
        pass

ROOT = Path(__file__).resolve().parent
MANIFEST_PATH = ROOT / "cmake" / "dependencies.json"
COMPONENTS = ["system", "tsalab", "qt", "occt", "gmsh", "eigen", "opensees", "ifcopenshell"]
OPTIONAL = {"gmsh", "eigen", "opensees", "ifcopenshell"}   # TSA se construit sans eux (fonction signalée absente)

# Commande de contrôle envoyée à OpenSees (même principe qu'OpenSeesManager::checkExecutable).
OPENSEES_CHECK = 'wipe; model BasicBuilder -ndm 2 -ndf 2; puts [format "OPENSEES_CHECK_OK_%s" [version]]; exit\n'


class InstallError(Exception):
    """Échec d'un composant : message destiné à l'utilisateur (cause et solution)."""


# --------------------------------------------------------------------------------------------------------------------
# Affichage
# --------------------------------------------------------------------------------------------------------------------
_COLOR = sys.stdout.isatty() and (os.name != "nt" or bool(os.environ.get("WT_SESSION")))


def _c(code, text):
    return f"\033[{code}m{text}\033[0m" if _COLOR else text


def info(msg):
    print(f"{_c('1;34', '[TSA]')} {msg}", flush=True)


def ok(msg):
    print(f"{_c('1;32', '[TSA]')} {msg}", flush=True)


def warn(msg):
    print(f"{_c('1;33', '[TSA] ATTENTION :')} {msg}", flush=True)


def fail(msg):
    print(f"{_c('1;31', '[TSA] ERREUR :')} {msg}", file=sys.stderr, flush=True)


def human(n):
    for unit in ("o", "Ko", "Mo", "Go"):
        if n < 1024 or unit == "Go":
            return f"{n:.0f} {unit}" if unit == "o" else f"{n:.1f} {unit}"
        n /= 1024.0
    return str(n)


# --------------------------------------------------------------------------------------------------------------------
# Contexte d'exécution
# --------------------------------------------------------------------------------------------------------------------
class Context:
    def __init__(self, args):
        self.args = args
        self.manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        self.platform = args.platform or ("windows" if os.name == "nt" else "linux")
        self.thirdparty = Path(args.thirdparty_dir).resolve() if args.thirdparty_dir else ROOT / "thirdparty"
        self.downloads = self.thirdparty / "_downloads"
        self.work = Path(args.work_dir).resolve() if args.work_dir else self.thirdparty / "_build"
        self.jobs = args.jobs or int(os.environ.get("TSA_JOBS", "0") or 0) or (os.cpu_count() or 2)
        self.cmake = args.cmake or shutil.which("cmake") or self._default_cmake()

    def _default_cmake(self):
        if os.name == "nt":
            for p in (r"C:\Program Files\CMake\bin\cmake.exe",):
                if os.path.exists(p):
                    return p
        return None

    @property
    def windows(self):
        return self.platform == "windows"

    @property
    def linux(self):
        return self.platform == "linux"

    def require_cmake(self):
        if not self.cmake:
            raise InstallError("CMake introuvable (nécessaire pour extraire les archives 7z et zstd) : l'installer "
                               "(Linux : composant « system ») ou indiquer --cmake <chemin>.")
        return self.cmake


# --------------------------------------------------------------------------------------------------------------------
# Téléchargement, empreintes, extraction
# --------------------------------------------------------------------------------------------------------------------
def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(ctx, url, sha256, name=None):
    """Télécharge url dans thirdparty/_downloads (réutilisé si l'empreinte correspond) et vérifie son SHA-256."""
    ctx.downloads.mkdir(parents=True, exist_ok=True)
    path = ctx.downloads / (name or url.rstrip("/").rsplit("/", 1)[-1])
    sha256 = sha256.lower()
    if path.exists():
        if sha256_of(path) == sha256:
            info(f"  {path.name} : déjà téléchargé (empreinte vérifiée)")
            return path
        path.unlink()
    part = path.with_name(path.name + ".part")
    last_error = None
    for attempt in range(1, 4):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "TSA-install-dependencies/1"})
            with urllib.request.urlopen(req, timeout=60) as r, open(part, "wb") as out:
                total = int(r.headers.get("Content-Length") or 0)
                done, shown, t0 = 0, -1, time.time()
                info(f"  téléchargement {url}" + (f" ({human(total)})" if total else ""))
                while True:
                    chunk = r.read(1 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
                    done += len(chunk)
                    if total:
                        pct = int(done * 100 / total) // 10 * 10
                        if pct != shown and (pct % 20 == 0 or time.time() - t0 > 10):
                            print(f"      {pct:3d} %  {human(done)}", flush=True)
                            shown = pct
            last_error = None
            break
        except (urllib.error.URLError, socket.timeout, OSError) as e:
            last_error = e
            warn(f"  tentative {attempt}/3 échouée : {e}")
            time.sleep(2 * attempt)
    if last_error is not None:
        if part.exists():
            part.unlink()
        raise InstallError(f"téléchargement impossible : {url} ({last_error})")
    got = sha256_of(part)
    if got != sha256:
        part.unlink()
        raise InstallError(f"empreinte SHA-256 inattendue pour {url}\n    obtenue  {got}\n    attendue {sha256}\n"
                           "  Archive supprimée, rien n'a été extrait (cmake/dependencies.json).")
    os.replace(part, path)
    return path


def _safe_target(dest, member_name):
    target = (dest / member_name).resolve()
    if os.path.commonpath([str(dest.resolve()), str(target)]) != str(dest.resolve()):
        raise InstallError(f"chemin hors du dossier de destination dans l'archive : {member_name}")
    return target


def extract(ctx, archive, dest):
    """Extrait archive dans dest : zip et tar par la bibliothèque standard, 7z et zstd par « cmake -E tar »."""
    dest = Path(dest)
    dest.mkdir(parents=True, exist_ok=True)
    name = archive.name.lower()
    if name.endswith(".zip") or name.endswith(".conda"):
        with zipfile.ZipFile(archive) as z:
            for m in z.infolist():
                _safe_target(dest, m.filename)
            z.extractall(dest)
    elif name.endswith((".tar.gz", ".tgz", ".tar.bz2", ".tar.xz", ".tar")):
        with tarfile.open(archive) as t:
            if hasattr(tarfile, "data_filter"):
                t.extractall(dest, filter="data")
            else:
                for m in t.getmembers():
                    _safe_target(dest, m.name)
                t.extractall(dest)
    else:
        subprocess.run([ctx.require_cmake(), "-E", "tar", "xf", str(archive.resolve())], cwd=str(dest), check=True,
                       stdout=subprocess.DEVNULL)


def run(cmd, cwd=None, env=None, log=None, progress=False):
    """Lance cmd ; avec log, la sortie va dans le journal (fin affichée en cas d'échec) et l'avancement
    « [n/N] » de Ninja est résumé tous les 5 %."""
    shown = " ".join(f'"{c}"' if " " in str(c) else str(c) for c in cmd)
    info(f"  $ {shown}")
    if log is None:
        r = subprocess.run([str(c) for c in cmd], cwd=cwd, env=env)
        if r.returncode != 0:
            raise InstallError(f"échec (code {r.returncode}) : {shown}")
        return
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    last = -5
    with open(log, "w", encoding="utf-8", errors="replace") as out:
        p = subprocess.Popen([str(c) for c in cmd], cwd=cwd, env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors="replace")
        for line in p.stdout:
            out.write(line)
            if progress:
                m = re.match(r"\[(\d+)/(\d+)\]", line)
                if m and int(m.group(2)) > 0:
                    pct = int(m.group(1)) * 100 // int(m.group(2))
                    if pct >= last + 5:
                        print(f"      {pct:3d} %  ({m.group(1)}/{m.group(2)})", flush=True)
                        last = pct
        p.wait()
    if p.returncode != 0:
        tail = log.read_text(encoding="utf-8", errors="replace").splitlines()[-40:]
        print("\n".join("      " + t for t in tail))
        raise InstallError(f"échec (code {p.returncode}) : {shown}\n  Journal complet : {log}")


def write_marker(directory, data):
    (Path(directory) / ".tsa-install.json").write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                                                        encoding="utf-8")


def newest(paths):
    """Chemin de la version la plus récente (tri par numéros de version dans le nom)."""
    def key(p):
        return [int(x) if x.isdigit() else x for x in re.split(r"(\d+)", str(p))]
    paths = sorted(paths, key=key)
    return paths[-1] if paths else None


def remove_tree(path):
    if Path(path).exists():
        shutil.rmtree(path, ignore_errors=False)


# --------------------------------------------------------------------------------------------------------------------
# Composants
# --------------------------------------------------------------------------------------------------------------------
class Component:
    name = ""
    title = ""
    platforms = ("linux", "windows")

    def __init__(self, ctx):
        self.ctx = ctx
        self.m = ctx.manifest

    def status(self):
        """(présent, détail). Présent : rien à installer."""
        raise NotImplementedError

    def install(self):
        raise NotImplementedError


class SystemPackages(Component):
    name, title, platforms = "system", "Paquets système (apt)", ("linux",)

    def _packages(self):
        return self.m["linux_system_packages"]["apt"]

    def _missing(self):
        if not shutil.which("dpkg-query"):
            return None
        missing = []
        for pkg in self._packages():
            r = subprocess.run(["dpkg-query", "-W", "-f=${Status}", pkg], stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, text=True)
            if "install ok installed" not in r.stdout:
                missing.append(pkg)
        return missing

    def status(self):
        missing = self._missing()
        if missing is None:
            return False, "dpkg/apt absents (distribution non Debian) : installer l'équivalent de " + \
                " ".join(self._packages())
        if not missing:
            return True, f"{len(self._packages())} paquets présents"
        return False, "manquants : " + " ".join(missing)

    def install(self):
        missing = self._missing()
        if missing is None:
            raise InstallError("gestionnaire apt introuvable : installer à la main l'équivalent de "
                               + " ".join(self._packages()) + " (docs/LINUX_BUILD.md § 1).")
        if not missing:
            return
        sudo = [] if os.geteuid() == 0 else ["sudo"]
        cmd = sudo + ["apt-get", "install", "-y", "--no-install-recommends"] + missing
        if self.ctx.args.no_system_packages:
            raise InstallError("paquets système manquants, installation désactivée (--no-system-packages ou CMake). "
                               "Lancer :\n    " + " ".join(cmd))
        info("Mot de passe administrateur éventuellement demandé par sudo (apt-get).")
        run(sudo + ["apt-get", "update"])
        run(cmd)


def _qt_version_of(prefix):
    for f in ("Qt6ConfigVersionImpl.cmake", "Qt6ConfigVersion.cmake"):
        p = Path(prefix) / "lib" / "cmake" / "Qt6" / f
        if p.exists():
            m = re.search(r'set\(PACKAGE_VERSION "([0-9.]+)"\)', p.read_text(encoding="utf-8", errors="replace"))
            if m:
                return m.group(1)
    return ""


def _vtuple(v):
    return tuple(int(x) for x in re.findall(r"\d+", v)[:3])


class TSALab(Component):
    """Dépôt voisin obligatoire (cœur scientifique TSALab/science) : cherché sous chacun des noms du manifeste,
    comme le fait cmake/TSAProduct.cmake, sinon cloné depuis GitHub sous le premier nom (../TSALab)."""
    name, title = "tsalab", "TSALab (cœur scientifique)"

    @property
    def spec(self):
        return self.m["tsalab"]

    def existing(self):
        for d in self.spec["dirs"]:
            p = ROOT.parent / d
            if (p / "science" / "CMakeLists.txt").exists():
                return p.resolve()
        return None

    def status(self):
        e = self.existing()
        if e:
            return True, str(e)
        return False, f"dépôt à cloner dans {ROOT.parent / self.spec['dirs'][0]}"

    def install(self):
        target = ROOT.parent / self.spec["dirs"][0]
        if target.exists() and any(target.iterdir()):
            raise InstallError(f"{target} existe mais ne contient pas science/CMakeLists.txt : le vérifier (dépôt "
                               "incomplet ?), ou indiquer -DTSALAB_ROOT_DIR=<chemin> à CMake.")
        git = shutil.which("git")
        if not git:
            raise InstallError(f"Git introuvable : installer Git (https://git-scm.com) puis relancer, ou cloner "
                               f"{self.spec['git_url']} dans {target}.")
        run([git, "clone", self.spec["git_url"], target])


class Qt(Component):
    name, title = "qt", "Qt 6"

    @property
    def spec(self):
        return self.m["qt"][self.ctx.platform]

    @property
    def target(self):
        return self.ctx.thirdparty / "Qt" / self.m["qt"]["version"] / self.spec["kit"]

    def _compatible(self, prefix):
        prefix = Path(prefix)
        if not (prefix / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").exists():
            return False
        if not (prefix / "lib" / "cmake" / "Qt6Svg").is_dir():
            return False
        if self.ctx.windows and not (prefix / "lib" / "Qt6Core.lib").exists():
            return False   # kit MSVC exigé (MinGW, ARM64… refusés par CMake)
        if self.ctx.linux and not glob.glob(str(prefix / "lib" / "libQt6Core.so*")):
            return False
        return _vtuple(_qt_version_of(prefix)) >= (6, 2)

    def existing(self):
        """Qt compatible déjà installée là où cmake/SetupDependencies.cmake (tsa_find_qt) la cherche."""
        cands = []
        for var in ("Qt6_DIR", "QTDIR", "TSA_QT_DIR"):
            v = os.environ.get(var)
            if v:
                p = Path(v)
                cands.append(p.parents[2] if (p / "Qt6Config.cmake").exists() and len(p.parents) > 2 else p)
        for p in os.environ.get("CMAKE_PREFIX_PATH", "").split(os.pathsep):
            if p:
                cands.append(Path(p))
        roots = [self.ctx.thirdparty / "Qt"]
        if self.ctx.windows:
            roots += [Path("C:/Qt"), Path(os.environ.get("SystemDrive", "C:") + "/Qt"),
                      Path(os.environ.get("USERPROFILE", "")) / "Qt"]
        else:
            roots += [Path.home() / "Qt"]
        for r in roots:
            cands += [Path(k) for k in glob.glob(str(r / "6.*" / "*"))]
        good = [c for c in cands if self._compatible(c)]
        return max(good, key=lambda c: _vtuple(_qt_version_of(c))) if good else None

    def status(self):
        if self._compatible(self.target):
            return True, str(self.target)
        e = self.existing()
        if e:
            return True, f"Qt {_qt_version_of(e)} déjà installée : {e} (retenue par CMake)"
        return False, f"Qt {self.m['qt']['version']} ({self.spec['kit']}) à installer dans {self.target}"

    def install(self):
        version = self.m["qt"]["package_version"]
        stage = self.ctx.work / f"qt-{self.spec['kit']}"
        remove_tree(stage)
        for a in self.spec["archives"]:
            archive = download(self.ctx, self.spec["base_url"] + version + a["name"], a["sha256"], a["name"])
            info(f"  extraction {a['name']}")
            extract(self.ctx, archive, stage / a.get("subdir", ""))
        # Chemins relatifs à l'installation (comme l'installateur de Qt et aqtinstall) : qtpaths, windeployqt.
        (stage / "bin").mkdir(exist_ok=True)
        (stage / "bin" / "qt.conf").write_text("[Paths]\nPrefix=..\n", encoding="utf-8")
        write_marker(stage, {"component": "qt", "version": version, "kit": self.spec["kit"]})
        if not self._compatible(stage):
            raise InstallError(f"installation de Qt incomplète dans {stage}")
        remove_tree(self.target)
        self.target.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(stage), str(self.target))


def find_linux_occt(ctx):
    """Préfixe OCCT 8 sous Linux : thirdparty/, TSA_OCCT_DIR, ~/opt/occt-8.* (comme scripts/linux/common.sh)."""
    cfg = Path("lib") / "cmake" / "opencascade" / "OpenCASCADEConfig.cmake"
    cands = [ctx.thirdparty / ctx.manifest["occt"]["linux"]["install_dir"]]
    if os.environ.get("TSA_OCCT_DIR"):
        cands.append(Path(os.environ["TSA_OCCT_DIR"]))
    n = newest(glob.glob(str(Path.home() / "opt" / "occt-8.*")))
    if n:
        cands.append(Path(n))
    for c in cands:
        if (c / cfg).exists():
            return c
    return None


class Occt(Component):
    name, title = "occt", "OpenCASCADE"

    def _win_dir(self, key):
        """thirdparty/<dossier>, ou l'ancien emplacement à la racine s'il n'a pas encore pu être déplacé."""
        name = self.m["occt"]["windows"][key]
        new, old = self.ctx.thirdparty / name, ROOT / name
        if not new.exists() and old.is_dir() and self.ctx.thirdparty == ROOT / "thirdparty":
            return old
        return new

    def status(self):
        if self.ctx.windows:
            sdk = self._win_dir("sdk_dir")
            tp = self._win_dir("thirdparty_dir")
            have_sdk = (sdk / "cmake" / "OpenCASCADEConfig.cmake").exists()
            have_tp = tp.is_dir() and any(tp.iterdir())
            if have_sdk and have_tp:
                legacy = " (ancien emplacement, à déplacer)" if sdk.parent == ROOT or tp.parent == ROOT else ""
                return True, f"{sdk} + {tp.name}{legacy}"
            return False, ("SDK manquant" if not have_sdk else "") + (" 3rdparty manquant" if not have_tp else "")
        found = find_linux_occt(self.ctx)
        if found:
            return True, str(found)
        return False, "à compiler depuis les sources (~15 à 30 min)"

    def install(self):
        if self.ctx.windows:
            self._install_windows()
        else:
            self._install_linux()

    def _install_windows(self):
        w = self.m["occt"]["windows"]
        forced = "occt" in self.ctx.args.force or "all" in self.ctx.args.force
        for key, dir_key in (("sdk", "sdk_dir"), ("thirdparty", "thirdparty_dir")):
            present = self._win_dir(dir_key)
            if not forced and key == "sdk" and (present / "cmake" / "OpenCASCADEConfig.cmake").exists():
                continue
            if not forced and key == "thirdparty" and present.is_dir() and any(present.iterdir()):
                continue
            final = self.ctx.thirdparty / w[dir_key]
            archive = download(self.ctx, w[key]["url"], w[key]["sha256"], f"occt-{w[dir_key]}.zip")
            stage = self.ctx.work / f"occt-{key}"
            remove_tree(stage)
            info(f"  extraction {archive.name}")
            extract(self.ctx, archive, stage / "outer")
            # Les archives GitHub d'OCCT contiennent une seconde archive (ex. opencascade-8.0.1-vc14-64.zip).
            inner = sorted((stage / "outer").glob("*.zip"))
            src = stage / "outer"
            if inner:
                extract(self.ctx, inner[0], stage / "inner")
                src = stage / "inner"
            if (src / w[dir_key]).is_dir():
                src = src / w[dir_key]
            remove_tree(final)
            shutil.move(str(src), str(final))
            remove_tree(stage)
        if not (self.ctx.thirdparty / w["sdk_dir"] / "cmake" / "OpenCASCADEConfig.cmake").exists():
            raise InstallError("SDK OpenCASCADE incomplet après extraction")

    def _install_linux(self):
        lx = self.m["occt"]["linux"]
        prefix = self.ctx.thirdparty / lx["install_dir"]
        archive = download(self.ctx, lx["source"]["url"], lx["source"]["sha256"], f"occt-{self.m['occt']['version']}.tar.gz")
        src_root = self.ctx.work / "occt-src"
        build = self.ctx.work / "occt-build"
        if not (src_root / lx["source"]["dir"]).is_dir():
            info("  extraction des sources")
            extract(self.ctx, archive, src_root)
        # Modules de TSA (FoundationClasses, ModelingData, ModelingAlgorithms, Visualization) + DataExchange et
        # ApplicationFramework pour IfcOpenShell ; RPATH $ORIGIN : bibliothèques autonomes (docs/LINUX_BUILD.md § 2).
        run([self.ctx.require_cmake(), "-S", src_root / lx["source"]["dir"], "-B", build, "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={prefix}", "-DCMAKE_INSTALL_RPATH=$ORIGIN",
             "-DBUILD_LIBRARY_TYPE=Shared", "-DBUILD_MODULE_ApplicationFramework=ON",
             "-DBUILD_MODULE_DataExchange=ON", "-DBUILD_MODULE_Draw=OFF", "-DUSE_TK=OFF", "-DUSE_FREETYPE=ON",
             "-DUSE_OPENGL=ON", "-DUSE_XLIB=ON"], log=self.ctx.work / "occt-configure.log")
        info(f"  compilation d'OpenCASCADE ({self.ctx.jobs} tâches ; journal {self.ctx.work / 'occt-build.log'})")
        run([self.ctx.require_cmake(), "--build", build, "--parallel", str(self.ctx.jobs)],
            log=self.ctx.work / "occt-build.log", progress=True)
        run([self.ctx.require_cmake(), "--install", build], log=self.ctx.work / "occt-install.log")
        if not (prefix / "lib" / "cmake" / "opencascade" / "OpenCASCADEConfig.cmake").exists():
            raise InstallError(f"installation d'OCCT incomplète : {prefix}")
        write_marker(prefix, {"component": "occt", "version": self.m["occt"]["version"], "source": lx["source"]})
        remove_tree(build)
        remove_tree(src_root)


class Gmsh(Component):
    name, title = "gmsh", "Gmsh (SDK)"

    @property
    def target(self):
        return self.ctx.thirdparty / self.m["gmsh"][self.ctx.platform]["dir"]

    def status(self):
        if (self.target / "include" / "gmsh.h").exists():
            return True, str(self.target)
        if self.ctx.linux:
            for c in [os.environ.get("TSA_GMSH_SDK"), newest(glob.glob(str(Path.home() / "opt" / "gmsh-*-Linux64-sdk")))]:
                if c and (Path(c) / "include" / "gmsh.h").exists():
                    return True, f"{c} (TSA_GMSH_SDK)"
        return False, f"à installer dans {self.target}"

    def install(self):
        g = self.m["gmsh"][self.ctx.platform]
        archive = download(self.ctx, g["url"], g["sha256"])
        stage = self.ctx.work / "gmsh"
        remove_tree(stage)
        info(f"  extraction {archive.name}")
        extract(self.ctx, archive, stage)
        src = stage / g["dir"]
        if not (src / "include" / "gmsh.h").exists():
            raise InstallError(f"archive Gmsh inattendue : {g['dir']}/include/gmsh.h absent")
        remove_tree(self.target)
        shutil.move(str(src), str(self.target))
        remove_tree(stage)


class Eigen(Component):
    name, title = "eigen", "Eigen (en-têtes)"

    @property
    def target(self):
        return self.ctx.thirdparty / self.m["eigen"]["dir"]

    def status(self):
        if (self.target / "include" / "eigen3" / "Eigen" / "Core").exists():
            return True, str(self.target)
        return False, f"à installer dans {self.target}"

    def install(self):
        e = self.m["eigen"]
        archive = download(self.ctx, e["url"], e["sha256"])
        stage = self.ctx.work / "eigen"
        remove_tree(stage)
        extract(self.ctx, archive, stage)
        src = stage / e["dir"]
        inc = self.target / "include" / "eigen3"
        inc.mkdir(parents=True, exist_ok=True)
        for d in ("Eigen", "unsupported"):
            remove_tree(inc / d)
            shutil.copytree(str(src / d), str(inc / d))
        shutil.copy2(str(src / "signature_of_eigen3_matrix_library"), str(inc))
        lic = self.target / "licenses"
        lic.mkdir(exist_ok=True)
        for f in src.glob("COPYING.*"):
            shutil.copy2(str(f), str(lic))
        remove_tree(stage)


# --- OpenSees -------------------------------------------------------------------------------------------------------
def same_length_path(prefix, length):
    """prefix complété par des « /. » (et « / ») sans effet jusqu'à length octets : chemin équivalent."""
    filler = length - len(prefix)
    if filler < 0:
        raise InstallError(f"chemin d'installation trop long pour OpenSees ({len(prefix)} > {length} caractères) : "
                           "utiliser --thirdparty-dir avec un chemin plus court")
    return prefix + b"/." * (filler // 2) + b"/" * (filler % 2)


def conda_replace_binary(data, placeholder, new):
    """Remplacement du préfixe dans un binaire, comme conda (chaînes terminées par NUL, longueur conservée).
    Appelé avec un new de même longueur que placeholder (same_length_path) : aucun octet ne se décale, ce qui
    préserve les chaînes partagées par suffixe de la table .dynstr (ex. « libtcl8.6.so », fin de
    « <préfixe>/lib/libtcl8.6.so »), que le remplacement de conda viderait."""
    def repl(match):
        occ = match.group().count(placeholder)
        padding = (len(placeholder) - len(new)) * occ
        if padding < 0:
            raise InstallError(f"chemin d'installation trop long pour OpenSees ({len(new)} > {len(placeholder)} "
                               "caractères) : utiliser --thirdparty-dir avec un chemin plus court")
        return match.group().replace(placeholder, new) + b"\0" * padding
    pat = re.compile(re.escape(placeholder) + b"([^\0]*?)\0")
    out = pat.sub(repl, data)
    assert len(out) == len(data)
    return out


def read_has_prefix(info_dir):
    """Entrées de info/has_prefix : (marqueur, mode, chemin relatif)."""
    p = Path(info_dir) / "has_prefix"
    entries = []
    if p.exists():
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            line = line.strip()
            if not line:
                continue
            parts = line.split(" ")
            if len(parts) >= 3:
                entries.append((parts[0], parts[1], " ".join(parts[2:])))
            else:
                entries.append(("/opt/anaconda1anaconda2anaconda3", "text", line))
    return entries


def run_opensees_check(exe):
    """Lance OpenSees avec la commande de contrôle (sans fenêtre) ; renvoie (réussi, détail)."""
    flags = 0x08000000 if os.name == "nt" else 0   # CREATE_NO_WINDOW
    try:
        r = subprocess.run([str(exe)], input=OPENSEES_CHECK, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, errors="replace", timeout=60, creationflags=flags)
    except (OSError, subprocess.TimeoutExpired) as e:
        return False, str(e)
    m = re.search(r"OPENSEES_CHECK_OK_([0-9.]+)", r.stdout)
    if m:
        return True, f"OpenSees {m.group(1)} répond"
    return False, (r.stdout.strip().splitlines() or ["aucune sortie"])[-1]


class OpenSees(Component):
    name, title = "opensees", "OpenSees (moteur externe)"

    @property
    def target(self):
        return self.ctx.thirdparty / "OpenSees"

    def _exe(self):
        names = ["bin/OpenSees.exe", "bin/gOpenSees.exe"] if self.ctx.windows else ["bin/OpenSees"]
        for n in names:
            if (self.target / n).exists():
                return self.target / n
        return None

    def status(self):
        exe = self._exe()
        if exe:
            return True, str(exe)
        found = shutil.which("OpenSees") or shutil.which("gOpenSees")
        if found and not self.ctx.args.platform:
            return True, f"{found} (PATH)"
        return False, f"à installer dans {self.target}"

    def install(self):
        spec = self.m["opensees"][self.ctx.platform]
        channel = self.m["opensees"]["channel_url"]
        stage = self.ctx.work / f"opensees-{self.ctx.platform}"
        remove_tree(stage)
        root = stage / "OpenSees"
        (root / "bin").mkdir(parents=True)
        (root / "lib").mkdir(parents=True)
        prefix_fixes = []   # (fichier installé, marqueur, mode)
        for pkg in spec["packages"]:
            fname = pkg["file"] + ".conda"
            archive = download(self.ctx, f"{channel}/{spec['subdir']}/{fname}", pkg["sha256"], fname)
            x = stage / pkg["file"]
            extract(self.ctx, archive, x)
            for zst in x.glob("*.tar.zst"):          # .conda = zip de pkg-*.tar.zst et info-*.tar.zst
                extract(self.ctx, zst, x)
                zst.unlink()
            if self.ctx.windows:
                self._take_windows(pkg, x, root)
            else:
                prefix_fixes += self._take_linux(x, root)
            remove_tree(x)
        if not (root / "lib" / "tcl8.6" / "init.tcl").exists():
            raise InstallError("bibliothèque Tcl 8.6 absente des paquets OpenSees")
        if self.ctx.linux:
            # Chemin de construction conda inscrit dans les binaires (info/has_prefix). Dans bin/OpenSees, sa seule
            # occurrence est l'entrée DT_NEEDED de libtcl8.6.so : « $ORIGIN/.. » (développé par ld.so) la rend
            # indépendante de l'emplacement du dépôt. Bibliothèques : chemin absolu de l'installation, comme conda
            # (chemins de configuration de hdf5, curl, openssl, krb5, inutilisés par OpenSees).
            final = str(self.target.resolve()).encode("utf-8")
            for path, placeholder, mode in prefix_fixes:
                data = path.read_bytes()
                ph = placeholder.encode("utf-8")
                if ph in data:
                    new = b"$ORIGIN/.." if path == root / "bin" / "OpenSees" else final
                    if mode == "binary":
                        data = conda_replace_binary(data, ph, same_length_path(new, len(ph)))
                    else:
                        data = data.replace(ph, new)
                    path.write_bytes(data)
            (root / "bin" / "OpenSees").chmod(0o755)
        lines = [f"OpenSees {self.m['opensees']['version']} (licence UC, docs/THIRD_PARTY_LICENSES.md), compilé par "
                 "conda-forge depuis https://github.com/OpenSees/OpenSees/archive/refs/tags/v3.8.0.tar.gz "
                 "(recette conda-forge/opensees-feedstock).",
                 f"Paquets {spec['subdir']} (cmake/dependencies.json), installés par install_dependencies.py :"]
        lines += [f"  {p['file']}|{p['sha256']}" for p in spec["packages"]]
        source_name = "SOURCE.txt" if self.ctx.windows else "SOURCE-linux.txt"
        (root / source_name).write_text("\n".join(lines) + "\n", encoding="utf-8")
        # Copie seulement si tout a réussi ; bin/ et lib/ sont partagés avec l'autre plateforme (noms distincts :
        # OpenSees.exe et DLL sous Windows, OpenSees et lib*.so* sous Linux ; lib/tcl8.6 identique).
        self.target.mkdir(parents=True, exist_ok=True)
        if self.ctx.linux:   # réinstallation : anciens fichiers Linux retirés (liens symboliques non écrasables)
            for old in list((self.target / "lib").glob("*.so*")) + [self.target / "bin" / "OpenSees"]:
                if old.is_symlink() or old.is_file():
                    old.unlink()
        shutil.copytree(str(root), str(self.target), symlinks=True, dirs_exist_ok=True)
        remove_tree(stage)
        exe = self._exe()
        if exe and not self.ctx.args.platform:
            good, detail = run_opensees_check(exe)
            (ok if good else warn)(f"  contrôle : {detail}")

    def _take_windows(self, pkg, x, root):
        for f in pkg["files"]:
            src = x / "Library" / "bin" / f
            if not src.exists():
                raise InstallError(f"{f} absent de {pkg['file']}")
            shutil.copy2(str(src), str(root / "bin"))
        self._take_tcl(x / "Library" / "lib", root)

    def _take_linux(self, x, root):
        fixes = []
        if (x / "bin" / "OpenSees").exists():
            shutil.copy2(str(x / "bin" / "OpenSees"), str(root / "bin" / "OpenSees"))
        lib = x / "lib"
        if lib.is_dir():
            for entry in lib.iterdir():
                if ".so" not in entry.name or entry.is_dir():
                    continue
                dest = root / "lib" / entry.name
                if entry.is_symlink():
                    if dest.exists() or dest.is_symlink():
                        dest.unlink()
                    os.symlink(os.readlink(str(entry)), str(dest))
                else:
                    shutil.copy2(str(entry), str(dest))
        self._take_tcl(lib, root)
        for placeholder, mode, rel in read_has_prefix(x / "info"):
            dest = root / rel
            if (rel.startswith("lib/") and "/" not in rel[4:] or rel == "bin/OpenSees") and dest.is_file() \
                    and not dest.is_symlink():
                fixes.append((dest, placeholder, mode))
        return fixes

    @staticmethod
    def _take_tcl(lib, root):
        if (lib / "tcl8.6" / "init.tcl").exists():
            # Bibliothèque de scripts Tcl et modules (.tm : msgcat, utilisé par clock, http, platform…).
            for d in ("tcl8.6", "tcl8"):
                if (lib / d).is_dir():
                    shutil.copytree(str(lib / d), str(root / "lib" / d), dirs_exist_ok=True)


class IfcOpenShell(Component):
    name, title = "ifcopenshell", "IfcOpenShell (import IFC)"

    @property
    def target(self):
        key = "windows_dir" if self.ctx.windows else "linux_dir"
        return self.ctx.thirdparty / self.m["ifcopenshell"][key]

    def _cfg(self, prefix):
        return Path(prefix) / "lib" / "cmake" / "IfcOpenShell" / "IfcOpenShellConfig.cmake"

    def status(self):
        if self._cfg(self.target).exists():
            return True, str(self.target)
        if self.ctx.linux:
            for c in [os.environ.get("TSA_IFCOPENSHELL_ROOT"), newest(glob.glob(str(Path.home() / "opt" / "ifcopenshell-*")))]:
                if c and self._cfg(c).exists():
                    return True, f"{c} (TSA_IFCOPENSHELL_ROOT)"
        return False, "à compiler (Boost puis IfcOpenShell : long, mémoire importante)"

    def install(self):
        if self.ctx.windows:
            occt = self.ctx.thirdparty / self.m["occt"]["windows"]["sdk_dir"]
            if not (occt / "inc" / "Standard_Version.hxx").exists():
                raise InstallError("OpenCASCADE doit être installé avant IfcOpenShell (composant occt)")
            run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                 ROOT / "scripts" / "build_ifcopenshell.ps1", "-OcctRoot", occt, "-Prefix", self.ctx.thirdparty])
        else:
            occt = find_linux_occt(self.ctx)
            if not occt:
                raise InstallError("OpenCASCADE doit être installé avant IfcOpenShell (composant occt)")
            if not (occt / "lib" / "libTKDESTEP.so").exists():
                raise InstallError(f"l'OCCT de {occt} n'a pas le module DataExchange exigé par IfcOpenShell : "
                                   "le recompiler (docs/LINUX_BUILD.md § 2) ou installer celui de thirdparty/ "
                                   "(--force occt)")
            env = dict(os.environ, TSA_OCCT_DIR=str(occt), TSA_JOBS=os.environ.get("TSA_JOBS", "4"))
            run(["bash", ROOT / "scripts" / "linux" / "build-ifcopenshell.sh", self.target,
                 self.ctx.work / "ifcopenshell"], env=env)
        if not self._cfg(self.target).exists():
            raise InstallError(f"IfcOpenShell non installé dans {self.target}")


CLASSES = [SystemPackages, TSALab, Qt, Occt, Gmsh, Eigen, OpenSees, IfcOpenShell]


# --------------------------------------------------------------------------------------------------------------------
# Configuration CMake (--configure) et compilateur Windows
# --------------------------------------------------------------------------------------------------------------------
def find_vcvars64():
    """vcvars64.bat de la dernière Visual Studio dotée des outils C++ x64 (vswhere), ou None."""
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / \
        "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None
    r = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                       capture_output=True, text=True, errors="replace")
    for line in r.stdout.splitlines():
        p = Path(line.strip()) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
        if line.strip() and p.exists():
            return p
    return None


def preset_generator(preset):
    """Générateur du preset de configuration (héritage suivi) ; InstallError si le preset n'existe pas."""
    presets = {p["name"]: p for p in
               json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))["configurePresets"]}
    if preset not in presets or presets[preset].get("hidden"):
        visible = [n for n, p in presets.items() if not p.get("hidden")]
        raise InstallError(f"preset CMake inconnu « {preset} » (disponibles : {', '.join(visible)})")
    p, seen = presets[preset], set()
    while p and p["name"] not in seen:
        seen.add(p["name"])
        if p.get("generator"):
            return p["generator"]
        inh = p.get("inherits")
        inh = inh[0] if isinstance(inh, list) and inh else inh
        p = presets.get(inh) if isinstance(inh, str) else None
    return ""


def configure(ctx, preset):
    """cmake --preset <preset> ; sous Windows, un preset Ninja est lancé dans l'environnement de vcvars64.bat."""
    if preset == "auto":
        preset = "ninja-debug" if ctx.windows else "linux-debug"
    generator = preset_generator(preset)
    cmake = ctx.cmake
    if not cmake:
        raise InstallError("CMake introuvable : l'installer (https://cmake.org, 3.20 ou plus) ou indiquer --cmake.")
    info(f"Configuration CMake : preset {preset} ({generator or 'générateur par défaut'})")
    if ctx.windows:
        vcvars = find_vcvars64()
        if not vcvars:
            raise InstallError("Visual Studio avec les outils C++ introuvable : installer Visual Studio 2026 Community, "
                               "charge de travail « Développement Desktop en C++ » (MSVC x64 + Windows SDK), puis "
                               "relancer. Qt et OCCT de TSA sont des builds MSVC : MinGW ne convient pas.")
        if "Visual Studio" not in generator and not shutil.which("cl"):
            line = f'"{vcvars}" >nul && "{cmake}" --preset {preset}'
            info(f"  $ {line}")
            env = dict(os.environ)   # vcvars64.bat appelle vswhere sans chemin : dossier de l'installateur dans le PATH
            env["PATH"] = str(Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) /
                              "Microsoft Visual Studio" / "Installer") + os.pathsep + env.get("PATH", "")
            rc = subprocess.call(f'cmd /d /s /c "{line}"', cwd=str(ROOT), env=env)
            build = f'"{vcvars}" puis cmake --build --preset {preset}  (ou invite « x64 Native Tools »)'
        else:
            rc = subprocess.call([cmake, "--preset", preset], cwd=str(ROOT))
            build = f"cmake --build --preset {preset}"
    else:
        rc = subprocess.call([cmake, "--preset", preset], cwd=str(ROOT))
        build = f"cmake --build --preset {preset}"
    if rc != 0:
        raise InstallError(f"échec de la configuration CMake (code {rc}) : voir les messages ci-dessus.")
    ok(f"Projet configuré. Compilation : {build}")


# --------------------------------------------------------------------------------------------------------------------
# Ancienne disposition : SDK OCCT à la racine du dépôt
# --------------------------------------------------------------------------------------------------------------------
def migrate_layout(ctx, dry_run=False):
    """Déplace opencascade-*-vc14-64 et 3rdparty-vc14-64 de la racine vers thirdparty/ (même volume : simple
    renommage) et met à jour le chemin d'OCCT inscrit dans la construction Windows d'IfcOpenShell."""
    w = ctx.manifest["occt"]["windows"]
    moved = False
    if ctx.thirdparty != ROOT / "thirdparty":
        return moved   # --thirdparty-dir (essais) : le dépôt n'est jamais modifié
    for d in (w["sdk_dir"], w["thirdparty_dir"]):
        old, new = ROOT / d, ctx.thirdparty / d
        if not old.is_dir():
            continue
        if new.exists():
            warn(f"{old} et {new} existent tous deux : l'ancien dossier n'est plus utilisé, le supprimer à la main.")
            continue
        if dry_run:
            info(f"à déplacer : {old} -> {new}")
            continue
        new.parent.mkdir(parents=True, exist_ok=True)
        info(f"déplacement {old.name} -> thirdparty/{new.name}")
        try:
            os.replace(str(old), str(new))
        except OSError as e:
            # Dossier verrouillé (fenêtre de l'Explorateur, indexation, antivirus…) : l'ancien emplacement reste
            # utilisable (CMake s'y replie aussi) ; rien n'est retéléchargé.
            warn(f"déplacement impossible de {old} ({e}) : ancien emplacement conservé pour l'instant. Fermer ce "
                 "qui l'utilise (fenêtre de l'Explorateur, Visual Studio, TSA.exe…) puis relancer le script.")
            continue
        moved = True
    targets = ctx.thirdparty / ctx.manifest["ifcopenshell"]["windows_dir"] / "lib" / "cmake" / "IfcOpenShell" / \
        "IfcOpenShellTargets.cmake"
    new_sdk = ctx.thirdparty / w["sdk_dir"] / "cmake" / "OpenCASCADEConfig.cmake"
    if targets.exists() and new_sdk.exists() and not dry_run:   # seulement une fois le SDK réellement déplacé
        text = targets.read_text(encoding="utf-8")
        fixed = re.sub(r"(?<!/thirdparty)/" + re.escape(w["sdk_dir"]) + r"(?=[/\"])", "/thirdparty/" + w["sdk_dir"],
                       text)
        if fixed != text:
            targets.write_text(fixed, encoding="utf-8")
            info(f"chemin d'OCCT mis à jour dans {targets.relative_to(ROOT) if ROOT in targets.parents else targets}")
    return moved


# --------------------------------------------------------------------------------------------------------------------
# Programme principal
# --------------------------------------------------------------------------------------------------------------------
def parse_args(argv):
    p = argparse.ArgumentParser(description="Télécharge et installe toutes les dépendances de TSA dans thirdparty/.",
                                formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__.split("\n\n", 2)[1])
    p.add_argument("--list", action="store_true", help="afficher l'état de chaque dépendance sans rien modifier")
    p.add_argument("--only", default="", help="composants à traiter, séparés par des virgules")
    p.add_argument("--skip", default="", help="composants à ignorer, séparés par des virgules")
    p.add_argument("--force", default="", help="composants à réinstaller même s'ils sont présents ('all' : tous)")
    p.add_argument("--no-system-packages", action="store_true",
                   help="Linux : signaler les paquets système manquants sans lancer sudo apt-get")
    p.add_argument("--jobs", type=int, default=0, help="compilations parallèles (défaut : TSA_JOBS ou nombre de cœurs)")
    p.add_argument("--cmake", default="", help="exécutable CMake à utiliser")
    p.add_argument("--from-cmake", action="store_true", help=argparse.SUPPRESS)   # appel par SetupDependencies.cmake
    p.add_argument("--platform", choices=("linux", "windows"), help=argparse.SUPPRESS)   # essais
    p.add_argument("--thirdparty-dir", default="", help=argparse.SUPPRESS)               # essais
    p.add_argument("--work-dir", default="", help="dossier de travail des compilations (défaut : thirdparty/_build)")
    p.add_argument("--configure", nargs="?", const="auto", default="", metavar="PRESET",
                   help="ensuite, configurer CMake avec ce preset (défaut : ninja-debug sous Windows, linux-debug "
                        "sous Linux)")
    a = p.parse_args(argv)
    for opt in ("only", "skip", "force"):
        vals = [v.strip() for v in getattr(a, opt).split(",") if v.strip()]
        unknown = [v for v in vals if v not in COMPONENTS and not (opt == "force" and v == "all")]
        if unknown:
            p.error(f"--{opt} : composant inconnu {', '.join(unknown)} (connus : {', '.join(COMPONENTS)})")
        setattr(a, opt, vals)
    if a.from_cmake:
        a.no_system_packages = True
    return a


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    ctx = Context(args)
    if ctx.platform == "linux" and platform.machine().lower() not in ("x86_64", "amd64") and not args.platform:
        fail(f"architecture {platform.machine()} : seules les dépendances Linux x86_64 sont publiées.")
        return 2
    comps = [cls(ctx) for cls in CLASSES if ctx.platform in cls.platforms]
    if args.only:
        comps = [c for c in comps if c.name in args.only]
    comps = [c for c in comps if c.name not in args.skip]
    forced = set(COMPONENTS) if "all" in args.force else set(args.force)

    info(f"Dépendances de TSA — {ctx.platform}, dossier {ctx.thirdparty}")
    if args.list:
        migrate_layout(ctx, dry_run=True)
        for c in comps:
            present, detail = c.status()
            mark = _c("32", "PRÉSENT ") if present else _c("33", "MANQUANT")
            print(f"  {mark}  {c.title:<28} {detail}")
        return 0

    ctx.thirdparty.mkdir(parents=True, exist_ok=True)
    try:
        migrate_layout(ctx)
    except InstallError as e:
        fail(str(e))
        return 1

    results = []
    for c in comps:
        present, detail = c.status()
        if present and c.name not in forced:
            ok(f"{c.title} : présent — {detail}")
            results.append((c, True, detail))
            continue
        info(f"{c.title} : installation ({detail})")
        t0 = time.time()
        try:
            c.install()
            present, detail = c.status()
            if not present:
                raise InstallError(detail)
            ok(f"{c.title} : installé en {time.time() - t0:.0f} s — {detail}")
            results.append((c, True, detail))
        except InstallError as e:
            fail(f"{c.title} : {e}")
            results.append((c, False, str(e).splitlines()[0]))
        except (OSError, subprocess.SubprocessError, zipfile.BadZipFile, tarfile.TarError) as e:
            # Archive corrompue, extraction ou copie impossible… : échec de ce composant seulement.
            fail(f"{c.title} : {type(e).__name__} : {e}")
            results.append((c, False, f"{type(e).__name__} : {e}"))
        except KeyboardInterrupt:
            fail("interrompu ; relancer le script reprend là où il s'est arrêté (téléchargements conservés).")
            return 130

    print()
    info("Bilan :")
    for c, good, detail in results:
        tag = _c("32", "OK      ") if good else (_c("33", "ABSENT  ") if c.name in OPTIONAL else _c("31", "ÉCHEC   "))
        print(f"  {tag}  {c.title:<28} {detail}")
    if ctx.windows and not args.from_cmake:
        vcvars = find_vcvars64()
        tag = _c("32", "OK      ") if vcvars else _c("31", "ABSENT  ")
        print(f"  {tag}  {'Visual Studio (MSVC x64)':<28} "
              f"{vcvars or 'obligatoire : Visual Studio 2026, « Développement Desktop en C++ »'}")
    failed = [c for c, good, _ in results if not good]
    required_ok = all(c.name in OPTIONAL for c in failed)
    if not failed:
        ok("Toutes les dépendances demandées sont en place.")
    else:
        warn("Composants facultatifs absents : TSA se construit sans eux (fonction correspondante indisponible)."
             if required_ok else "Dépendances obligatoires manquantes : voir ci-dessus.")
    if not required_ok:
        return 1
    if args.configure:
        try:
            configure(ctx, args.configure)
        except InstallError as e:
            fail(str(e))
            return 1
    elif not args.from_cmake:
        info("Suite : python install_dependencies.py --configure, ou cmake --preset "
             + ("ninja-debug (invite x64 Native Tools)" if ctx.windows else "linux-debug") + ".")
    return 0


if __name__ == "__main__":
    sys.exit(main())
