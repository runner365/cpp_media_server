include(ExternalProject)

set(EXTRA_CFLAGS "-I${PREFIX_DIR}/include")
set(EXTRA_LDFLAGS "-L${PREFIX_DIR}/lib")
set(EXTRA_LIBS "-lm -lpthread -ldl")

add_subdirectory(libsdptransform)

IF (APPLE)
ExternalProject_Add(openssl
    URL ${CMAKE_CURRENT_SOURCE_DIR}/openssl-1.1.1.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/openssl
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        ./config --prefix=${PREFIX_DIR} no-shared
        --libdir=${PREFIX_DIR}/lib
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install_sw
    )
ELSEIF (UNIX)
ExternalProject_Add(openssl
    URL ${CMAKE_CURRENT_SOURCE_DIR}/openssl-1.1.1.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/openssl
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        ./Configure --prefix=${PREFIX_DIR} no-shared linux-x86_64
        --libdir=${PREFIX_DIR}/lib
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install_sw
    )
ENDIF ()

ExternalProject_Add(libsrtp
    DEPENDS openssl
    URL ${CMAKE_CURRENT_SOURCE_DIR}/libsrtp.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/libsrtp
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        ./configure --prefix=${PREFIX_DIR} --libdir=${INSTALL_RPATH} --enable-openssl --with-openssl-dir=${INSTALL_RPATH}/..
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install
    )