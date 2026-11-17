cmake_minimum_required(VERSION 3.16)
project(jarvis_son VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# En el teléfono (Termux) se activan dotprod/fp16 explícitamente; ver docs/decisiones.md (ADR-005).
option(JARVIS_ARM_NATIVO "Compilar con -march=armv8.2-a+dotprod+fp16" OFF)

add_library(jarvis_core src/config.cpp src/proceso.cpp src/texto.cpp src/orquestador.cpp)
target_include_directories(jarvis_core PUBLIC include)
target_compile_options(jarvis_core PRIVATE -Wall -Wextra)
if(JARVIS_ARM_NATIVO)
  target_compile_options(jarvis_core PRIVATE -march=armv8.2-a+dotprod+fp16)
endif()

add_executable(jarvis src/main.cpp)
target_link_libraries(jarvis PRIVATE jarvis_core)
target_compile_options(jarvis PRIVATE -Wall -Wextra)

enable_testing()
add_executable(jarvis_pruebas tests/unit/pruebas_texto.cpp)
target_link_libraries(jarvis_pruebas PRIVATE jarvis_core)
add_test(NAME pruebas_unitarias COMMAND jarvis_pruebas)
add_test(NAME turno_mock COMMAND jarvis --mock --wav mock.wav --config /no/existe.ini)
# El turno simulado debe recorrer todas las etapas y entregar la respuesta ya limpia.
set_tests_properties(turno_mock PROPERTIES
  PASS_REGULAR_EXPRESSION "Jarvis-son: Son las diez de la ma"
  FAIL_REGULAR_EXPRESSION "ERROR];think;end of text;https")
