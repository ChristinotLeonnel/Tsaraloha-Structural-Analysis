# =============================================================================
# cmake/SetupDependencies.cmake
# Gestion et telechargement automatique des dependances externes :
# - OpenCASCADE Technology (OCCT 8.0.1)
# - Dependances 3rdparty (FreeType, TBB, FreeImage, Jemalloc, etc.)
# =============================================================================

cmake_minimum_required(VERSION 3.20)

option(TSA_AUTO_DOWNLOAD_DEPS "Telecharger automatiquement OpenCASCADE et 3rdparty s'ils sont absents" ON)
set(TSA_OCCT_VERSION "8.0.1" CACHE STRING "Version d'OpenCASCADE a utiliser")

function(setup_external_dependencies)
    set(OCCT_FOLDER_NAME "opencascade-${TSA_OCCT_VERSION}-vc14-64")
    set(LOCAL_OCCT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${OCCT_FOLDER_NAME}")
    set(LOCAL_OCCT_CMAKE "${LOCAL_OCCT_DIR}/cmake")
    set(LOCAL_3RDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty-vc14-64")

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
    # 2. Telechargement d'OpenCASCADE si absent
    # -------------------------------------------------------------------------
    if(NEED_OCCT_DOWNLOAD)
        if(NOT TSA_AUTO_DOWNLOAD_DEPS)
            message(FATAL_ERROR
                "OpenCASCADE ${TSA_OCCT_VERSION} introuvable.\n"
                "Veuillez definir -DOpenCASCADE_DIR=\"<chemin>/cmake\" ou "
                "activer le telechargement automatique avec -DTSA_AUTO_DOWNLOAD_DEPS=ON."
            )
        endif()

        message(STATUS "==================================================================")
        message(STATUS "[TSA] OpenCASCADE ${TSA_OCCT_VERSION} introuvable localement.")
        message(STATUS "[TSA] Telechargement automatique depuis GitHub Releases...")
        message(STATUS "==================================================================")

        set(OCCT_URL "https://github.com/Open-Cascade-SAS/OCCT/releases/download/V${TSA_OCCT_VERSION}/opencascade-release-no-pch.zip")
        set(DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps_download")
        set(OUTER_ZIP "${DOWNLOAD_DIR}/opencascade-release-no-pch.zip")
        set(EXTRACT_STAGE "${DOWNLOAD_DIR}/occt_stage")

        file(MAKE_DIRECTORY "${DOWNLOAD_DIR}")
        file(MAKE_DIRECTORY "${EXTRACT_STAGE}")

        message(STATUS "[TSA] Telechargement de ${OCCT_URL} ...")
        file(DOWNLOAD
            "${OCCT_URL}"
            "${OUTER_ZIP}"
            SHOW_PROGRESS
            STATUS OCCT_DL_STATUS
            LOG OCCT_DL_LOG
        )
        list(GET OCCT_DL_STATUS 0 OCCT_DL_CODE)
        list(GET OCCT_DL_STATUS 1 OCCT_DL_MSG)
        if(NOT OCCT_DL_CODE EQUAL 0)
            message(FATAL_ERROR "Echec du telechargement d'OpenCASCADE : ${OCCT_DL_MSG}\n${OCCT_DL_LOG}")
        endif()

        message(STATUS "[TSA] Extraction de l'archive principale...")
        file(ARCHIVE_EXTRACT INPUT "${OUTER_ZIP}" DESTINATION "${EXTRACT_STAGE}")

        # L'archive officielle GitHub contient l'archive imbriquee opencascade-8.0.1-vc14-64.zip
        file(GLOB INNER_OCCT_ZIPS "${EXTRACT_STAGE}/*.zip")
        if(INNER_OCCT_ZIPS)
            list(GET INNER_OCCT_ZIPS 0 INNER_ZIP)
            message(STATUS "[TSA] Decompression du package SDK vers ${CMAKE_CURRENT_SOURCE_DIR} ...")
            file(ARCHIVE_EXTRACT INPUT "${INNER_ZIP}" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}")
        else()
            execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${EXTRACT_STAGE}" "${LOCAL_OCCT_DIR}")
        endif()

        # Nettoyage du dossier temporaire de telechargement
        file(REMOVE_RECURSE "${DOWNLOAD_DIR}")

        if(EXISTS "${LOCAL_OCCT_CMAKE}/OpenCASCADEConfig.cmake")
            message(STATUS "[TSA] OpenCASCADE ${TSA_OCCT_VERSION} installe avec succes dans ${LOCAL_OCCT_DIR} !")
            set(OpenCASCADE_DIR "${LOCAL_OCCT_CMAKE}" CACHE PATH "Path to OpenCASCADE CMake config" FORCE)
        else()
            message(FATAL_ERROR "Echec de l'installation d'OpenCASCADE : ${LOCAL_OCCT_CMAKE}/OpenCASCADEConfig.cmake est introuvable apres extraction.")
        endif()
    endif()

    # -------------------------------------------------------------------------
    # 3. Verification et telechargement des dependances 3rdparty
    # -------------------------------------------------------------------------
    if(NOT EXISTS "${LOCAL_3RDPARTY_DIR}")
        if(NOT TSA_AUTO_DOWNLOAD_DEPS)
            message(WARNING
                "Dossier 3rdparty (${LOCAL_3RDPARTY_DIR}) introuvable.\n"
                "Les DLLs d'execution (FreeType, TBB, etc.) risquent de manquer lors du lancement de l'application."
            )
        else()
            message(STATUS "==================================================================")
            message(STATUS "[TSA] Dependances 3rdparty introuvables.")
            message(STATUS "[TSA] Telechargement automatique de 3rdparty-vc14-64...")
            message(STATUS "==================================================================")

            set(TP_URL "https://github.com/Open-Cascade-SAS/OCCT/releases/download/V${TSA_OCCT_VERSION}/3rdparty-vc14-64.zip")
            set(DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps_download")
            set(OUTER_TP_ZIP "${DOWNLOAD_DIR}/3rdparty-vc14-64-outer.zip")
            set(EXTRACT_STAGE "${DOWNLOAD_DIR}/3rdparty_stage")
            set(FINAL_STAGE "${DOWNLOAD_DIR}/3rdparty_final")

            file(MAKE_DIRECTORY "${DOWNLOAD_DIR}")
            file(MAKE_DIRECTORY "${EXTRACT_STAGE}")
            file(MAKE_DIRECTORY "${FINAL_STAGE}")

            message(STATUS "[TSA] Telechargement de ${TP_URL} ...")
            file(DOWNLOAD
                "${TP_URL}"
                "${OUTER_TP_ZIP}"
                SHOW_PROGRESS
                STATUS TP_DL_STATUS
                LOG TP_DL_LOG
            )
            list(GET TP_DL_STATUS 0 TP_DL_CODE)
            list(GET TP_DL_STATUS 1 TP_DL_MSG)
            if(NOT TP_DL_CODE EQUAL 0)
                message(FATAL_ERROR "Echec du telechargement des dependances 3rdparty : ${TP_DL_MSG}\n${TP_DL_LOG}")
            endif()

            message(STATUS "[TSA] Extraction de l'archive 3rdparty...")
            file(ARCHIVE_EXTRACT INPUT "${OUTER_TP_ZIP}" DESTINATION "${EXTRACT_STAGE}")

            file(GLOB INNER_TP_ZIPS "${EXTRACT_STAGE}/*.zip")
            if(INNER_TP_ZIPS)
                list(GET INNER_TP_ZIPS 0 INNER_ZIP)
                message(STATUS "[TSA] Decompression des bibliotheques 3rdparty...")
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
            message(STATUS "[TSA] Dependances 3rdparty installees avec succes dans ${LOCAL_3RDPARTY_DIR} !")
        endif()
    else()
        message(STATUS "Dependances 3rdparty detectees localement: ${LOCAL_3RDPARTY_DIR}")
    endif()

endfunction()
