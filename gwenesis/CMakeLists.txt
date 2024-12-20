#cmake_minimum_required(VERSION 3.13)

add_library(gwenesis INTERFACE)
file(GLOB_RECURSE SRC "*.c")
target_sources(gwenesis
INTERFACE
	${SRC}
)

