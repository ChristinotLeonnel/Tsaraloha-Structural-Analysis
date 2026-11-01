# Mémoire de Claude Code (copie versionnée)

Copie de la mémoire automatique de Claude Code (`MEMORY.md` et ses notes), versionnée pour continuer le travail sur
un autre poste. Elle complète la mémoire du projet (`.claude/CLAUDE.md`, `current-state.md`, `tasks.md`,
`known-issues.md`, `changelog.md`, `decisions.md`, `architecture.md`), que Claude Code lit directement dans le dépôt.

## Restaurer sur un autre poste

Claude Code lit sa mémoire automatique dans `~/.claude/projects/<dossier du projet>/memory/`, où le nom du dossier
est le chemin du dépôt dont les séparateurs (`:`, `\`, `/`, espaces) sont remplacés par `-`.
Exemple : `D:\Tsaraloha Platforme\TSA` → `D--Tsaraloha-Platforme-TSA`.

Windows (PowerShell, depuis la racine du dépôt) :
```powershell
$dst = "$env:USERPROFILE\.claude\projects\D--Tsaraloha-Platforme-TSA\memory"   # adapter au chemin du dépôt
New-Item -ItemType Directory -Force $dst | Out-Null
Copy-Item .claude\memory\*.md $dst -Exclude README.md
```
Linux (chemin du dépôt `/home/<utilisateur>/Tsaraloha/TSA` → `-home-<utilisateur>-Tsaraloha-TSA`) :
```bash
dst=~/.claude/projects/$(pwd | sed 's#[/: ]#-#g')/memory
mkdir -p "$dst" && cp .claude/memory/*.md "$dst"/ && rm -f "$dst/README.md"
```

## Mettre à jour la copie

Après une session, recopier le dossier de mémoire du poste dans `.claude/memory/` avant le commit (même commande
en sens inverse). L'historique des conversations n'est pas versionné : l'état du travail en cours est dans
`.claude/current-state.md` et `.claude/tasks.md`.

## Dernière session (2026-11-01)

Dépendances installées en une fois (`install_dependencies.py`, `cmake/dependencies.json`), OCCT et 3rdparty attendus
dans `thirdparty/`, installation automatique par CMake sous Linux (`docs/DEPENDENCIES.md` § 6). Restent à faire
(`.claude/tasks.md`) : déplacement réel des dossiers OCCT sur le poste Windows d'origine (refusé : dossiers
verrouillés) et première exécution complète sous Linux.
