cmake_minimum_required(VERSION 3.20)
project(TSA VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Activer l'infrastructure Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# =============================================================================
# Configuration Qt 6
# =============================================================================
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg Network)
find_package(Qt6 QUIET COMPONENTS Test)   # facultatif : tests des raccourcis (frappes simulées)

# =============================================================================
# Configuration OpenCASCADE (OCCT) et dépendances tierces
# =============================================================================
# Gestion automatique : si OCCT ou 3rdparty sont absents, CMake les télécharge
# automatiquement depuis les dépôts officiels GitHub (V8.0.1).
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetupDependencies.cmake")
setup_external_dependencies()

find_package(OpenCASCADE REQUIRED)

message(STATUS "OpenCASCADE Version: ${OpenCASCADE_VERSION}")
message(STATUS "OpenCASCADE Include: ${OpenCASCADE_INCLUDE_DIR}")

# =============================================================================
# Sources et cibles : module partagé avec TSALab (cmake/TSAProduct.cmake)
# =============================================================================
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/TSAProduct.cmake")
tsa_setup_compiler_launchers()

tsa_add_product(
    NAME TSA
    PRODUCT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/product"
    RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resources/TSA.rc"
)
