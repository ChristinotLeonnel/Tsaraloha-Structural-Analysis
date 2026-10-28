cmake_minimum_required(VERSION 3.20)

# Diagnostic des dépendances (cible tsa-dependency-report, docs/DEPENDENCIES.md). Lecture seule : n'installe, ne
# télécharge et ne modifie rien. Affiche le rapport écrit à la configuration (<build>/tsa-dependencies.txt), puis
# vérifie que chaque chemin retenu existe encore (installation déplacée ou supprimée depuis la configuration) et
# cherche OpenSees comme l'application le fera au lancement.
#
# Paramètres (-D) : REPORT (tsa-dependencies.txt), SOURCE_DIR (racine du dépôt TSA)

if(NOT EXISTS "${REPORT}")
    message(FATAL_ERROR "Rapport absent (${REPORT}) : configurer d'abord le projet (cmake --preset …).")
endif()

file(STRINGS "${REPORT}" lines ENCODING UTF-8)
foreach(line ${lines})
    message(STATUS "${line}")
endforeach()
# Chemins retenus, un par ligne (écrits à côté du rapport) : chacun doit toujours exister.
string(REPLACE "tsa-dependencies.txt" "tsa-dependencies-paths.txt" paths_file "${REPORT}")
set(missing "")
if(EXISTS "${paths_file}")
    file(STRINGS "${paths_file}" paths ENCODING UTF-8)
    foreach(path ${paths})
        if(NOT EXISTS "${path}")
            list(APPEND missing "${path}")
        endif()
    endforeach()
endif()

message(STATUS "--- État actuel ---")
foreach(p ${missing})
    message(WARNING "Chemin retenu à la configuration, introuvable maintenant : ${p}\n"
                    "Reconfigurer (cmake --preset …) pour une nouvelle détection.")
endforeach()
if(NOT missing)
    message(STATUS "Tous les chemins retenus existent.")
endif()

set(ops "")
foreach(exe bin/OpenSees.exe bin/gOpenSees.exe OpenSees.exe gOpenSees.exe bin/OpenSees OpenSees)
    if(NOT ops AND EXISTS "${SOURCE_DIR}/thirdparty/OpenSees/${exe}")
        set(ops "${SOURCE_DIR}/thirdparty/OpenSees/${exe}")
    endif()
endforeach()
if(NOT ops)
    find_program(ops NAMES OpenSees gOpenSees NO_CACHE)
endif()
if(ops)
    message(STATUS "OpenSees (maintenant) : ${ops}")
else()
    message(STATUS "OpenSees (maintenant) : ABSENT — calculs OpenSees indisponibles ; voir docs/DEPENDENCIES.md (W-3)")
endif()
