#!/usr/bin/env python3
"""Contrôle des couches de la base commune TSA / TSALab (docs/TSARALOHA_ARCHITECTURE.md, ADR-024).

  model     (<P>_Model)    : tout src/ sauf les couches ci-dessous ; aucun widget Qt
  graphics  (<P>_Graphics) : Viewer, Geometry, Interaction, rendu des grilles, UI/Theme, UI/Ruler
  widgets   (<P>_Widgets)  : composants d'interface partagés (reste de UI/, widgets NDC)
  app       (TSA)          : MainWindow, ruban, AppShell, Start Center, App/Application, main

Règle : une couche n'inclut que des fichiers de sa couche ou des couches inférieures
(model < graphics < widgets < app). Les widgets Qt sont interdits dans model.
Usage : python tools/check_layers.py [racine_src]   (code de sortie 1 si violation)
"""
import os
import re
import sys

ORDER = {"model": 0, "graphics": 1, "widgets": 2, "app": 3}

GRAPHICS_PREFIXES = (
    "Viewer/", "Geometry/", "Interaction/", "UI/Theme/", "UI/Ruler/",
    "Grid/GridRenderer", "Grid/GridLabelRenderer", "Grid/SnapMarker",
)
APP_PREFIXES = (
    "UI/MainWindow", "UI/Ribbon/", "UI/Shell/", "UI/Home/",
    "App/Application", "main.cpp", "ShellExtension/",
)
WIDGETS_PREFIXES = ("UI/", "NDC/NDCViewerWidget", "NDC/ReportConfigDialog")
# Fichiers d'en-tête de la couche modèle placés sous App/ (identité produit, sans widget).
MODEL_EXCEPTIONS = ("App/ProductInfo.h", "App/ProductHooks.h")
WIDGET_RE = re.compile(r"#include\s+<(?:QtWidgets/)?Q(?:Widget|MainWindow|Dialog|Action|Menu|Label|PushButton|"
                       r"Application|LineEdit|ComboBox|TreeWidget|TableWidget|DockWidget|ToolBar|MessageBox|"
                       r"FileDialog|Layout|VBoxLayout|HBoxLayout|GridLayout|CheckBox|SpinBox|Frame|ScrollArea)\b")
INCLUDE_RE = re.compile(r'#include\s+"([^"]+)"')


def layer_of(rel):
    if rel.startswith(MODEL_EXCEPTIONS):
        return "model"
    if rel.startswith(GRAPHICS_PREFIXES):
        return "graphics"
    if rel.startswith(APP_PREFIXES):
        return "app"
    if rel.startswith(WIDGETS_PREFIXES):
        return "widgets"
    return "model"


def main():
    src = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "src"))
    violations = []
    for root, _, files in os.walk(src):
        for name in files:
            if not name.endswith((".h", ".hpp", ".cpp")):
                continue
            path = os.path.join(root, name)
            rel = os.path.relpath(path, src).replace(os.sep, "/")
            layer = layer_of(rel)
            text = open(path, encoding="utf-8", errors="ignore").read()
            if layer == "model" and WIDGET_RE.search(text):
                violations.append(f"{rel} [model] inclut un widget Qt")
            for inc in INCLUDE_RE.findall(text):
                candidates = [os.path.normpath(os.path.join(root, inc)), os.path.normpath(os.path.join(src, inc))]
                target = next((c for c in candidates if os.path.isfile(c) and c.startswith(src)), None)
                if not target:
                    continue
                trel = os.path.relpath(target, src).replace(os.sep, "/")
                tlayer = layer_of(trel)
                if ORDER[tlayer] > ORDER[layer]:
                    violations.append(f"{rel} [{layer}] inclut {trel} [{tlayer}]")
    for v in sorted(set(violations)):
        print("VIOLATION", v)
    print(f"check_layers : {len(set(violations))} violation(s)")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
