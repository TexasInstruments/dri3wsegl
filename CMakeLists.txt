cmake_minimum_required(VERSION 3.0)
project(dri3ws)

IF(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Release'.")
    SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
ENDIF()

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)


IF(NOT PVR_BUILD_TYPE)
    message(STATUS "Setting PVR build type to 'Release'.")
    SET(PVR_BUILD_TYPE Release CACHE STRING "Choose the type of PVR build." FORCE)
    set_property(CACHE PVR_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
ENDIF()

string(TOUPPER "${PVR_BUILD_TYPE}" U_PVR_BUILD_TYPE)


IF(NOT BO_TYPE)
    message(STATUS "Setting BO_TYPE type to 'Dumb'.")
    SET(BO_TYPE Dumb CACHE STRING "Choose Dumb or GBM buffers" FORCE)
    set_property(CACHE BO_TYPE PROPERTY STRINGS "Dumb" "GBM")
ENDIF()

string(TOUPPER "${BO_TYPE}" U_BO_TYPE)

set(PVR_KM ~/work/sgx/omap5-sgx-ddk-linux CACHE FILEPATH "desc")
set(PVR_UM_LIBS ~/work/sgx/omap5-sgx-ddk-um-linux/targetfs/jacinto6evm/lib CACHE FILEPATH "desc")

set(ENABLE_DRI3TEST OFF CACHE BOOL "Enable dri3test")


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -Wno-unused-parameter -fvisibility=hidden")

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

string(TOLOWER ${PVR_BUILD_TYPE} PVR_BUILD)


set(PVR_KM_BUILD ${PVR_KM}/eurasia_km/eurasiacon/binary2_omap_linux_${PVR_BUILD})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include ${PVR_KM_BUILD}/config_kernel.h")

include_directories(
    SGX_DDK_REL_1.14@4004414_API_headers_MIT/eurasia/eurasiacon/include
    ${PVR_KM}/eurasia_km/include4
    ${PVR_KM}/eurasia_km/services4/srvkm/hwdefs
)

link_directories(${PVR_UM_LIBS})


find_package(PkgConfig REQUIRED)

pkg_check_modules(X11_XCB x11-xcb REQUIRED)
pkg_check_modules(XCB xcb REQUIRED)
pkg_check_modules(XCBDRI3 xcb-dri3 REQUIRED)
pkg_check_modules(XCBPRESENT xcb-present REQUIRED)

if (${U_BO_TYPE} MATCHES DUMB)
	add_definitions(-DDRI3WS_USE_DUMB)
elseif (${U_BO_TYPE} MATCHES GBM)
	pkg_check_modules(GBM gbm REQUIRED)
	add_definitions(-DDRI3WS_USE_GBM)
else()
	message(FATAL_ERROR "No BO type selected: ${U_BO_TYPE}")
endif()

add_definitions(-D_GNU_SOURCE)
add_definitions(-DWSEGL_MODULE -DGLX_DIRECT_RENDERING -DHAVE_PTHREAD -DMODULE_NAME=libpvrDRI3WSEGL.so)

add_library(pvrDRI3WSEGL SHARED dri3_ws.c dri3_ws.h xhelpers.c xhelpers.h pvrhelpers.c pvrhelpers.h helpers.h)

target_link_libraries(pvrDRI3WSEGL ${GBM_LIBRARIES} srv_um pvr2d
		${X11_XCB_LIBRARIES} ${XCB_LIBRARIES} ${XCBDRI3_LIBRARIES} ${XCBPRESENT_LIBRARIES})

install(TARGETS pvrDRI3WSEGL
    DESTINATION usr/lib/)


if (ENABLE_DRI3TEST)
    pkg_check_modules(GBM gbm REQUIRED)

    pkg_check_modules(LIBDRM libdrm>=2.4.64 REQUIRED)
    pkg_check_modules(X11XCB x11-xcb REQUIRED)

    pkg_check_modules(LIBDRM_OMAP libdrm_omap)
    if(LIBDRM_OMAP_FOUND)
        add_definitions(-DHAS_LIBDRM_OMAP)
    endif()

    pkg_check_modules(LIBDRM_ETNAVIV libdrm_etnaviv)
    if(LIBDRM_ETNAVIV_FOUND)
        add_definitions(-DHAS_LIBDRM_ETNAVIV)
    endif()

    include_directories(
        ${LIBDRM_INCLUDE_DIRS}
        ${GBM_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
    )

    add_executable(dri3test dri3test.c)

    target_link_libraries(dri3test
        ${LIBDRM_LIBRARIES}
        ${LIBDRM_OMAP_LIBRARIES}
        ${LIBDRM_ETNAVIV_LIBRARIES}
        ${GBM_LIBRARIES}
        ${X11_LIBRARIES}
        ${XCB_LIBRARIES}
        ${X11XCB_LIBRARIES}
        ${XCBDRI3_LIBRARIES}
        ${XCBPRESENT_LIBRARIES}
    )
endif()
