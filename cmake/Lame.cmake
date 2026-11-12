# LAME 3.100 (LGPL-2.0-or-later): the MP3 encoder behind "Save as MP3". Only the encoding
# library is built, from the release archive, checked against its published SHA-256.
FetchContent_Declare(lame
    URL https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz
        https://deb.debian.org/debian/pool/main/l/lame/lame_3.100.orig.tar.gz
    URL_HASH SHA256=ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_SUBDIR do-not-add)
FetchContent_MakeAvailable(lame)

set(_lame "${lame_SOURCE_DIR}/libmp3lame")
add_library(riffhouse_lame STATIC
    ${_lame}/VbrTag.c ${_lame}/bitstream.c ${_lame}/encoder.c ${_lame}/fft.c ${_lame}/gain_analysis.c
    ${_lame}/id3tag.c ${_lame}/lame.c ${_lame}/newmdct.c ${_lame}/presets.c ${_lame}/psymodel.c
    ${_lame}/quantize.c ${_lame}/quantize_pvt.c ${_lame}/reservoir.c ${_lame}/set_get.c ${_lame}/tables.c
    ${_lame}/takehiro.c ${_lame}/util.c ${_lame}/vbrquantize.c ${_lame}/version.c ${_lame}/mpglib_interface.c)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|amd64")
    target_sources(riffhouse_lame PRIVATE ${_lame}/vector/xmm_quantize_sub.c)
endif()
target_include_directories(riffhouse_lame
    PRIVATE "${CMAKE_SOURCE_DIR}/cmake/lame" "${lame_SOURCE_DIR}/include" "${_lame}" "${_lame}/vector"
    INTERFACE "${lame_SOURCE_DIR}/include")
target_compile_definitions(riffhouse_lame PRIVATE HAVE_CONFIG_H)
if (MSVC)
    target_compile_options(riffhouse_lame PRIVATE /w)
    target_compile_definitions(riffhouse_lame PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(riffhouse_lame PRIVATE -w)
    find_library(RIFFHOUSE_LIBM m)
    if (RIFFHOUSE_LIBM)
        target_link_libraries(riffhouse_lame PRIVATE ${RIFFHOUSE_LIBM})
    endif()
endif()
