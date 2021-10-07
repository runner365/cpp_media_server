include(ExternalProject)

set(EXTRA_CFLAGS "-I${PREFIX_DIR}/include")
set(EXTRA_LDFLAGS "-L${PREFIX_DIR}/lib64")
set(EXTRA_LIBS "-lm -lpthread -ldl")

add_subdirectory(libsdptransform)
add_subdirectory(libsrtp)