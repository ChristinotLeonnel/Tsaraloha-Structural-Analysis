cmake_minimum_required(VERSION 3.20)

# Déploiement de DÉVELOPPEMENT (après chaque compilation) : les DLL nécessaires à côté de l'exécutable pour
# le lancer depuis le dossier de compilation. Le paquet distribuable est produit par PackageTSA.cmake
# (cible package_<produit>), dans un dossier propre.
#
# Paramètres (-D) : TARGET_DIR, SOURCE_DIR, TARGET_FILE, WINDEPLOYQT_EXECUTABLE (facultatif)

if(NOT TARGET_DIR)
    message(FATAL_ERROR "TARGET_DIR non défini.")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/RuntimeDependencies.cmake")

message(STATUS "=== Déploiement des dépendances vers : ${TARGET_DIR} ===")

# 1. DLL importées (OCCT, FreeType, FreeImage, TBB, ffmpeg, OpenVR, jemalloc, runtime MSVC) : seules celles
#    réellement nécessaires, résolues par dumpbin. Qt est laissé à windeployqt.
tsa_runtime_search_dirs(search_dirs "${SOURCE_DIR}")
if(EXISTS "${TARGET_FILE}")
    tsa_copy_runtime_dependencies(
        BINARIES "${TARGET_FILE}"
        DESTINATION "${TARGET_DIR}"
        SEARCH_DIRS ${search_dirs}
        EXCLUDE_REGEXES "^[Qq]t6"
        RESULT_VAR copied
        UNRESOLVED_VAR unresolved)
    list(LENGTH copied n)
    message(STATUS "Dépendances résolues copiées : ${n}")
    if(unresolved)
        message(WARNING "DLL introuvables : ${unresolved}")
    endif()
endif()

# 2. Déploiement Qt via windeployqt
if(WINDEPLOYQT_EXECUTABLE AND EXISTS "${WINDEPLOYQT_EXECUTABLE}" AND EXISTS "${TARGET_FILE}")
    execute_process(
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-compiler-runtime --no-opengl-sw "${TARGET_FILE}"
        RESULT_VARIABLE WINDEPLOY_RES OUTPUT_QUIET)
    if(NOT WINDEPLOY_RES EQUAL 0)
        message(WARNING "windeployqt a retourné le code : ${WINDEPLOY_RES}")
    endif()
    # Traduction française des textes standard de Qt (BUG-020) : seule qtbase_fr est déployée.
    get_filename_component(QT_BIN_DIR "${WINDEPLOYQT_EXECUTABLE}" DIRECTORY)
    set(QT_FR_QM "${QT_BIN_DIR}/../translations/qtbase_fr.qm")
    if(EXISTS "${QT_FR_QM}")
        file(COPY "${QT_FR_QM}" DESTINATION "${TARGET_DIR}/translations")
    endif()
endif()

# 3. Bibliothèques TSALib
if(EXISTS "${SOURCE_DIR}/Extensions")
    file(COPY "${SOURCE_DIR}/Extensions" DESTINATION "${TARGET_DIR}")
endif()

message(STATUS "=== Déploiement terminé ===")
