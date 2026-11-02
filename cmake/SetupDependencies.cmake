# =============================================================================
# cmake/SetupDependencies.cmake
# Gestion centrale des dependances externes (docs/DEPENDENCIES.md), partagee par TSA et TSALab :
# - versions prises en charge et validees (Qt, OCCT), rapport tsa-dependencies.txt ;
# - tsa_find_qt() : choix automatique d'une installation Qt compatible (avant find_package(Qt6)) ;
# - tsa_validate_dependencies() : controles apres find_package (chaine de compilation, melange, versions) ;
# - dependances absentes installees dans thirdparty/ (TSA_AUTO_DOWNLOAD_DEPS) : Windows, OpenCASCADE 8.0.1, ses
#   3rdparty (FreeType, TBB, FreeImage…) et OpenSees ; Linux, toutes (Qt, OCCT, Gmsh, Eigen, OpenSees, IfcOpenShell)
#   par install_dependencies.py (racine), appele depuis tsa_find_qt().
# Versions, adresses et empreintes : cmake/dependencies.json (source unique, lue aussi par install_dependencies.py).
# Gmsh et IfcOpenShell : cmake/Gmsh.cmake et cmake/IfcOpenShell.cmake (meme rapport).
# =============================================================================

cmake_minimum_required(VERSION 3.20)

option(TSA_AUTO_DOWNLOAD_DEPS "Installer automatiquement dans thirdparty/ les dependances absentes (Windows : OCCT, 3rdparty, OpenSees ; Linux : toutes, par install_dependencies.py)" ON)
set(TSA_OCCT_VERSION "8.0.1" CACHE STRING "Version d'OpenCASCADE a utiliser")
# OpenSees absent de thirdparty/OpenSees et du PATH : paquets conda-forge épinglés (TSA_OPENSEES_CONDA_PACKAGES), ou
# archive officielle OpenSees3.8.0-x64.exe.zip déjà obtenue (le serveur officiel ne la publie plus depuis 2026-10, W-3),
# acceptée seulement avec l'empreinte du manifeste winget OpenSees.OpenSees 3.8.0 (comme
# OpenSeesManager::officialArchiveSha256).
set(TSA_OPENSEES_SHA256 "5E37A5A95C645E31C7700CE6EC5B08C178BAEDC83241F7CBE10EF10CF6A4FB41" CACHE STRING "Empreinte SHA-256 attendue de l'archive OpenSees")
set(TSA_OPENSEES_ARCHIVE "" CACHE FILEPATH "Archive locale OpenSees3.8.0-x64.exe.zip (empreinte vérifiée)")

# SDK partagés (OCCT, 3rdparty…) : <racine de la base commune TSA>/thirdparty, aussi utilisée par TSALab.
get_filename_component(TSA_DEPS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(TSA_THIRDPARTY_DIR "${TSA_DEPS_ROOT}/thirdparty")

# Manifeste des dépendances téléchargées (versions, adresses, SHA-256), partagé avec install_dependencies.py.
file(READ "${CMAKE_CURRENT_LIST_DIR}/dependencies.json" TSA_DEPS_MANIFEST)
# tsa_manifest_get(<var> <clé>...) : valeur du manifeste (chaîne vide si la clé n'existe pas).
function(tsa_manifest_get out)
    string(JSON _v ERROR_VARIABLE _err GET "${TSA_DEPS_MANIFEST}" ${ARGN})
    if(_err)
        set(_v "")
    endif()
    set(${out} "${_v}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Versions prises en charge (justification : docs/DEPENDENCIES.md). « Validée » = compilée ET testée ; une version
# seulement acceptée est signalée comme non validée, jamais présentée comme fonctionnelle.
# -----------------------------------------------------------------------------
# Qt : aucun appel C++ postérieur à 6.2 relevé (recherche des API 6.3-6.10), plancher conservé ; les traductions
# exigent qt_add_lrelease(TS_FILES … QM_OUTPUT_DIRECTORY …) : forme sans cible depuis 6.7, QM_OUTPUT_DIRECTORY depuis 6.9.
set(TSA_QT_MIN_VERSION "6.2")
set(TSA_QT_TRANSLATIONS_MIN_VERSION "6.9")
set(TSA_QT_VALIDATED_VERSIONS "6.11.2;6.12.0")
# OCCT : code écrit pour l'API 8 (Xw_Window / Aspect_Drawable, dépréciations 8.0 traitées), 7.6 refusé (LINUX_BUILD.md).
set(TSA_OCCT_MIN_VERSION "8.0")
set(TSA_OCCT_VALIDATED_VERSIONS "8.0.1")

# -----------------------------------------------------------------------------
# Rapport des dépendances : chaque détection enregistre ce qu'elle a retenu ; le rapport est affiché en fin de
# configuration et écrit dans <build>/tsa-dependencies.txt (cible tsa-dependency-report).
# Statuts : VALIDÉE, NON VALIDÉE (acceptée, jamais testée), ABSENTE, DÉSACTIVÉE, INCOMPLÈTE.
# -----------------------------------------------------------------------------
function(tsa_dependency_record name status version location)
    get_property(_names GLOBAL PROPERTY TSA_DEPENDENCY_NAMES)
    if(NOT name IN_LIST _names)
        set_property(GLOBAL APPEND PROPERTY TSA_DEPENDENCY_NAMES "${name}")
    endif()
    string(REPLACE ";" " " _note "${ARGN}")
    set_property(GLOBAL PROPERTY "TSA_DEPENDENCY_${name}" "${status}|${version}|${location}|${_note}")
endfunction()

# Statut d'une version trouvée : VALIDÉE si elle figure dans la liste des versions testées.
function(tsa_validation_status out version validated)
    if(version IN_LIST validated)
        set(${out} "VALIDÉE" PARENT_SCOPE)
    else()
        set(${out} "NON VALIDÉE" PARENT_SCOPE)
    endif()
endfunction()

function(tsa_write_dependency_report)
    set(_lines
        "Dépendances de ${PROJECT_NAME} — ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}"
        "Compilateur : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})"
        "CMake : ${CMAKE_VERSION}, générateur ${CMAKE_GENERATOR}, configuration ${CMAKE_BUILD_TYPE}")
    set(_paths "")
    get_property(_names GLOBAL PROPERTY TSA_DEPENDENCY_NAMES)
    foreach(_n ${_names})
        get_property(_v GLOBAL PROPERTY "TSA_DEPENDENCY_${_n}")
        string(REPLACE "|" ";" _v "${_v}")
        list(GET _v 0 _status)
        list(GET _v 1 _version)
        list(GET _v 2 _location)
        list(GET _v 3 _note)
        set(_line "${_n} : ${_status}")
        if(_version)
            string(APPEND _line " ${_version}")
        endif()
        if(_location)
            string(APPEND _line " — ${_location}")
            string(APPEND _paths "${_location}\n")
        endif()
        if(_note)
            string(APPEND _line " (${_note})")
        endif()
        list(APPEND _lines "${_line}")
    endforeach()
    message(STATUS "==================== Dépendances retenues ====================")
    set(_text "")
    foreach(_l ${_lines})
        message(STATUS "${_l}")
        string(APPEND _text "${_l}\n")
    endforeach()
    message(STATUS "Rapport : ${CMAKE_BINARY_DIR}/tsa-dependencies.txt (cible tsa-dependency-report)")
    message(STATUS "==============================================================")
    file(WRITE "${CMAKE_BINARY_DIR}/tsa-dependencies.txt" "${_text}")
    file(WRITE "${CMAKE_BINARY_DIR}/tsa-dependencies-paths.txt" "${_paths}")   # chemins retenus (tsa-dependency-report)
endfunction()

# -----------------------------------------------------------------------------
# Qt 6
# -----------------------------------------------------------------------------
# Version et chaîne de compilation d'une installation Qt (<préfixe>/lib/cmake/Qt6) : MSVC (Qt6Core.lib), MinGW
# (libQt6Core.a), ELF (libQt6Core.so), sinon vide (Android, WebAssembly… : jamais retenue).
function(tsa_qt_kit_info prefix out_version out_toolchain)
    set(_version "")
    set(_toolchain "")
    # Qt ≥ 6.x : la version est dans Qt6ConfigVersionImpl.cmake (Qt6ConfigVersion.cmake n'est qu'un aiguillage).
    foreach(_f Qt6ConfigVersionImpl.cmake Qt6ConfigVersion.cmake)
        if(NOT _version AND EXISTS "${prefix}/lib/cmake/Qt6/${_f}")
            file(STRINGS "${prefix}/lib/cmake/Qt6/${_f}" _l REGEX "set\\(PACKAGE_VERSION \"[0-9.]+\"\\)")
            if(_l MATCHES "\"([0-9.]+)\"")
                set(_version "${CMAKE_MATCH_1}")
            endif()
        endif()
    endforeach()
    file(GLOB _so "${prefix}/lib/libQt6Core.so*")
    if(prefix MATCHES "(android|wasm|ios|arm64)[^/]*$")
        # bibliothèques d'une autre plateforme ou architecture
    elseif(EXISTS "${prefix}/lib/Qt6Core.lib" OR EXISTS "${prefix}/lib/Qt6Cored.lib")
        set(_toolchain "MSVC")
    elseif(EXISTS "${prefix}/lib/libQt6Core.a" OR EXISTS "${prefix}/lib/libQt6Core.dll.a")
        set(_toolchain "MinGW")
    elseif(_so)
        set(_toolchain "ELF")
    endif()
    set(${out_version} "${_version}" PARENT_SCOPE)
    set(${out_toolchain} "${_toolchain}" PARENT_SCOPE)
endfunction()

# Chaîne de compilation exigée des bibliothèques C++ par le compilateur de ce projet.
function(tsa_expected_toolchain out)
    if(MSVC)
        set(${out} "MSVC" PARENT_SCOPE)
    elseif(WIN32)
        set(${out} "MinGW" PARENT_SCOPE)
    else()
        set(${out} "ELF" PARENT_SCOPE)
    endif()
endfunction()

# À appeler AVANT find_package(Qt6). Ne fait rien si Qt6_DIR désigne une installation valide ou si CMAKE_PREFIX_PATH
# en contient une (mécanismes CMake standard, prioritaires). Sinon, parmi les installations trouvées (variables
# d'environnement Qt6_DIR, QTDIR, TSA_QT_DIR, puis <racine>/6.*/<kit> pour chaque racine de TSA_QT_SEARCH_ROOTS),
# retient la version la plus récente ≥ TSA_QT_MIN_VERSION dont la chaîne de compilation est celle du compilateur.
# Un Qt6_DIR resté en cache vers une installation disparue (ex. 6.11.2 désinstallé) est remplacé, et les autres
# Qt6*_DIR du cache qui pointent hors de la Qt retenue sont effacés (aucun mélange de deux installations).
function(tsa_find_qt)
    # Première fonction appelée par chaque produit (TSA, TSALab) : sous Linux, les dépendances absentes sont
    # installées ici, avant toute recherche de Qt ou d'OCCT.
    tsa_provision_unix_dependencies()

    if(WIN32)
        set(_roots "C:/Qt" "$ENV{SystemDrive}/Qt" "$ENV{USERPROFILE}/Qt")
    else()
        set(_roots "$ENV{HOME}/Qt")
    endif()
    set(TSA_QT_SEARCH_ROOTS "${_roots}" CACHE STRING "Dossiers d'installation de Qt explorés (<racine>/6.*/<kit>)")
    # Qt installée par install_dependencies.py, toujours explorée (y compris avec un TSA_QT_SEARCH_ROOTS en cache).
    set(_all_roots "${TSA_THIRDPARTY_DIR}/Qt" ${TSA_QT_SEARCH_ROOTS})

    if(Qt6_DIR AND EXISTS "${Qt6_DIR}/Qt6Config.cmake")
        return()
    endif()
    _tsa_prefix_paths(_prefixes)
    foreach(_p ${_prefixes})
        if(EXISTS "${_p}/lib/cmake/Qt6/Qt6Config.cmake")
            return()
        endif()
    endforeach()

    set(_candidates "")
    foreach(_e Qt6_DIR QTDIR TSA_QT_DIR)
        set(_v "$ENV{${_e}}")
        if(_v AND EXISTS "${_v}/Qt6Config.cmake")
            get_filename_component(_v "${_v}/../../.." ABSOLUTE)
        endif()
        if(_v AND EXISTS "${_v}/lib/cmake/Qt6/Qt6Config.cmake")
            list(APPEND _candidates "${_v}")
        endif()
    endforeach()
    foreach(_r ${_all_roots})
        file(GLOB _kits LIST_DIRECTORIES true "${_r}/6.*/*")
        foreach(_k ${_kits})
            if(EXISTS "${_k}/lib/cmake/Qt6/Qt6Config.cmake")
                list(APPEND _candidates "${_k}")
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES _candidates)

    tsa_expected_toolchain(_want)
    set(_best "")
    set(_best_version "")
    set(_seen "")
    foreach(_c ${_candidates})
        tsa_qt_kit_info("${_c}" _ver _tc)
        if(NOT _tc)
            list(APPEND _seen "${_c} (${_ver}, autre plateforme)")
        elseif(NOT _tc STREQUAL _want)
            list(APPEND _seen "${_c} (${_ver}, ${_tc} au lieu de ${_want} : chaîne de compilation incompatible)")
        elseif(NOT _ver OR _ver VERSION_LESS TSA_QT_MIN_VERSION)
            list(APPEND _seen "${_c} (${_ver} < ${TSA_QT_MIN_VERSION})")
        else()
            list(APPEND _seen "${_c} (${_ver}, ${_tc})")
            if(NOT _best OR _ver VERSION_GREATER _best_version)
                set(_best "${_c}")
                set(_best_version "${_ver}")
            endif()
        endif()
    endforeach()
    string(REPLACE ";" "\n  " _seen_text "${_seen}")

    if(NOT _best)
        if(WIN32)
            message(FATAL_ERROR
                "Qt 6 (≥ ${TSA_QT_MIN_VERSION}, ${_want}) introuvable.\n"
                "Installations examinées :\n  ${_seen_text}\n"
                "Solutions : « python install_dependencies.py --only qt » (Qt dans thirdparty/Qt), installer Qt 6 "
                "pour ${_want} (kit msvc20xx_64 avec Visual Studio) dans C:/Qt, ou indiquer "
                "-DQt6_DIR=<Qt>/<version>/<kit>/lib/cmake/Qt6, -DCMAKE_PREFIX_PATH=<Qt>/<version>/<kit>, "
                "-DTSA_QT_SEARCH_ROOTS=<dossier>.")
        endif()
        return()   # ailleurs : Qt du système, cherché par find_package
    endif()

    set(Qt6_DIR "${_best}/lib/cmake/Qt6" CACHE PATH "Configuration CMake de Qt 6" FORCE)
    message(STATUS "Qt ${_best_version} détecté automatiquement : ${_best}")
    if(_seen)
        message(STATUS "  installations examinées :\n  ${_seen_text}")
    endif()
    string(TOLOWER "${_best}" _best_lc)
    get_cmake_property(_cache CACHE_VARIABLES)
    foreach(_var ${_cache})
        if(_var MATCHES "^Qt6.+_DIR$")
            string(TOLOWER "${${_var}}" _val_lc)
            string(FIND "${_val_lc}" "${_best_lc}" _pos)
            if(NOT _pos EQUAL 0)
                unset(${_var} CACHE)
            endif()
        endif()
    endforeach()
endfunction()

# Contrôles APRÈS find_package(Qt6) et find_package(OpenCASCADE) (appelé par TSAProduct.cmake, donc pour TSA et
# TSALab) : chaîne de compilation, architecture, mélange d'installations, plages de versions. Toute incompatibilité
# arrête la configuration avec la cause et la solution.
function(tsa_validate_dependencies)
    tsa_expected_toolchain(_want)
    if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Windows : seule la compilation x64 est prise en charge (SDK OCCT win64, Qt *_64). "
                            "Utiliser l'invite « x64 Native Tools » ou -A x64.")
    endif()

    # --- Qt ---
    get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    tsa_qt_kit_info("${_qt_prefix}" _kit_version _qt_tc)
    if(_qt_tc AND NOT _qt_tc STREQUAL _want)
        message(FATAL_ERROR "Qt ${Qt6_VERSION} (${_qt_prefix}) est compilé pour ${_qt_tc}, le compilateur est ${_want} "
                            "(${CMAKE_CXX_COMPILER_ID}) : bibliothèques C++ incompatibles (ABI). Choisir le kit Qt de ce "
                            "compilateur (-DQt6_DIR=…/lib/cmake/Qt6) dans un dossier de compilation neuf.")
    endif()
    # Kit msvcAAAA : compilé avec Visual Studio AAAA, toolset au moins aussi récent requis (compatibilité binaire MSVC).
    if(MSVC AND _qt_prefix MATCHES "msvc(20[0-9][0-9])")
        set(_kit_year "${CMAKE_MATCH_1}")
        set(_min_msvc 0)
        if(_kit_year STREQUAL "2019")
            set(_min_msvc 1920)
        elseif(_kit_year STREQUAL "2022")
            set(_min_msvc 1930)
        elseif(_kit_year VERSION_GREATER 2022)
            set(_min_msvc 1950)
        endif()
        if(MSVC_VERSION LESS _min_msvc)
            message(FATAL_ERROR "Qt ${Qt6_VERSION} (kit msvc${_kit_year}) exige Visual Studio ${_kit_year} ou plus récent "
                                "(MSVC_VERSION ≥ ${_min_msvc}) ; compilateur actuel : ${MSVC_VERSION}.")
        endif()
    endif()
    string(TOLOWER "${_qt_prefix}" _qt_prefix_lc)
    set(_mixed "")
    set(_components "")
    foreach(_c Core Gui Widgets Svg Network Test LinguistTools)
        if(TARGET Qt6::${_c} OR (_c STREQUAL "LinguistTools" AND TARGET Qt6::lrelease))
            list(APPEND _components ${_c})
        endif()
        if(Qt6${_c}_DIR AND EXISTS "${Qt6${_c}_DIR}")
            get_filename_component(_d "${Qt6${_c}_DIR}" ABSOLUTE)
            string(TOLOWER "${_d}" _d)
            string(FIND "${_d}" "${_qt_prefix_lc}" _pos)
            if(NOT _pos EQUAL 0)
                list(APPEND _mixed "Qt6${_c}_DIR=${Qt6${_c}_DIR}")
            endif()
        endif()
    endforeach()
    if(_mixed)
        string(REPLACE ";" "\n  " _mixed "${_mixed}")
        message(FATAL_ERROR "Mélange d'installations Qt : Qt6_DIR désigne ${_qt_prefix}, mais\n  ${_mixed}\n"
                            "Supprimer ces entrées du cache (cmake -U \"Qt6*_DIR\" …) ou utiliser un dossier de compilation neuf.")
    endif()
    tsa_validation_status(_st "${Qt6_VERSION}" "${TSA_QT_VALIDATED_VERSIONS}")
    string(REPLACE ";" " " _components "${_components}")
    set(_note "${_qt_tc}" "modules : ${_components}")
    if(_st STREQUAL "NON VALIDÉE")
        list(APPEND _note "versions validées : ${TSA_QT_VALIDATED_VERSIONS}")
    endif()
    if(Qt6_VERSION VERSION_LESS TSA_QT_TRANSLATIONS_MIN_VERSION)
        list(APPEND _note "traductions désactivées (< ${TSA_QT_TRANSLATIONS_MIN_VERSION})")
    endif()
    string(REPLACE ";" ", " _note "${_note}")
    tsa_dependency_record(Qt "${_st}" "${Qt6_VERSION}" "${_qt_prefix}" "${_note}")

    # --- OpenCASCADE ---
    if(OpenCASCADE_VERSION VERSION_LESS TSA_OCCT_MIN_VERSION)
        message(FATAL_ERROR "OpenCASCADE ${OpenCASCADE_VERSION} (${OpenCASCADE_DIR}) : TSA exige OpenCASCADE "
                            "≥ ${TSA_OCCT_MIN_VERSION} (API 8). Indiquer -DOpenCASCADE_DIR=<OCCT 8>/cmake "
                            "(ou lib/cmake/opencascade) ; sous Windows, le SDK ${TSA_OCCT_VERSION} est téléchargé s'il est absent.")
    endif()
    if(WIN32 AND DEFINED OpenCASCADE_COMPILER)
        if(MSVC AND NOT OpenCASCADE_COMPILER MATCHES "^vc")
            message(FATAL_ERROR "OpenCASCADE (${OpenCASCADE_DIR}) compilé avec ${OpenCASCADE_COMPILER}, le compilateur est MSVC.")
        elseif(NOT MSVC AND OpenCASCADE_COMPILER MATCHES "^vc")
            message(FATAL_ERROR "Le SDK OpenCASCADE ${OpenCASCADE_VERSION} (${OpenCASCADE_COMPILER}) est compilé avec MSVC : "
                                "il ne peut pas être lié par ${CMAKE_CXX_COMPILER_ID}. Compiler TSA avec Visual Studio (MSVC).")
        endif()
    endif()
    tsa_validation_status(_st "${OpenCASCADE_VERSION}" "${TSA_OCCT_VALIDATED_VERSIONS}")
    set(_note "")
    if(OpenCASCADE_COMPILER)
        set(_note "${OpenCASCADE_COMPILER}")
    endif()
    tsa_dependency_record(OpenCASCADE "${_st}" "${OpenCASCADE_VERSION}" "${OpenCASCADE_DIR}" "${_note}")

    # --- OpenSees : exécutable externe facultatif, cherché comme OpenSeesManager (sans le lancer) ---
    # thirdparty/OpenSees peut contenir les deux plateformes (copie de travail partagée) : seuls les noms de celle-ci.
    set(_ops "")
    if(WIN32)
        set(_ops_names bin/OpenSees.exe bin/gOpenSees.exe OpenSees.exe gOpenSees.exe)
    else()
        set(_ops_names bin/OpenSees OpenSees)
    endif()
    foreach(_exe ${_ops_names})
        if(NOT _ops AND EXISTS "${TSA_THIRDPARTY_DIR}/OpenSees/${_exe}")
            set(_ops "${TSA_THIRDPARTY_DIR}/OpenSees/${_exe}")
        endif()
    endforeach()
    if(NOT _ops)
        find_program(TSA_OPENSEES_IN_PATH NAMES OpenSees gOpenSees)
        if(TSA_OPENSEES_IN_PATH)
            set(_ops "${TSA_OPENSEES_IN_PATH}")
        endif()
    endif()
    if(_ops)
        tsa_dependency_record(OpenSees "TROUVÉE" "" "${_ops}" "version contrôlée au lancement par OpenSeesManager")
    else()
        tsa_dependency_record(OpenSees "ABSENTE" "" "" "facultative : calculs OpenSees indisponibles, suites qui en dépendent NON EXÉCUTÉES —"
                              "python install_dependencies.py --only opensees, OpenSees dans le PATH, ou (Windows) "
                              "-DTSA_OPENSEES_ARCHIVE=<zip officiel>")
    endif()

    # --- Outils de déploiement Windows ---
    if(WIN32)
        find_program(TSA_DUMPBIN_FOR_REPORT dumpbin)
        if(TSA_DUMPBIN_FOR_REPORT)
            tsa_dependency_record(dumpbin "TROUVÉE" "" "${TSA_DUMPBIN_FOR_REPORT}" "résolution des DLL déployées")
        else()
            tsa_dependency_record(dumpbin "ABSENTE" "" "" "lancer CMake depuis l'invite x64 Native Tools : DLL OCCT non copiées sinon")
        endif()
    endif()
endfunction()

# CMAKE_PREFIX_PATH (variable et environnement ; séparateur « : » sous Unix).
function(_tsa_prefix_paths out)
    set(_prefixes ${CMAKE_PREFIX_PATH})
    set(_env_prefixes "$ENV{CMAKE_PREFIX_PATH}")
    if(NOT WIN32)
        string(REPLACE ":" ";" _env_prefixes "${_env_prefixes}")
    endif()
    list(APPEND _prefixes ${_env_prefixes})
    set(${out} ${_prefixes} PARENT_SCOPE)
endfunction()

# Préfixe d'une dépendance Unix contenant <marker> : thirdparty/<dossier>, variable d'environnement <env_var>, puis la
# version la plus récente de ~/opt/<motif> (mêmes emplacements que scripts/linux/common.sh et install_dependencies.py).
function(_tsa_unix_find out marker thirdparty_name env_var opt_glob)
    set(_c "")
    if(thirdparty_name)
        list(APPEND _c "${TSA_THIRDPARTY_DIR}/${thirdparty_name}")
    endif()
    if(env_var AND DEFINED ENV{${env_var}})
        list(APPEND _c "$ENV{${env_var}}")
    endif()
    if(opt_glob)
        file(GLOB _g LIST_DIRECTORIES true "$ENV{HOME}/opt/${opt_glob}")
        list(SORT _g COMPARE NATURAL ORDER DESCENDING)
        list(APPEND _c ${_g})
    endif()
    foreach(_p ${_c})
        if(EXISTS "${_p}/${marker}")
            set(${out} "${_p}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} "" PARENT_SCOPE)
endfunction()

# Linux (et autres Unix) : chaque dépendance absente de ses emplacements connus est installée dans thirdparty/ par
# install_dependencies.py (téléchargements vérifiés par SHA-256 ; OCCT et IfcOpenShell compilés depuis les sources).
# Les paquets système (compilateur, en-têtes d'OCCT, bibliothèques X11) exigent sudo : jamais installés depuis CMake,
# seulement signalés (« python3 install_dependencies.py » les installe). Désactivation : -DTSA_AUTO_DOWNLOAD_DEPS=OFF ;
# une dépendance facultative se désactive aussi par -DTSA_WITH_GMSH=OFF ou -DTSA_WITH_IFCOPENSHELL=OFF.
function(tsa_provision_unix_dependencies)
    if(WIN32 OR NOT TSA_AUTO_DOWNLOAD_DEPS)
        return()
    endif()
    _tsa_prefix_paths(_prefixes)
    set(_need "")

    # Qt 6 (kit ELF ≥ minimum) : Qt6_DIR, CMAKE_PREFIX_PATH, <racine>/6.*/<kit>. Le Qt du système n'est pas retenu
    # (souvent < 6.9 : traductions désactivées, version non validée).
    set(_have FALSE)
    if(Qt6_DIR AND EXISTS "${Qt6_DIR}/Qt6Config.cmake")
        set(_have TRUE)
    endif()
    foreach(_p ${_prefixes})
        if(EXISTS "${_p}/lib/cmake/Qt6/Qt6Config.cmake")
            set(_have TRUE)
        endif()
    endforeach()
    if(NOT _have)
        file(GLOB _kits LIST_DIRECTORIES true "${TSA_THIRDPARTY_DIR}/Qt/6.*/*" "$ENV{HOME}/Qt/6.*/*")
        foreach(_r ${TSA_QT_SEARCH_ROOTS})
            file(GLOB _more LIST_DIRECTORIES true "${_r}/6.*/*")
            list(APPEND _kits ${_more})
        endforeach()
        foreach(_k ${_kits})
            tsa_qt_kit_info("${_k}" _ver _tc)
            if(_tc STREQUAL "ELF" AND _ver AND NOT _ver VERSION_LESS TSA_QT_MIN_VERSION)
                set(_have TRUE)
            endif()
        endforeach()
    endif()
    if(NOT _have)
        list(APPEND _need qt)
    endif()

    # OpenCASCADE 8
    if(NOT (OpenCASCADE_DIR AND EXISTS "${OpenCASCADE_DIR}/OpenCASCADEConfig.cmake"))
        set(_have FALSE)
        foreach(_p ${_prefixes})
            if(EXISTS "${_p}/lib/cmake/opencascade/OpenCASCADEConfig.cmake")
                set(_have TRUE)
            endif()
        endforeach()
        tsa_manifest_get(_dir occt linux install_dir)
        _tsa_unix_find(_found "lib/cmake/opencascade/OpenCASCADEConfig.cmake" "${_dir}" TSA_OCCT_DIR "occt-8.*")
        if(NOT _have AND NOT _found)
            list(APPEND _need occt)
        endif()
    endif()

    # Gmsh (facultatif)
    if(NOT (DEFINED TSA_WITH_GMSH AND NOT TSA_WITH_GMSH) AND NOT (TSA_GMSH_SDK AND EXISTS "${TSA_GMSH_SDK}/include/gmsh.h"))
        tsa_manifest_get(_dir gmsh linux dir)
        _tsa_unix_find(_found "include/gmsh.h" "${_dir}" TSA_GMSH_SDK "gmsh-*-Linux64-sdk")
        if(NOT _found)
            list(APPEND _need gmsh)
        endif()
    endif()

    # Eigen (en-têtes du moteur fem3d, si le dépôt voisin TSA-3D-FEM-Engine, ancien nom 3DDesplacement, est présent)
    tsa_manifest_get(_dir eigen dir)
    if((EXISTS "${TSA_DEPS_ROOT}/../TSA-3D-FEM-Engine/CMakeLists.txt" OR EXISTS "${TSA_DEPS_ROOT}/../3DDesplacement/CMakeLists.txt")
       AND NOT EXISTS "${TSA_THIRDPARTY_DIR}/${_dir}/include/eigen3/Eigen/Core")
        list(APPEND _need eigen)
    endif()

    # OpenSees (facultatif, exécutable externe)
    if(NOT EXISTS "${TSA_THIRDPARTY_DIR}/OpenSees/bin/OpenSees")
        find_program(TSA_OPENSEES_IN_PATH NAMES OpenSees gOpenSees)
        if(NOT TSA_OPENSEES_IN_PATH)
            list(APPEND _need opensees)
        endif()
    endif()

    # IfcOpenShell (facultatif ; compilé contre l'OCCT retenu, avec DataExchange)
    if(NOT (DEFINED TSA_WITH_IFCOPENSHELL AND NOT TSA_WITH_IFCOPENSHELL)
       AND NOT (TSA_IFCOPENSHELL_ROOT AND EXISTS "${TSA_IFCOPENSHELL_ROOT}/lib/cmake/IfcOpenShell/IfcOpenShellConfig.cmake"))
        tsa_manifest_get(_dir ifcopenshell linux_dir)
        _tsa_unix_find(_found "lib/cmake/IfcOpenShell/IfcOpenShellConfig.cmake" "${_dir}" TSA_IFCOPENSHELL_ROOT "ifcopenshell-*")
        if(NOT _found)
            list(APPEND _need ifcopenshell)
        endif()
    endif()

    if(NOT _need)
        return()
    endif()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(NOT Python3_FOUND)
        message(FATAL_ERROR "Dépendances absentes (${_need}) et Python 3 introuvable : installer python3, ou installer "
                            "les dépendances à la main (docs/LINUX_BUILD.md), ou -DTSA_AUTO_DOWNLOAD_DEPS=OFF.")
    endif()
    string(REPLACE ";" "," _only "${_need}")
    string(REPLACE ";" ", " _need_text "${_need}")
    message(STATUS "==================================================================")
    message(STATUS "[${PROJECT_NAME}] Dépendances absentes : ${_need_text}")
    message(STATUS "[${PROJECT_NAME}] Installation dans ${TSA_THIRDPARTY_DIR} par install_dependencies.py")
    message(STATUS "[${PROJECT_NAME}] (OCCT et IfcOpenShell sont compilés : de 15 min à plus d'une heure la première fois ;")
    message(STATUS "[${PROJECT_NAME}]  désactiver : -DTSA_AUTO_DOWNLOAD_DEPS=OFF)")
    message(STATUS "==================================================================")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${TSA_DEPS_ROOT}/install_dependencies.py" --from-cmake
                            --cmake "${CMAKE_COMMAND}" --only "${_only}"
                    RESULT_VARIABLE _r)
    if(NOT _r EQUAL 0)
        message(FATAL_ERROR "install_dependencies.py n'a pas pu installer une dépendance obligatoire (détails ci-dessus). "
                            "Paquets système manquants : lancer « python3 install_dependencies.py » (sudo apt-get).")
    endif()
endfunction()

# Ancienne disposition (avant 2026-11) : SDK OCCT et 3rdparty à la racine du dépôt. Déplacés dans thirdparty/
# (même volume : simple renommage), comme le fait install_dependencies.py ; le chemin d'OCCT inscrit dans la
# construction Windows d'IfcOpenShell (IfcOpenShellTargets.cmake, chemin absolu) est mis à jour.
function(tsa_migrate_legacy_layout)
    tsa_manifest_get(_sdk occt windows sdk_dir)
    tsa_manifest_get(_tp occt windows thirdparty_dir)
    set(_dirs "${_sdk}" "${_tp}" "opencascade-${TSA_OCCT_VERSION}-vc14-64")
    list(REMOVE_DUPLICATES _dirs)
    foreach(_d ${_dirs})
        if(NOT _d OR NOT IS_DIRECTORY "${TSA_DEPS_ROOT}/${_d}")
            continue()
        endif()
        if(EXISTS "${TSA_THIRDPARTY_DIR}/${_d}")
            message(WARNING "${TSA_DEPS_ROOT}/${_d} n'est plus utilisé (remplacé par thirdparty/${_d}) : le supprimer à la main.")
            continue()
        endif()
        message(STATUS "[${PROJECT_NAME}] Déplacement de ${_d} dans thirdparty/ (nouvelle disposition)")
        file(MAKE_DIRECTORY "${TSA_THIRDPARTY_DIR}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E rename "${TSA_DEPS_ROOT}/${_d}" "${TSA_THIRDPARTY_DIR}/${_d}"
                        RESULT_VARIABLE _r)
        if(NOT _r EQUAL 0)
            # Dossier verrouillé (fenêtre de l'Explorateur, indexation, antivirus…) : l'ancien emplacement reste
            # utilisé pour cette configuration (setup_external_dependencies, RuntimeDependencies.cmake).
            message(WARNING "Déplacement impossible de ${TSA_DEPS_ROOT}/${_d} vers thirdparty/ (dossier utilisé par "
                            "un autre programme) : ancien emplacement conservé pour cette fois. Fermer ce qui "
                            "l'utilise, puis relancer CMake ou « python install_dependencies.py ».")
        endif()
    endforeach()
    tsa_manifest_get(_ifc ifcopenshell windows_dir)
    set(_targets "${TSA_THIRDPARTY_DIR}/${_ifc}/lib/cmake/IfcOpenShell/IfcOpenShellTargets.cmake")
    if(_sdk AND EXISTS "${_targets}" AND EXISTS "${TSA_THIRDPARTY_DIR}/${_sdk}/cmake/OpenCASCADEConfig.cmake")
        file(READ "${_targets}" _text)
        string(REPLACE "${TSA_DEPS_ROOT}/${_sdk}" "${TSA_THIRDPARTY_DIR}/${_sdk}" _fixed "${_text}")
        if(NOT _fixed STREQUAL _text)
            file(WRITE "${_targets}" "${_fixed}")
            message(STATUS "[${PROJECT_NAME}] Chemin d'OCCT mis à jour dans ${_targets}")
        endif()
    endif()
endfunction()

# Téléchargement d'un SDK Windows d'OCCT : empreinte du manifeste exigée quand la version est celle du manifeste.
function(_tsa_download_occt_archive url dest key)
    tsa_manifest_get(_mver occt version)
    set(_hash_args "")
    if(TSA_OCCT_VERSION STREQUAL _mver)
        tsa_manifest_get(_sha occt windows ${key} sha256)
        set(_hash_args EXPECTED_HASH "SHA256=${_sha}")
    else()
        message(WARNING "OpenCASCADE ${TSA_OCCT_VERSION} ≠ ${_mver} (cmake/dependencies.json) : archive non vérifiée.")
    endif()
    message(STATUS "[${PROJECT_NAME}] Telechargement de ${url} ...")
    file(DOWNLOAD "${url}" "${dest}" SHOW_PROGRESS STATUS _st LOG _log TLS_VERIFY ON ${_hash_args})
    list(GET _st 0 _code)
    list(GET _st 1 _msg)
    if(NOT _code EQUAL 0)
        file(REMOVE "${dest}")
        message(FATAL_ERROR "Echec du telechargement de ${url} : ${_msg}\n${_log}")
    endif()
endfunction()

function(setup_external_dependencies)
    # Linux : OCCT installé par install_dependencies.py (tsa_find_qt) ou trouvé dans CMAKE_PREFIX_PATH, TSA_OCCT_DIR,
    # ~/opt/occt-8.* (docs/LINUX_BUILD.md). Un CMAKE_PREFIX_PATH qui contient un OCCT l'emporte (find_package).
    if(NOT WIN32)
        if(OpenCASCADE_DIR AND EXISTS "${OpenCASCADE_DIR}/OpenCASCADEConfig.cmake")
            return()
        endif()
        _tsa_prefix_paths(_prefixes)
        foreach(_p ${_prefixes})
            if(EXISTS "${_p}/lib/cmake/opencascade/OpenCASCADEConfig.cmake")
                message(STATUS "OpenCASCADE : CMAKE_PREFIX_PATH (${_p})")
                return()
            endif()
        endforeach()
        tsa_manifest_get(_dir occt linux install_dir)
        _tsa_unix_find(_found "lib/cmake/opencascade/OpenCASCADEConfig.cmake" "${_dir}" TSA_OCCT_DIR "occt-8.*")
        if(_found)
            set(OpenCASCADE_DIR "${_found}/lib/cmake/opencascade" CACHE PATH "Path to OpenCASCADE CMake config" FORCE)
            message(STATUS "OpenCASCADE : ${_found}")
        else()
            message(STATUS "OpenCASCADE : recherche par find_package (OpenCASCADE_DIR ou CMAKE_PREFIX_PATH)")
        endif()
        return()
    endif()

    # Windows : SDK officiel MSVC (binaires) et ses 3rdparty dans thirdparty/.
    tsa_migrate_legacy_layout()
    set(OCCT_FOLDER_NAME "opencascade-${TSA_OCCT_VERSION}-vc14-64")
    set(LOCAL_OCCT_DIR "${TSA_THIRDPARTY_DIR}/${OCCT_FOLDER_NAME}")
    set(LOCAL_3RDPARTY_DIR "${TSA_THIRDPARTY_DIR}/3rdparty-vc14-64")
    # Déplacement impossible (dossier verrouillé) : ancien emplacement à la racine, plutôt qu'un nouveau téléchargement.
    if(NOT EXISTS "${LOCAL_OCCT_DIR}/cmake/OpenCASCADEConfig.cmake"
       AND EXISTS "${TSA_DEPS_ROOT}/${OCCT_FOLDER_NAME}/cmake/OpenCASCADEConfig.cmake")
        set(LOCAL_OCCT_DIR "${TSA_DEPS_ROOT}/${OCCT_FOLDER_NAME}")
    endif()
    if(NOT EXISTS "${LOCAL_3RDPARTY_DIR}" AND IS_DIRECTORY "${TSA_DEPS_ROOT}/3rdparty-vc14-64")
        set(LOCAL_3RDPARTY_DIR "${TSA_DEPS_ROOT}/3rdparty-vc14-64")
    endif()
    set(LOCAL_OCCT_CMAKE "${LOCAL_OCCT_DIR}/cmake")

    # -------------------------------------------------------------------------
    # 1. Verification de la presence d'OpenCASCADE
    # -------------------------------------------------------------------------
    set(NEED_OCCT_DOWNLOAD FALSE)

    if(OpenCASCADE_DIR AND EXISTS "${OpenCASCADE_DIR}/OpenCASCADEConfig.cmake")
        message(STATUS "OpenCASCADE configure via OpenCASCADE_DIR: ${OpenCASCADE_DIR}")
    elseif(EXISTS "${LOCAL_OCCT_CMAKE}/OpenCASCADEConfig.cmake")
        message(STATUS "OpenCASCADE detecte localement: ${LOCAL_OCCT_CMAKE}")
        set(OpenCASCADE_DIR "${LOCAL_OCCT_CMAKE}" CACHE PATH "Path to OpenCASCADE CMake config" FORCE)
    else()
        set(NEED_OCCT_DOWNLOAD TRUE)
    endif()

    # -------------------------------------------------------------------------
    # 2. Telechargement d'OpenCASCADE si absent (empreinte SHA-256 du manifeste)
    # -------------------------------------------------------------------------
    if(NEED_OCCT_DOWNLOAD)
        if(NOT TSA_AUTO_DOWNLOAD_DEPS)
            message(FATAL_ERROR
                "OpenCASCADE ${TSA_OCCT_VERSION} introuvable.\n"
                "Veuillez definir -DOpenCASCADE_DIR=\"<chemin>/cmake\", lancer « python install_dependencies.py » ou "
                "activer le telechargement automatique avec -DTSA_AUTO_DOWNLOAD_DEPS=ON."
            )
        endif()

        message(STATUS "==================================================================")
        message(STATUS "[${PROJECT_NAME}] OpenCASCADE ${TSA_OCCT_VERSION} introuvable localement.")
        message(STATUS "[${PROJECT_NAME}] Telechargement automatique depuis GitHub Releases...")
        message(STATUS "==================================================================")

        set(OCCT_URL "https://github.com/Open-Cascade-SAS/OCCT/releases/download/V${TSA_OCCT_VERSION}/opencascade-release-no-pch.zip")
        set(DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps_download")
        set(OUTER_ZIP "${DOWNLOAD_DIR}/opencascade-release-no-pch.zip")
        set(EXTRACT_STAGE "${DOWNLOAD_DIR}/occt_stage")

        file(MAKE_DIRECTORY "${DOWNLOAD_DIR}")
        file(MAKE_DIRECTORY "${EXTRACT_STAGE}")
        file(MAKE_DIRECTORY "${TSA_THIRDPARTY_DIR}")
        _tsa_download_occt_archive("${OCCT_URL}" "${OUTER_ZIP}" sdk)

        message(STATUS "[${PROJECT_NAME}] Extraction de l'archive principale...")
        file(ARCHIVE_EXTRACT INPUT "${OUTER_ZIP}" DESTINATION "${EXTRACT_STAGE}")

        # L'archive officielle GitHub contient l'archive imbriquee opencascade-8.0.1-vc14-64.zip
        file(GLOB INNER_OCCT_ZIPS "${EXTRACT_STAGE}/*.zip")
        if(INNER_OCCT_ZIPS)
            list(GET INNER_OCCT_ZIPS 0 INNER_ZIP)
            message(STATUS "[${PROJECT_NAME}] Decompression du package SDK vers ${TSA_THIRDPARTY_DIR} ...")
            file(ARCHIVE_EXTRACT INPUT "${INNER_ZIP}" DESTINATION "${TSA_THIRDPARTY_DIR}")
        else()
            execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${EXTRACT_STAGE}" "${LOCAL_OCCT_DIR}")
        endif()

        # Nettoyage du dossier temporaire de telechargement
        file(REMOVE_RECURSE "${DOWNLOAD_DIR}")

        if(EXISTS "${LOCAL_OCCT_CMAKE}/OpenCASCADEConfig.cmake")
            message(STATUS "[${PROJECT_NAME}] OpenCASCADE ${TSA_OCCT_VERSION} installe avec succes dans ${LOCAL_OCCT_DIR} !")
            set(OpenCASCADE_DIR "${LOCAL_OCCT_CMAKE}" CACHE PATH "Path to OpenCASCADE CMake config" FORCE)
        else()
            message(FATAL_ERROR "Echec de l'installation d'OpenCASCADE : ${LOCAL_OCCT_CMAKE}/OpenCASCADEConfig.cmake est introuvable apres extraction.")
        endif()
    endif()

    # -------------------------------------------------------------------------
    # 3. Verification et telechargement des dependances 3rdparty (empreinte SHA-256 du manifeste)
    # -------------------------------------------------------------------------
    if(NOT EXISTS "${LOCAL_3RDPARTY_DIR}")
        if(NOT TSA_AUTO_DOWNLOAD_DEPS)
            message(WARNING
                "Dossier 3rdparty (${LOCAL_3RDPARTY_DIR}) introuvable.\n"
                "Les DLLs d'execution (FreeType, TBB, etc.) risquent de manquer lors du lancement de l'application."
            )
        else()
            message(STATUS "==================================================================")
            message(STATUS "[${PROJECT_NAME}] Dependances 3rdparty introuvables.")
            message(STATUS "[${PROJECT_NAME}] Telechargement automatique de 3rdparty-vc14-64...")
            message(STATUS "==================================================================")

            set(TP_URL "https://github.com/Open-Cascade-SAS/OCCT/releases/download/V${TSA_OCCT_VERSION}/3rdparty-vc14-64.zip")
            set(DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps_download")
            set(OUTER_TP_ZIP "${DOWNLOAD_DIR}/3rdparty-vc14-64-outer.zip")
            set(EXTRACT_STAGE "${DOWNLOAD_DIR}/3rdparty_stage")
            set(FINAL_STAGE "${DOWNLOAD_DIR}/3rdparty_final")

            file(MAKE_DIRECTORY "${DOWNLOAD_DIR}")
            file(MAKE_DIRECTORY "${EXTRACT_STAGE}")
            file(MAKE_DIRECTORY "${FINAL_STAGE}")
            _tsa_download_occt_archive("${TP_URL}" "${OUTER_TP_ZIP}" thirdparty)

            message(STATUS "[${PROJECT_NAME}] Extraction de l'archive 3rdparty...")
            file(ARCHIVE_EXTRACT INPUT "${OUTER_TP_ZIP}" DESTINATION "${EXTRACT_STAGE}")

            file(GLOB INNER_TP_ZIPS "${EXTRACT_STAGE}/*.zip")
            if(INNER_TP_ZIPS)
                list(GET INNER_TP_ZIPS 0 INNER_ZIP)
                message(STATUS "[${PROJECT_NAME}] Decompression des bibliotheques 3rdparty...")
                file(ARCHIVE_EXTRACT INPUT "${INNER_ZIP}" DESTINATION "${FINAL_STAGE}")
            else()
                execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${EXTRACT_STAGE}" "${FINAL_STAGE}")
            endif()

            # Verifier si l'archive extraite a un dossier racine 3rdparty-vc14-64
            if(EXISTS "${FINAL_STAGE}/3rdparty-vc14-64")
                execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${FINAL_STAGE}/3rdparty-vc14-64" "${LOCAL_3RDPARTY_DIR}")
            else()
                execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${FINAL_STAGE}" "${LOCAL_3RDPARTY_DIR}")
            endif()

            file(REMOVE_RECURSE "${DOWNLOAD_DIR}")
            message(STATUS "[${PROJECT_NAME}] Dependances 3rdparty installees avec succes dans ${LOCAL_3RDPARTY_DIR} !")
        endif()
    else()
        message(STATUS "Dependances 3rdparty detectees localement: ${LOCAL_3RDPARTY_DIR}")
    endif()

    # -------------------------------------------------------------------------
    # 4. OpenSees (executable externe, facultatif) si thirdparty/OpenSees n'en contient aucun
    # -------------------------------------------------------------------------
    if(TSA_AUTO_DOWNLOAD_DEPS)
        tsa_download_opensees("${TSA_THIRDPARTY_DIR}/OpenSees" "${CMAKE_BINARY_DIR}/_deps_download")
    endif()

endfunction()

# Installe bin/ et lib/ de l'archive officielle d'OpenSees dans <dest>, sauf si un exécutable y est déjà (mêmes noms et
# emplacements qu'OpenSeesManager::findExecutableInternal). Jamais bloquant : OpenSees est facultatif (TSA se construit
# sans lui ; les suites qui en dépendent sont alors NON EXÉCUTÉES), un échec n'émet qu'un avertissement.
function(tsa_download_opensees dest work_dir)
    foreach(_exe bin/OpenSees.exe bin/gOpenSees.exe OpenSees.exe gOpenSees.exe)
        if(EXISTS "${dest}/${_exe}")
            message(STATUS "OpenSees detecte localement: ${dest}/${_exe}")
            return()
        endif()
    endforeach()

    # Déjà dans le PATH : trouvé à l'exécution par OpenSeesManager, rien à installer.
    find_program(TSA_OPENSEES_IN_PATH NAMES OpenSees gOpenSees)
    if(TSA_OPENSEES_IN_PATH AND NOT TSA_OPENSEES_ARCHIVE)
        message(STATUS "OpenSees detecte dans le PATH: ${TSA_OPENSEES_IN_PATH}")
        return()
    endif()

    set(_stage "${work_dir}/opensees_stage")
    file(REMOVE_RECURSE "${_stage}")
    file(MAKE_DIRECTORY "${_stage}")
    set(_problem "")
    if(NOT TSA_OPENSEES_ARCHIVE)
        tsa_install_opensees_condaforge("${dest}" "${_stage}" _problem)
        file(REMOVE_RECURSE "${_stage}")
        if(_problem)
            message(WARNING "OpenSees non installe : ${_problem}.\n"
                            "Calculs OpenSees indisponibles (TSA se construit et fonctionne sans eux). Solutions : placer "
                            "OpenSees.exe et lib/tcl8.6 dans ${dest}, ou OpenSees dans le PATH ; ou "
                            "-DTSA_OPENSEES_ARCHIVE=<OpenSees3.8.0-x64.exe.zip officiel> ; -DTSA_AUTO_DOWNLOAD_DEPS=OFF pour "
                            "ne plus essayer (docs/DEPENDENCIES.md §5).")
        else()
            message(STATUS "[${PROJECT_NAME}] OpenSees 3.8.0 (conda-forge) installe dans ${dest} (empreintes verifiees).")
        endif()
        return()
    endif()

    # Archive officielle déjà obtenue (le serveur officiel ne la publie plus, W-3) : même contrôle d'empreinte.
    message(STATUS "[${PROJECT_NAME}] OpenSees : installation de l'archive locale ${TSA_OPENSEES_ARCHIVE} ...")
    set(_zip "${work_dir}/opensees.zip")
    if(EXISTS "${TSA_OPENSEES_ARCHIVE}")
        configure_file("${TSA_OPENSEES_ARCHIVE}" "${_zip}" COPYONLY)
        set(_code 0)
    else()
        set(_code 1)
        set(_msg "archive introuvable : ${TSA_OPENSEES_ARCHIVE}")
    endif()
    if(NOT _code EQUAL 0)
        set(_problem "${_msg}")
    else()
        file(SHA256 "${_zip}" _hash)
        string(TOUPPER "${_hash}" _hash)
        string(TOUPPER "${TSA_OPENSEES_SHA256}" _expected)
        if(NOT _hash STREQUAL _expected)
            set(_problem "empreinte SHA-256 inattendue (${_hash}, attendu ${_expected}) : archive rejetee")
        else()
            file(ARCHIVE_EXTRACT INPUT "${_zip}" DESTINATION "${_stage}")
            # Archive officielle : OpenSees3.8.0/bin (OpenSees.exe, DLL) et OpenSees3.8.0/lib (tcl8.6).
            file(GLOB _bins LIST_DIRECTORIES true "${_stage}/*/bin" "${_stage}/bin")
            if(NOT _bins)
                set(_problem "dossier bin absent de l'archive")
            else()
                list(GET _bins 0 _bin)
                get_filename_component(_root "${_bin}" DIRECTORY)
                file(COPY "${_root}/bin" DESTINATION "${dest}")
                if(EXISTS "${_root}/lib")
                    file(COPY "${_root}/lib" DESTINATION "${dest}")
                endif()
                if(NOT EXISTS "${dest}/bin/OpenSees.exe" AND NOT EXISTS "${dest}/bin/gOpenSees.exe")
                    set(_problem "aucun executable OpenSees dans l'archive extraite")
                endif()
            endif()
        endif()
    endif()
    file(REMOVE_RECURSE "${_stage}")
    file(REMOVE "${_zip}")

    if(_problem)
        message(WARNING "OpenSees non installe depuis ${TSA_OPENSEES_ARCHIVE} : ${_problem}.")
    else()
        message(STATUS "[${PROJECT_NAME}] OpenSees installe dans ${dest} (empreinte verifiee).")
    endif()
endfunction()

# OpenSees 3.8.0 pour Windows depuis conda-forge (W-3) : compilé depuis l'étiquette officielle v3.8.0 par la recette
# publique github.com/conda-forge/opensees-feedstock. Paquets épinglés, empreintes SHA-256 publiées par anaconda.org
# (une archive d'empreinte différente n'est jamais extraite). Seuls OpenSees.exe, les DLL qu'il importe (fermeture
# dumpbin /dependents ET exports transférés, établie les 2026-10-28 et 29 ; chargement et commande de contrôle vérifiés)
# et la bibliothèque Tcl 8.6 de tcl86t.dll sont installés : <dest>/bin et <dest>/lib,
# disposition attendue par OpenSeesManager (Tcl dans <bin>/../lib/tcl8.6). Runtime MSVC : celui de Windows / du paquet.
# Entrées construites depuis cmake/dependencies.json (opensees.windows.packages, source unique partagée avec
# install_dependencies.py) : « fichier|sha256|fichiers de Library/bin à installer (séparés par des virgules) ».
# libblas.dll / liblapack.dll ne contiennent aucun code : leurs exports sont TRANSFÉRÉS à openblas.dll (invisible pour
# dumpbin /dependents ; sans lui, OpenSees.exe ne se charge pas : 0xC0000135, constaté le 2026-10-29).
set(TSA_OPENSEES_CONDA_PACKAGES "")
string(JSON _tsa_n LENGTH "${TSA_DEPS_MANIFEST}" opensees windows packages)
math(EXPR _tsa_last "${_tsa_n} - 1")
foreach(_tsa_i RANGE ${_tsa_last})
    string(JSON _tsa_file GET "${TSA_DEPS_MANIFEST}" opensees windows packages ${_tsa_i} file)
    string(JSON _tsa_sha GET "${TSA_DEPS_MANIFEST}" opensees windows packages ${_tsa_i} sha256)
    string(JSON _tsa_nf LENGTH "${TSA_DEPS_MANIFEST}" opensees windows packages ${_tsa_i} files)
    math(EXPR _tsa_lf "${_tsa_nf} - 1")
    set(_tsa_files "")
    foreach(_tsa_j RANGE ${_tsa_lf})
        string(JSON _tsa_f GET "${TSA_DEPS_MANIFEST}" opensees windows packages ${_tsa_i} files ${_tsa_j})
        list(APPEND _tsa_files "${_tsa_f}")
    endforeach()
    string(REPLACE ";" "," _tsa_files "${_tsa_files}")
    list(APPEND TSA_OPENSEES_CONDA_PACKAGES "${_tsa_file}|${_tsa_sha}|${_tsa_files}")
endforeach()

# Télécharge, vérifie et assemble dans <stage>/OpenSees, puis copie dans <dest> seulement si tout a réussi.
function(tsa_install_opensees_condaforge dest stage out_problem)
    set(_root "${stage}/OpenSees")
    file(MAKE_DIRECTORY "${_root}/bin" "${_root}/lib")
    foreach(_entry ${TSA_OPENSEES_CONDA_PACKAGES})
        string(REPLACE "|" ";" _f "${_entry}")
        list(GET _f 0 _name)
        list(GET _f 1 _sha)
        list(GET _f 2 _files)
        string(REPLACE "," ";" _files "${_files}")
        message(STATUS "[${PROJECT_NAME}] OpenSees : conda-forge/win-64/${_name}.conda ...")
        set(_pkg "${stage}/${_name}.conda")
        file(DOWNLOAD "https://conda.anaconda.org/conda-forge/win-64/${_name}.conda" "${_pkg}"
             STATUS _status TLS_VERIFY ON TIMEOUT 600 INACTIVITY_TIMEOUT 60)
        list(GET _status 0 _code)
        if(NOT _code EQUAL 0)
            list(GET _status 1 _msg)
            set(${out_problem} "${_name} : ${_msg}" PARENT_SCOPE)
            return()
        endif()
        # Contrôle explicite (EXPECTED_HASH ferait échouer toute la configuration : OpenSees est facultatif).
        file(SHA256 "${_pkg}" _hash)
        if(NOT _hash STREQUAL _sha)
            file(REMOVE "${_pkg}")
            set(${out_problem} "${_name} : empreinte SHA-256 ${_hash}, attendue ${_sha} : paquet rejeté" PARENT_SCOPE)
            return()
        endif()
        # .conda = zip contenant pkg-<nom>.tar.zst
        set(_x "${stage}/${_name}")
        file(ARCHIVE_EXTRACT INPUT "${_pkg}" DESTINATION "${_x}")
        file(ARCHIVE_EXTRACT INPUT "${_x}/pkg-${_name}.tar.zst" DESTINATION "${_x}")
        foreach(_file ${_files})
            if(NOT EXISTS "${_x}/Library/bin/${_file}")
                set(${out_problem} "${_file} absent de ${_name}" PARENT_SCOPE)
                return()
            endif()
            file(COPY "${_x}/Library/bin/${_file}" DESTINATION "${_root}/bin")
        endforeach()
        if(EXISTS "${_x}/Library/lib/tcl8.6/init.tcl")
            # Bibliothèque de scripts Tcl et modules (.tm : msgcat, utilisé par clock, http, platform…).
            file(COPY "${_x}/Library/lib/tcl8.6" "${_x}/Library/lib/tcl8" DESTINATION "${_root}/lib")
        endif()
        file(REMOVE_RECURSE "${_x}")
        file(REMOVE "${_pkg}")
    endforeach()
    if(NOT EXISTS "${_root}/lib/tcl8.6/init.tcl")
        set(${out_problem} "bibliothèque Tcl 8.6 absente" PARENT_SCOPE)
        return()
    endif()
    file(WRITE "${_root}/SOURCE.txt"
         "OpenSees 3.8.0 (licence UC, docs/THIRD_PARTY_LICENSES.md), compilé par conda-forge depuis "
         "https://github.com/OpenSees/OpenSees/archive/refs/tags/v3.8.0.tar.gz (recette conda-forge/opensees-feedstock).\n"
         "Paquets (cmake/dependencies.json, opensees.windows) :\n")
    foreach(_entry ${TSA_OPENSEES_CONDA_PACKAGES})
        file(APPEND "${_root}/SOURCE.txt" "  ${_entry}\n")
    endforeach()
    file(COPY "${_root}/bin" "${_root}/lib" "${_root}/SOURCE.txt" DESTINATION "${dest}")
    set(${out_problem} "" PARENT_SCOPE)
endfunction()
