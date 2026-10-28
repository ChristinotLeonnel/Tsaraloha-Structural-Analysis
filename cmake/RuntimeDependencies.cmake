# Résolution des DLL réellement importées (mode script : cmake -P). Commun au déploiement de développement
# (DeployDependencies.cmake) et au paquet distribuable (PackageTSA.cmake). Remplace l'ancienne copie de
# toutes les DLL de 3rdparty-vc14-64 (Qt 5, VTK, Tcl/Tk, Doxygen… inutiles à TSA).
#
# tsa_runtime_search_dirs(<var> <source_dir> [<extra>...])
#   Dossiers où chercher : SDK OpenCASCADE (OCCT_BIN_DIR s'il est défini), bin/ de chaque bibliothèque 3rdparty,
#   dossiers supplémentaires.
#
# tsa_copy_runtime_dependencies(BINARIES <exe/dll>... DESTINATION <dir> SEARCH_DIRS <dir>...
#                                [EXCLUDE_REGEXES <regex>...] [RESULT_VAR <var>] [UNRESOLVED_VAR <var>])
#   Copie dans DESTINATION les dépendances résolues hors Windows (et hors EXCLUDE_REGEXES, ex. Qt laissé à
#   windeployqt). RESULT_VAR : chemins source copiés ; UNRESOLVED_VAR : noms non trouvés.

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

function(tsa_runtime_search_dirs out source_dir)
    # DLL d'OCCT : bin/ de l'OCCT retenu à la configuration (OCCT_BIN_DIR = OpenCASCADE_BINARY_DIR, passé par -D),
    # sinon le SDK téléchargé par défaut dans la base commune.
    if(OCCT_BIN_DIR)
        set(dirs "${OCCT_BIN_DIR}")
    else()
        set(dirs "${source_dir}/opencascade-8.0.1-vc14-64/win64/vc14/bin")
    endif()
    # bin/ et bin/win64/ (ex. openvr_api.dll, importée par TKOpenGl).
    file(GLOB tp LIST_DIRECTORIES true "${source_dir}/3rdparty-vc14-64/*/bin" "${source_dir}/3rdparty-vc14-64/*/bin/win64")
    foreach(d ${tp})
        # Qt 5.11 du SDK OCCT : jamais utilisé par TSA (Qt 6) ; exclu pour éviter toute confusion.
        if(NOT d MATCHES "qt5\\.")
            list(APPEND dirs "${d}")
        endif()
    endforeach()
    list(APPEND dirs ${ARGN})
    set(${out} ${dirs} PARENT_SCOPE)
endfunction()

function(tsa_copy_runtime_dependencies)
    cmake_parse_arguments(R "" "DESTINATION;RESULT_VAR;UNRESOLVED_VAR" "BINARIES;SEARCH_DIRS;EXCLUDE_REGEXES" ${ARGN})
    find_program(TSA_DUMPBIN dumpbin)
    if(NOT TSA_DUMPBIN)
        message(WARNING "dumpbin introuvable (lancer depuis l'invite développeur Visual Studio) : dépendances non résolues")
        return()
    endif()
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "dumpbin")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${TSA_DUMPBIN}")
    set(exes "")
    set(libs "")
    foreach(b ${R_BINARIES})
        if(b MATCHES "\\.[eE][xX][eE]$")
            list(APPEND exes "${b}")
        else()
            list(APPEND libs "${b}")
        endif()
    endforeach()
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES ${exes}
        LIBRARIES ${libs}
        RESOLVED_DEPENDENCIES_VAR resolved
        UNRESOLVED_DEPENDENCIES_VAR unresolved
        CONFLICTING_DEPENDENCIES_PREFIX conflict
        DIRECTORIES ${R_SEARCH_DIRS}
        PRE_EXCLUDE_REGEXES "^api-ms-.*" "^ext-ms-.*"
        POST_EXCLUDE_REGEXES "^[A-Za-z]:/[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/.*")
    # Même DLL trouvée à plusieurs endroits (ex. copie déjà déployée à côté de l'exécutable et original du SDK,
    # selon le dossier de la DLL qui l'importe) : acceptée seulement si toutes les copies sont identiques.
    foreach(name ${conflict_FILENAMES})
        list(GET conflict_${name} 0 first)
        file(SHA256 "${first}" ref_hash)
        foreach(other ${conflict_${name}})
            file(SHA256 "${other}" other_hash)
            if(NOT other_hash STREQUAL ref_hash)
                message(FATAL_ERROR "DLL ${name} : copies différentes (${conflict_${name}})")
            endif()
        endforeach()
        list(APPEND resolved "${first}")
    endforeach()
    set(copied "")
    foreach(dep ${resolved})
        get_filename_component(name "${dep}" NAME)
        set(skip FALSE)
        foreach(rx ${R_EXCLUDE_REGEXES})
            if(name MATCHES "${rx}")
                set(skip TRUE)
            endif()
        endforeach()
        get_filename_component(dep_dir "${dep}" DIRECTORY)
        file(REAL_PATH "${dep_dir}" dep_dir)
        file(REAL_PATH "${R_DESTINATION}" dest_real)
        if(NOT skip AND NOT dep_dir STREQUAL dest_real)
            file(COPY "${dep}" DESTINATION "${R_DESTINATION}")
            list(APPEND copied "${dep}")
        endif()
    endforeach()
    if(R_RESULT_VAR)
        set(${R_RESULT_VAR} ${copied} PARENT_SCOPE)
    endif()
    if(R_UNRESOLVED_VAR)
        set(${R_UNRESOLVED_VAR} ${unresolved} PARENT_SCOPE)
    endif()
endfunction()
