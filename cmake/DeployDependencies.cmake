cmake_minimum_required(VERSION 3.20)

# Paramètres attendus passés via -D :
# TARGET_DIR : Répertoire de l'exécutable (ex: build/Release ou build/Debug)
# SOURCE_DIR : Répertoire racine du projet
# TARGET_FILE : Chemin complet vers TSA.exe
# WINDEPLOYQT_EXECUTABLE : Chemin vers windeployqt.exe (optionnel)

if(NOT TARGET_DIR)
    message(FATAL_ERROR "TARGET_DIR non défini.")
endif()

message(STATUS "=== Déploiement automatique des dépendances vers : ${TARGET_DIR} ===")

# 1. Copie des DLLs OpenCASCADE
file(GLOB OCCT_DLLS "${SOURCE_DIR}/opencascade-8.0.1-vc14-64/win64/vc14/bin/*.dll")
if(OCCT_DLLS)
    file(COPY ${OCCT_DLLS} DESTINATION "${TARGET_DIR}")
    list(LENGTH OCCT_DLLS OCCT_COUNT)
    message(STATUS "OpenCASCADE : ${OCCT_COUNT} DLLs copiées avec succès.")
endif()

# 2. Copie des DLLs 3rdparty (FreeType, TBB, FreeImage, Jemalloc, etc.)
file(GLOB_RECURSE TP_DLLS "${SOURCE_DIR}/3rdparty-vc14-64/*.dll")
if(TP_DLLS)
    file(COPY ${TP_DLLS} DESTINATION "${TARGET_DIR}")
    list(LENGTH TP_DLLS TP_COUNT)
    message(STATUS "3rdparty : ${TP_COUNT} DLLs copiées avec succès.")
endif()

# 3. Déploiement Qt via windeployqt
if(WINDEPLOYQT_EXECUTABLE AND EXISTS "${WINDEPLOYQT_EXECUTABLE}" AND EXISTS "${TARGET_FILE}")
    message(STATUS "Exécution de windeployqt sur ${TARGET_FILE}...")
    execute_process(
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            --compiler-runtime
            "${TARGET_FILE}"
        RESULT_VARIABLE WINDEPLOY_RES
    )
    if(NOT WINDEPLOY_RES EQUAL 0)
        message(WARNING "windeployqt a retourné le code : ${WINDEPLOY_RES}")
    else()
        message(STATUS "Qt : Déploiement windeployqt terminé avec succès.")
    endif()
endif()

message(STATUS "=== Déploiement terminé. L'exécutable peut être lancé directement ! ===")
