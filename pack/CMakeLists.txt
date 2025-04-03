cmake_minimum_required (VERSION 3.18.4)
project (pack VERSION 1.0.0)

add_executable (ns-pack
	${PROJECT_SOURCE_DIR}/../nspre.hpp
	main.cpp
)

target_include_directories (ns-pack PUBLIC ${PROJECT_SOURCE_DIR}/..)
