# Gmsh (SDK, API C++) : moteur de maillage de TSA (docs/MESH.md). Encapsulé dans src/Mesh/Gmsh/GmshMeshEngine.
#
# Le SDK Windows est compilé avec MinGW (GCC) : avec MSVC, seule l'API C++ « gmsh.h_cwrap » (API C++
# redéfinie sur l'API C) est utilisable. La bibliothèque d'importation MSVC est générée à la configuration
# depuis la table d'export de la DLL (API C « gmsh* » seulement), ce qui permet le chargement différé
# (/DELAYLOAD) : TSA démarre sans la DLL (maillage indisponible, signalé) au lieu de refuser de démarrer.
#
# Licence : Gmsh est distribué sous GNU GPL v2 ou ultérieure (avec exceptions). Voir docs/THIRD_PARTY_LICENSES.md.
#
# Hors Windows : SDK Linux officiel (gmsh-<version>-Linux64-sdk, libgmsh.so compilée avec GCC 6) par son API
# C++ native « gmsh.h » (gmsh.h_cwrap y provoquerait une récursion infinie par interposition ELF) ; liaison
# directe (chemin du SDK dans le RUNPATH de l'arborescence de compilation), aucune copie de la bibliothèque ;
# script de version gmsh-hide-std.map sur les exécutables. Voir docs/LINUX_BUILD.md.
#
# Résultat : cible importée TSA::Gmsh (si disponible), variables TSA_GMSH_DLL, TSA_GMSH_LICENSE_DIR.

if(WIN32)
    set(_gmsh_default_sdk "${TSA_ROOT}/thirdparty/gmsh-4.15.2-Windows64-sdk")
else()
    set(_gmsh_default_sdk "")   # thirdparty/ ne contient que le SDK Windows ; scripts/linux/configure.sh le renseigne
endif()
set(TSA_GMSH_SDK "${_gmsh_default_sdk}" CACHE PATH "SDK Gmsh (gmsh.info, Windows64-sdk ou Linux64-sdk)")
option(TSA_WITH_GMSH "Maillage par Gmsh (SDK)" ON)
option(TSA_PACKAGE_GMSH "Inclure la DLL Gmsh dans le paquet distribuable — licence GPL : voir docs/THIRD_PARTY_LICENSES.md" ON)

set(TSA_GMSH_DLL "")
set(TSA_GMSH_LICENSE_DIR "")
if(TSA_WITH_GMSH AND NOT TARGET TSA::Gmsh)
    file(GLOB _gmsh_dlls "${TSA_GMSH_SDK}/lib/gmsh-*.dll")
    if(WIN32 AND MSVC AND _gmsh_dlls AND EXISTS "${TSA_GMSH_SDK}/include/gmsh.h_cwrap")
        list(GET _gmsh_dlls 0 TSA_GMSH_DLL)
        get_filename_component(_gmsh_dll_name "${TSA_GMSH_DLL}" NAME)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        set(_gmsh_dir "${CMAKE_BINARY_DIR}/gmsh")
        set(_gmsh_def "${_gmsh_dir}/gmsh.def")
        set(_gmsh_lib "${_gmsh_dir}/gmsh_msvc.lib")
        if(Python3_FOUND AND (NOT EXISTS "${_gmsh_lib}" OR "${TSA_GMSH_DLL}" IS_NEWER_THAN "${_gmsh_lib}"))
            file(MAKE_DIRECTORY "${_gmsh_dir}")
            execute_process(COMMAND "${Python3_EXECUTABLE}" "${TSA_ROOT}/tools/dll_to_def.py" "${TSA_GMSH_DLL}" "${_gmsh_def}" gmsh
                            RESULT_VARIABLE _r OUTPUT_QUIET)
            if(_r EQUAL 0)
                execute_process(COMMAND "${CMAKE_AR}" /nologo "/def:${_gmsh_def}" /machine:x64 "/out:${_gmsh_lib}" RESULT_VARIABLE _r OUTPUT_QUIET)
            endif()
            if(NOT _r EQUAL 0)
                file(REMOVE "${_gmsh_lib}")
            endif()
        endif()
        if(EXISTS "${_gmsh_lib}")
            add_library(TSA::Gmsh SHARED IMPORTED GLOBAL)
            set_target_properties(TSA::Gmsh PROPERTIES
                IMPORTED_IMPLIB "${_gmsh_lib}"
                IMPORTED_LOCATION "${TSA_GMSH_DLL}"
                INTERFACE_INCLUDE_DIRECTORIES "${TSA_GMSH_SDK}/include"
                INTERFACE_COMPILE_DEFINITIONS "TSA_HAVE_GMSH=1;TSA_GMSH_DLL_NAME=\"${_gmsh_dll_name}\""
                INTERFACE_LINK_OPTIONS "/DELAYLOAD:${_gmsh_dll_name}"
                INTERFACE_LINK_LIBRARIES "delayimp.lib")
            set(TSA_GMSH_LICENSE_DIR "${TSA_GMSH_SDK}/share/doc/gmsh")
            message(STATUS "Gmsh : ${TSA_GMSH_DLL} (chargement différé)")
        else()
            set(TSA_GMSH_DLL "")
            message(WARNING "Gmsh : bibliothèque d'importation MSVC non générée (Python 3 et lib.exe requis) — maillage indisponible")
        endif()
    elseif(NOT WIN32 AND TSA_GMSH_SDK AND EXISTS "${TSA_GMSH_SDK}/include/gmsh.h_cwrap")
        find_library(TSA_GMSH_LIBRARY NAMES gmsh PATHS "${TSA_GMSH_SDK}/lib" NO_DEFAULT_PATH)
        if(TSA_GMSH_LIBRARY)
            add_library(TSA::Gmsh SHARED IMPORTED GLOBAL)
            set_target_properties(TSA::Gmsh PROPERTIES
                IMPORTED_LOCATION "${TSA_GMSH_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${TSA_GMSH_SDK}/include"
                INTERFACE_COMPILE_DEFINITIONS "TSA_HAVE_GMSH=1"
                # Exécutables : instanciations std non exportées vers libgmsh (GCC 6), voir gmsh-hide-std.map.
                INTERFACE_LINK_OPTIONS "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:LINKER:--version-script=${CMAKE_CURRENT_LIST_DIR}/gmsh-hide-std.map>"
                INTERFACE_LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/gmsh-hide-std.map")
            set(TSA_GMSH_LICENSE_DIR "${TSA_GMSH_SDK}/share/doc/gmsh")
            message(STATUS "Gmsh : ${TSA_GMSH_LIBRARY}")
        else()
            message(STATUS "Gmsh : libgmsh absente de ${TSA_GMSH_SDK}/lib — maillage indisponible")
        endif()
    elseif(NOT WIN32)
        message(STATUS "Gmsh : SDK Linux introuvable (TSA_GMSH_SDK=\"${TSA_GMSH_SDK}\", include/gmsh.h_cwrap attendu) — maillage indisponible")
    elseif(NOT MSVC)
        message(STATUS "Gmsh : intégration prévue pour le SDK Windows avec MSVC uniquement — maillage indisponible")
    else()
        message(STATUS "Gmsh : SDK absent (${TSA_GMSH_SDK}) — maillage indisponible")
    endif()
endif()

# Rapport des dépendances (SetupDependencies.cmake). Version validée : 4.15.2 (SDK officiel gmsh.info).
string(REGEX MATCH "gmsh-([0-9]+\\.[0-9]+\\.[0-9]+)" _gmsh_ver_match "${TSA_GMSH_SDK}")
set(_gmsh_ver "${CMAKE_MATCH_1}")
if(NOT TSA_WITH_GMSH)
    tsa_dependency_record(Gmsh "DÉSACTIVÉE" "" "" "TSA_WITH_GMSH=OFF : maillage indisponible")
elseif(TARGET TSA::Gmsh)
    tsa_validation_status(_gmsh_st "${_gmsh_ver}" "4.15.2")
    tsa_dependency_record(Gmsh "${_gmsh_st}" "${_gmsh_ver}" "${TSA_GMSH_SDK}" "facultative, GPL v2+")
else()
    tsa_dependency_record(Gmsh "ABSENTE" "" "" "facultative : maillage indisponible, suites mesh/surfaces/meshview NON EXÉCUTÉES —"
                          "-DTSA_GMSH_SDK=<gmsh-4.15.2-Windows64-sdk ou Linux64-sdk>")
endif()

# Copie de la DLL Gmsh à côté d'un exécutable (développement ; le paquet la reçoit par PackageTSA.cmake).
function(tsa_copy_gmsh_runtime target)
    if(TARGET TSA::Gmsh AND TSA_GMSH_DLL)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TSA_GMSH_DLL}" "$<TARGET_FILE_DIR:${target}>")
    endif()
endfunction()
