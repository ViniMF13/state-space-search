cmake_minimum_required(VERSION 3.14)
project(TP_Ponte_Tocha)

# Configura o padrão do C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Baixa e compila o prometheus-cpp automaticamente
include(FetchContent)
FetchContent_Declare(
  prometheus-cpp
  GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
  GIT_TAG        v1.1.0
)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(prometheus-cpp)

# Diz ao CMake para compilar nosso arquivo principal (que criaremos a seguir)
add_executable(tp1_app src/main.cpp)

# Conecta a biblioteca do Prometheus ao nosso executável
target_link_libraries(tp1_app PRIVATE prometheus-cpp::pull prometheus-cpp::core)