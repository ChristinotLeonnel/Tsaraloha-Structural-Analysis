cmake_minimum_required(VERSION 3.20)

# Paquet distribuable autonome (cible package_<produit>, docs/DEPLOYMENT.md). Construit un dossier PROPRE
# (jamais le dossier de compilation) :
#
#   <DIST_DIR>/
#     <PRODUCT>.exe, outils (.exe), DLL résolues (OCCT, 3rdparty), Qt (windeployqt), runtime MSVC
#     platforms/ styles/ imageformats/ iconengines/ tls/ …  plugins Qt
#     translations/qtbase_fr.qm
#     resources/occt/        ressources OpenCASCADE (Shaders, Textures, messages, unités)
#     engines/OpenSees/      bin/OpenSees.exe + libiomp5md.dll + runtime ; lib/tcl8.6
#     Extensions/            bibliothèques TSALib livrées
#     modules/               modules livrés (module.json)
#     plugins/               plugins binaires (vide par défaut)
#     licenses/              THIRD_PARTY_LICENSES.md et licences fournies
#     docs/                  shortcut.txt (référence), TSA3D, SDK
#     MANIFEST.json          fichiers distribués : taille, SHA-256, catégorie, origine
#
# Paramètres (-D) : BUILD_DIR SOURCE_DIR DIST_DIR PRODUCT VERSION CONFIG WINDEPLOYQT_EXECUTABLE
#                   VCREDIST_DIR (VCToolsRedistDir) PACKAGE_OPENSEES (ON/OFF) EXTRA_EXECUTABLES (liste ;)

foreach(v BUILD_DIR SOURCE_DIR DIST_DIR PRODUCT)
    if(NOT ${v})
        message(FATAL_ERROR "${v} non défini")
    endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/RuntimeDependencies.cmake")

message(STATUS "=== Paquet ${PRODUCT} ${VERSION} (${CONFIG}) → ${DIST_DIR} ===")
if(CONFIG STREQUAL "Debug")
    message(WARNING "Paquet Debug : le runtime MSVC de débogage n'est PAS redistribuable ; utiliser ninja-release.")
endif()
file(REMOVE_RECURSE "${DIST_DIR}")
file(MAKE_DIRECTORY "${DIST_DIR}")

# --- 1. Exécutables du produit --------------------------------------------------------------------
set(binaries "${BUILD_DIR}/${PRODUCT}.exe")
string(REPLACE "|" ";" EXTRA_EXECUTABLES "${EXTRA_EXECUTABLES}")
foreach(extra ${EXTRA_EXECUTABLES})
    if(EXISTS "${BUILD_DIR}/${extra}")
        list(APPEND binaries "${BUILD_DIR}/${extra}")
    endif()
endforeach()
set(dist_binaries "")
foreach(b ${binaries})
    file(COPY "${b}" DESTINATION "${DIST_DIR}")
    get_filename_component(n "${b}" NAME)
    list(APPEND dist_binaries "${DIST_DIR}/${n}")
endforeach()

# --- 2. Qt (windeployqt sur l'exécutable copié) ----------------------------------------------------
if(WINDEPLOYQT_EXECUTABLE AND EXISTS "${WINDEPLOYQT_EXECUTABLE}")
    if(CONFIG STREQUAL "Debug")
        set(qtmode --debug)
    else()
        set(qtmode --release)
    endif()
    execute_process(
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${qtmode} --no-translations --no-compiler-runtime --no-opengl-sw
                --no-system-d3d-compiler --no-system-dxc-compiler --no-quick-import ${dist_binaries}
        RESULT_VARIABLE res OUTPUT_QUIET)
    if(NOT res EQUAL 0)
        message(FATAL_ERROR "windeployqt a échoué (${res})")
    endif()
    get_filename_component(qt_bin "${WINDEPLOYQT_EXECUTABLE}" DIRECTORY)
    if(EXISTS "${qt_bin}/../translations/qtbase_fr.qm")
        file(COPY "${qt_bin}/../translations/qtbase_fr.qm" DESTINATION "${DIST_DIR}/translations")
    endif()
else()
    message(FATAL_ERROR "windeployqt introuvable")
endif()

# --- 3. Autres DLL réellement importées (exécutables et DLL du paquet, récursivement) --------------
tsa_runtime_search_dirs(search_dirs "${SOURCE_DIR}")
file(GLOB pkg_dlls "${DIST_DIR}/*.dll")
tsa_copy_runtime_dependencies(
    BINARIES ${dist_binaries} ${pkg_dlls}
    DESTINATION "${DIST_DIR}"
    SEARCH_DIRS ${search_dirs}
    EXCLUDE_REGEXES "^[Qq]t6" "^msvcp140" "^vcruntime140" "^concrt140" "^vccorlib140" "^vcomp140"
    RESULT_VAR copied
    UNRESOLVED_VAR unresolved)
foreach(u ${unresolved})
    if(NOT u MATCHES "^(msvcp140|vcruntime140|concrt140|vcomp140)")
        message(FATAL_ERROR "DLL requise introuvable : ${u}")
    endif()
endforeach()

# --- 4. Runtime MSVC redistribuable (app-local, licence Microsoft « Redistributable Code ») ---------
set(crt_dirs "")
if(VCREDIST_DIR AND NOT CONFIG STREQUAL "Debug")
    file(GLOB crt_dirs LIST_DIRECTORIES true "${VCREDIST_DIR}/x64/Microsoft.VC*.CRT" "${VCREDIST_DIR}/x64/Microsoft.VC*.OpenMP")
endif()
if(crt_dirs)
    foreach(d ${crt_dirs})
        file(GLOB crt "${d}/*.dll")
        file(COPY ${crt} DESTINATION "${DIST_DIR}")
    endforeach()
elseif(NOT CONFIG STREQUAL "Debug")
    message(WARNING "VCToolsRedistDir introuvable : runtime MSVC non inclus (installer vc_redist.x64.exe sur le poste cible)")
endif()

# --- 5. Ressources OpenCASCADE ----------------------------------------------------------------------
set(occt_src "${SOURCE_DIR}/opencascade-8.0.1-vc14-64/src")
foreach(r Shaders Textures StdResource SHMessage XSMessage UnitsAPI XSTEPResource TObj)
    if(EXISTS "${occt_src}/${r}")
        file(COPY "${occt_src}/${r}" DESTINATION "${DIST_DIR}/resources/occt")
    endif()
endforeach()

# --- 6. Moteur OpenSees -----------------------------------------------------------------------------
if(PACKAGE_OPENSEES AND EXISTS "${SOURCE_DIR}/thirdparty/OpenSees/bin/OpenSees.exe")
    message(WARNING "OpenSees inclus : sa licence (The Regents of the University of California) autorise l'usage "
                    "éducatif, de recherche et non lucratif ; un usage commercial exige un accord de l'Université. "
                    "Vérifier avant toute diffusion (docs/THIRD_PARTY_LICENSES.md) ou configurer -DTSA_PACKAGE_OPENSEES=OFF.")
    set(os "${DIST_DIR}/engines/OpenSees")
    file(COPY "${SOURCE_DIR}/thirdparty/OpenSees/bin/OpenSees.exe" "${SOURCE_DIR}/thirdparty/OpenSees/bin/libiomp5md.dll"
         DESTINATION "${os}/bin")
    file(COPY "${SOURCE_DIR}/thirdparty/OpenSees/lib" DESTINATION "${os}")
    # OpenSees.exe est lancé comme processus séparé : son propre runtime à côté de lui.
    foreach(d ${crt_dirs})
        file(GLOB crt "${d}/*.dll")
        file(COPY ${crt} DESTINATION "${os}/bin")
    endforeach()
endif()

# --- 7. Données livrées -----------------------------------------------------------------------------
if(EXISTS "${SOURCE_DIR}/Extensions")
    file(COPY "${SOURCE_DIR}/Extensions" DESTINATION "${DIST_DIR}")
endif()
file(MAKE_DIRECTORY "${DIST_DIR}/plugins")
# Modules livrés : disposition assemblée par la compilation (<build>/modules/<id> : module.json, bin/, …).
file(GLOB mods LIST_DIRECTORIES true "${BUILD_DIR}/modules/*")
foreach(m ${mods})
    if(IS_DIRECTORY "${m}" AND EXISTS "${m}/module.json")
        file(COPY "${m}" DESTINATION "${DIST_DIR}/modules" PATTERN "*.pdb" EXCLUDE PATTERN "*.ilk" EXCLUDE)
    endif()
endforeach()
file(MAKE_DIRECTORY "${DIST_DIR}/docs")
foreach(doc shortcut.txt TSA3D.md DEPLOYMENT.md SDK.md TEMPLATES.md SHORTCUTS.md)
    if(EXISTS "${SOURCE_DIR}/docs/${doc}")
        file(COPY "${SOURCE_DIR}/docs/${doc}" DESTINATION "${DIST_DIR}/docs")
    endif()
endforeach()
if(EXISTS "${SOURCE_DIR}/docs/schemas")
    file(COPY "${SOURCE_DIR}/docs/schemas" DESTINATION "${DIST_DIR}/docs")
endif()
foreach(ex tsa3d templates)
    if(EXISTS "${SOURCE_DIR}/sdk/examples/${ex}")
        file(COPY "${SOURCE_DIR}/sdk/examples/${ex}" DESTINATION "${DIST_DIR}/docs/examples")
    endif()
endforeach()

# --- 8. Licences ------------------------------------------------------------------------------------
file(MAKE_DIRECTORY "${DIST_DIR}/licenses")
if(EXISTS "${SOURCE_DIR}/docs/THIRD_PARTY_LICENSES.md")
    file(COPY "${SOURCE_DIR}/docs/THIRD_PARTY_LICENSES.md" DESTINATION "${DIST_DIR}/licenses")
endif()
foreach(lic "${SOURCE_DIR}/opencascade-8.0.1-vc14-64/LICENSE_LGPL_21.txt" "${SOURCE_DIR}/opencascade-8.0.1-vc14-64/OCCT_LGPL_EXCEPTION.txt")
    if(EXISTS "${lic}")
        file(COPY "${lic}" DESTINATION "${DIST_DIR}/licenses/OpenCASCADE")
    endif()
endforeach()
if(WINDEPLOYQT_EXECUTABLE)
    # <Qt>/<version>/<kit>/bin/windeployqt.exe → <Qt>/Licenses (installateur officiel : LICENSE = LGPLv3 + GPLv3)
    get_filename_component(qt_root "${WINDEPLOYQT_EXECUTABLE}/../.." ABSOLUTE)
    file(GLOB qt_lic "${qt_root}/../../Licenses/LICENSE" "${qt_root}/../../Licenses/*LGPL*" "${qt_root}/../Licenses/*LGPL*")
    if(qt_lic)
        file(COPY ${qt_lic} DESTINATION "${DIST_DIR}/licenses/Qt")
    else()
        message(WARNING "Texte de licence Qt introuvable : à ajouter dans licenses/Qt avant diffusion")
    endif()
endif()
# Bibliothèques tierces importées par OpenCASCADE : textes fournis par le SDK (les absents sont signalés
# dans docs/THIRD_PARTY_LICENSES.md).
set(tp "${SOURCE_DIR}/3rdparty-vc14-64")
foreach(pair "FreeType|${tp}/freetype-2.13.3-x64/LICENSE.TXT" "oneTBB|${tp}/tbb-2021.13.0-x64/LICENSE.txt"
             "FreeImage|${tp}/freeimage-3.18.0-x64/license-fi.txt")
    string(REPLACE "|" ";" pair "${pair}")
    list(GET pair 0 lname)
    list(GET pair 1 lfile)
    if(EXISTS "${lfile}")
        file(COPY "${lfile}" DESTINATION "${DIST_DIR}/licenses/${lname}")
    endif()
endforeach()

# --- 9. Manifeste : chaque fichier distribué, taille, SHA-256, catégorie -------------------------
file(GLOB_RECURSE all_files RELATIVE "${DIST_DIR}" "${DIST_DIR}/*")
list(SORT all_files)
set(entries "")
set(total 0)
foreach(f ${all_files})
    file(SIZE "${DIST_DIR}/${f}" sz)
    file(SHA256 "${DIST_DIR}/${f}" h)
    math(EXPR total "${total} + ${sz}")
    set(cat "donnée")
    if(f MATCHES "^engines/")
        set(cat "moteur")
    elseif(f MATCHES "^resources/")
        set(cat "ressource")
    elseif(f MATCHES "^(platforms|styles|imageformats|iconengines|tls|networkinformation|generic|translations)/")
        set(cat "plugin Qt")
    elseif(f MATCHES "^[Qq]t6.*\\.dll$")
        set(cat "Qt")
    elseif(f MATCHES "^(msvcp|vcruntime|concrt|vccorlib|vcomp)140.*\\.dll$")
        set(cat "runtime MSVC")
    elseif(f MATCHES "^TK.*\\.dll$")
        set(cat "OpenCASCADE")
    elseif(f MATCHES "\\.dll$")
        set(cat "bibliothèque tierce")
    elseif(f MATCHES "\\.exe$")
        set(cat "exécutable")
    elseif(f MATCHES "^(licenses|docs)/")
        set(cat "documentation")
    elseif(f MATCHES "^(Extensions|modules|plugins)/")
        set(cat "extension")
    endif()
    string(REPLACE "\"" "\\\"" fj "${f}")
    list(APPEND entries "    {\"path\": \"${fj}\", \"size\": ${sz}, \"sha256\": \"${h}\", \"category\": \"${cat}\"}")
endforeach()
string(TIMESTAMP now "%Y-%m-%dT%H:%M:%SZ" UTC)
if(EXISTS "${DIST_DIR}/engines/OpenSees/bin/OpenSees.exe")
    set(os_json true)
else()
    set(os_json false)
endif()
list(LENGTH all_files nfiles)
string(JOIN ",\n" body ${entries})
file(WRITE "${DIST_DIR}/MANIFEST.json"
"{\n  \"product\": \"${PRODUCT}\",\n  \"version\": \"${VERSION}\",\n  \"configuration\": \"${CONFIG}\",\n  \"generated\": \"${now}\",\n  \"files\": ${nfiles},\n  \"totalBytes\": ${total},\n  \"openSeesIncluded\": ${os_json},\n  \"entries\": [\n${body}\n  ]\n}\n")
math(EXPR mb "${total} / 1048576")
message(STATUS "=== Paquet terminé : ${nfiles} fichiers, ${mb} Mo — vérifier : python tools/verify_package.py \"${DIST_DIR}\" ===")
