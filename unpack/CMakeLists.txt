cmake_minimum_required (VERSION 3.18.4)
project (unpack VERSION 1.0.0)

add_executable (ns-unpack
	${PROJECT_SOURCE_DIR}/../nspre.hpp
	main.cpp
)

target_include_directories (ns-unpack PRIVATE ${PROJECT_SOURCE_DIR}/..)
