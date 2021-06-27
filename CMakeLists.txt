cmake_minimum_required (VERSION 3.5)

project(chat C)

add_compile_options(-Wall -Wextra -Werror -std=c2x -O2 -g)

string(APPEND CMAKE_C_FLAGS "-D_GNU_SOURCE")

add_executable(server server.c)
add_executable(server_load server_load.c)
add_executable(client client.c)
add_executable(client_load client_load.c)
add_executable(client_load_no_uring client_load_no_uring.c)
add_executable(client_load_liburing client_load_liburing.c)

target_link_libraries(client_load_liburing "/usr/lib/liburing.so")

file(GLOB ALL_SOURCE_FILES *.c *.h)

add_custom_target(
	format
	COMMAND /usr/bin/clang-format
	-i
	-style=file
	-assume-filename=.clang-format
	${ALL_SOURCE_FILES}
)

set(TIDY_CHECKS 
	*
	-cppcoreguidelines-pro-type-reinterpret-cast
	-cppcoreguidelines-pro-bounds-array-to-pointer-decay
	-fuchsia-default-arguments
	-readability-implicit-bool-conversion
	-llvm-header-guard
	-hicpp-no-array-decay
	-misc-definitions-in-headers
	-cppcoreguidelines-pro-type-vararg
	-hicpp-vararg
	-fuchsia-overloaded-operator
	-llvm-include-order
)
string(REPLACE ";" "," TIDY_CHECKS_STR "${TIDY_CHECKS}")

add_custom_target(
	tidy
	COMMAND /usr/bin/clang-tidy
	-checks=${TIDY_CHECKS_STR}
	-p ${PROJECT_SOURCE_DIR}
	${ALL_SOURCE_FILES}
	--
	-Ilib
	--std=c2x
)
