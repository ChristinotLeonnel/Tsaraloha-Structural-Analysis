# MetDeDeplacement 2 — méthode des déplacements pour ossatures planes.
# Bibliothèque autonome (C++20, sans dépendance). Utilisable seule ou via TSA (add_subdirectory).
# L'ancien code (entrée JSON « dessin ») est conservé dans legacy/ et n'est pas compilé.

cmake_minimum_required(VERSION 3.20)
project(MetDeDeplacement VERSION 2.0.0 LANGUAGES CXX)

add_library(MetDeDeplacement STATIC
    include/mdd/MetDeDeplacement.h
    src/Internal.h
    src/Element.cpp
    src/BandSolver.cpp
    src/Solver.cpp
    src/Curves.cpp
)
target_include_directories(MetDeDeplacement PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(MetDeDeplacement PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(MetDeDeplacement PRIVATE /W4 /utf-8 /permissive-)
endif()
