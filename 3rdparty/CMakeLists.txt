include(ExternalProject)

add_subdirectory(libsdptransform)
add_subdirectory(libuv)
set(ENABLE_OPENSSL ON)
set(TEST_APPS OFF CACHE INTERNAL "")
add_subdirectory(libsrtp)
unset(TEST_APPS)

IF (WIN32)
    return()
ENDIF()

set(EXTRA_CFLAGS "-I${PREFIX_DIR}/include")
set(EXTRA_LDFLAGS "-L${PREFIX_DIR}/lib")
set(EXTRA_LIBS "-lm -pthread -ldl")

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

ExternalProject_Add(yasm
    URL ${CMAKE_CURRENT_SOURCE_DIR}/yasm-1.3.0.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/yasm-1.3.0
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        ./configure --prefix=${PREFIX_DIR} --libdir=${INSTALL_RPATH}
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install
    )

#opus-1.3.1.tar.gz
ExternalProject_Add(libopus
    DEPENDS yasm
    URL ${CMAKE_CURRENT_SOURCE_DIR}/opus-1.3.1.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/opus-1.3.1
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        ./configure --prefix=${PREFIX_DIR} --libdir=${INSTALL_RPATH} --disable-asm --enable-static --enable-shared -with-pic
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install
    )

ExternalProject_Add(libx264
    DEPENDS yasm
    URL ${CMAKE_CURRENT_SOURCE_DIR}/x264-master.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/x264-master
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        ./configure --prefix=${PREFIX_DIR} --libdir=${INSTALL_RPATH} --disable-asm --enable-static
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install
    )

ExternalProject_Add(libfdk_aac
    URL ${CMAKE_CURRENT_SOURCE_DIR}/fdk-aac-0.1.6.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/fdk-aac-0.1.6
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        ./configure --prefix=${PREFIX_DIR} --libdir=${INSTALL_RPATH} --disable-asm --enable-static --disable-shared -with-pic
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install
    )


IF (APPLE)
#ffmpeg-git.tar.gz
ExternalProject_Add(ffmpeg
    DEPENDS yasm libopus libx264 libfdk_aac
    URL ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-4.1.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-4.1
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        export PKG_CONFIG_PATH=${INSTALL_RPATH}/pkgconfig && ./configure --prefix=${PREFIX_DIR} --libdir=${INSTALL_RPATH} --enable-shared --enable-static --disable-avdevice --disable-doc --disable-devices --enable-libfdk-aac --enable-libopus --enable-nonfree --enable-gpl --extra-libs=-ldl --disable-ffplay --disable-ffprobe
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install
    )
ELSEIF (UNIX)
ExternalProject_Add(ffmpeg
    DEPENDS yasm libopus libx264 libfdk_aac
    URL ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-4.1.tar.gz
    SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg-4.1
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND
        export PKG_CONFIG_PATH=${INSTALL_RPATH}/pkgconfig && ./configure --prefix=${PREFIX_DIR}
        --libdir=${INSTALL_RPATH}
        --disable-shared
        --enable-static
        --disable-asm
        --disable-avdevice
        --disable-doc
        --disable-devices
        --disable-ffplay
        --disable-devices --disable-securetransport --disable-audiotoolbox --disable-videotoolbox
        --disable-lzma
        --disable-filter=coreimage --disable-filter=coreimagesrc
        --disable-iconv
        --disable-vaapi
        --enable-libfdk-aac
        --enable-libopus
        --enable-nonfree
        --enable-gpl
        --extra-libs="-lm -lpthread -ldl -lz -lbz2"
        --disable-ffplay
        --disable-ffprobe
        --enable-pic
    BUILD_COMMAND make -j 4
    INSTALL_COMMAND make install
    )
ENDIF ()


