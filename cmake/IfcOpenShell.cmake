# IfcOpenShell 0.9.0post1 (LGPL-3.0) : lecture des schémas IFC2X3 / IFC4 / IFC4X3_ADD2 et noyau géométrique
# OpenCASCADE, pour l'import IFC (src/BIM/IFC/IfcOpenShellImport). Compilé contre l'OCCT de TSA par
# scripts/build_ifcopenshell.ps1 et installé dans thirdparty/ avec Boost (statique) et Eigen (en-têtes).
#
# Sans la bibliothèque, TSA se compile et l'import IFC se limite au lecteur STEP interne (signalé à l'utilisateur).
# Liaison dynamique, DLL remplaçables : conditions de la LGPL v3, voir docs/THIRD_PARTY_LICENSES.md.
#
# Résultat : TSA_HAVE_IFCOPENSHELL, TSA_IFCOPENSHELL_DLLS ; fonctions tsa_link_ifcopenshell(),
# tsa_copy_ifcopenshell_runtime().

# thirdparty/ifcopenshell-0.9.0post1 contient la construction MSVC : hors Windows, indiquer une construction
# native par -DTSA_IFCOPENSHELL_ROOT=<chemin> (sinon import IFC par le lecteur STEP interne) : celle de
# scripts/linux/build-ifcopenshell.sh installe Boost et Eigen dans ce même préfixe, et les modules restent dans
# <préfixe>/lib, à côté de libifcparse qui les cherche dans son propre dossier (RUNPATH, aucune copie).
if(WIN32)
    set(_tsa_ifcopenshell_default "${TSA_ROOT}/thirdparty/ifcopenshell-0.9.0post1")
else()
    set(_tsa_ifcopenshell_default "")
endif()
set(TSA_IFCOPENSHELL_ROOT "${_tsa_ifcopenshell_default}" CACHE PATH "IfcOpenShell installé (scripts/build_ifcopenshell.ps1)")
option(TSA_WITH_IFCOPENSHELL "Import IFC complet par IfcOpenShell (LGPL-3.0)" ON)
# Versions publiées : jamais de paquet sans l'import IFC complet (configuration en échec si la bibliothèque manque).
option(TSA_REQUIRE_IFCOPENSHELL "Échec de la configuration si IfcOpenShell est absent ou inutilisable" OFF)

set(TSA_HAVE_IFCOPENSHELL OFF)
set(TSA_IFCOPENSHELL_DLLS "")
if(TSA_WITH_IFCOPENSHELL AND EXISTS "${TSA_IFCOPENSHELL_ROOT}/lib/cmake/IfcOpenShell/IfcOpenShellConfig.cmake")
    list(APPEND CMAKE_PREFIX_PATH "${TSA_IFCOPENSHELL_ROOT}")
    if(WIN32)   # ailleurs, thirdparty/boost-1.92.0 (bibliothèques MSVC) ne doit pas être trouvé
        list(APPEND CMAKE_PREFIX_PATH "${TSA_ROOT}/thirdparty/boost-1.92.0" "${TSA_ROOT}/thirdparty/eigen-3.4.1")
    endif()
    set(Boost_USE_STATIC_LIBS ON)
    find_package(IfcOpenShell CONFIG QUIET)
    if(TARGET IfcOpenShell::IfcGeom AND TARGET IfcOpenShell::IfcParse)
        set(TSA_HAVE_IFCOPENSHELL ON)
        # Toutes les DLL : bibliothèques liées et modules chargés à l'exécution (schémas, noyau OpenCASCADE,
        # correspondances de géométrie), cherchés dans le dossier de la bibliothèque qui les charge.
        file(GLOB TSA_IFCOPENSHELL_DLLS "${TSA_IFCOPENSHELL_ROOT}/bin/*.dll")
        if(WIN32)
            list(LENGTH TSA_IFCOPENSHELL_DLLS _n)
        else()
            file(GLOB _so "${TSA_IFCOPENSHELL_ROOT}/lib/*ifcopenshell*.so")
            list(LENGTH _so _n)
        endif()
        message(STATUS "IfcOpenShell : ${TSA_IFCOPENSHELL_ROOT} (${_n} bibliothèques et modules)")
    else()
        message(WARNING "IfcOpenShell présent mais inutilisable (Boost ou Eigen absents de thirdparty/ ?) — "
                        "import IFC par le lecteur STEP interne. Relancer scripts/build_ifcopenshell.ps1.")
    endif()
else()
    message(STATUS "IfcOpenShell absent (${TSA_IFCOPENSHELL_ROOT}) — import IFC par le lecteur STEP interne "
                   "(capacités réduites) ; voir scripts/build_ifcopenshell.ps1")
endif()
if(TSA_REQUIRE_IFCOPENSHELL AND NOT TSA_HAVE_IFCOPENSHELL)
    message(FATAL_ERROR "TSA_REQUIRE_IFCOPENSHELL : IfcOpenShell introuvable ou inutilisable dans ${TSA_IFCOPENSHELL_ROOT} "
                        "(lancer scripts/build_ifcopenshell.ps1)")
endif()

# Pont vers IfcOpenShell : IfcOpenShellImport.cpp, seul fichier qui inclut ses en-têtes, est retiré de la bibliothèque
# OBJECT du modèle et compilé dans une bibliothèque statique qui reçoit seule les définitions et chemins d'IfcOpenShell,
# Boost et Eigen. Liés au modèle, ils entreraient dans son en-tête précompilé, réutilisé par toutes les autres cibles
# (avertissement C4651 sur chaque fichier). Les utilisateurs du modèle lient le pont et IfcOpenShell (LINK_ONLY).
# Arguments après <model> : ceux de tsa_apply_common_settings (dossier du produit, dossiers propres).
set(TSA_IFCOPENSHELL_BRIDGE_SOURCE "${TSA_ROOT}/src/BIM/IFC/IfcOpenShellImport.cpp")

function(tsa_link_ifcopenshell model)
    if(NOT TSA_HAVE_IFCOPENSHELL)
        return()   # IfcOpenShellImport.cpp reste dans le modèle, compilé en réponses « absent »
    endif()
    get_target_property(_sources ${model} SOURCES)
    list(REMOVE_ITEM _sources "${TSA_IFCOPENSHELL_BRIDGE_SOURCE}")
    set_property(TARGET ${model} PROPERTY SOURCES ${_sources})

    set(bridge ${model}_IfcOpenShell)
    add_library(${bridge} STATIC "${TSA_IFCOPENSHELL_BRIDGE_SOURCE}")
    tsa_apply_common_settings(${bridge} ${ARGN})
    # En-têtes de TSA comme le modèle (Qt, cœur scientifique ; OCCT par tsa_apply_common_settings), puis IfcOpenShell.
    target_link_libraries(${bridge} PRIVATE tsalab_science Qt6::Core Qt6::Gui IfcOpenShell::IfcGeom IfcOpenShell::IfcParse)
    # Comme la compilation d'IfcOpenShell : pas d'auto-liaison de Boost (bibliothèques fournies par CMake).
    # IFOPSH_SAFE_INSTANCE n'est pas défini, comme pour la bibliothèque 0.9.0post1 (disposition des classes
    # d'express.h identique de part et d'autre).
    target_compile_definitions(${bridge} PRIVATE TSA_HAVE_IFCOPENSHELL=1 BOOST_ALL_NO_LIB)
    target_link_libraries(${model} PRIVATE ${bridge})
endfunction()

# Copie des DLL d'IfcOpenShell à côté d'un exécutable (développement ; le paquet les reçoit par PackageTSA.cmake).
function(tsa_copy_ifcopenshell_runtime target)
    if(TSA_HAVE_IFCOPENSHELL AND TSA_IFCOPENSHELL_DLLS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TSA_IFCOPENSHELL_DLLS} "$<TARGET_FILE_DIR:${target}>")
    endif()
endfunction()
