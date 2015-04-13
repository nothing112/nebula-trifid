# Locate FBX
# This module defines:
# FBX_INCLUDE_DIR, where to find the headers
#
# FBX_LIBRARY
# FBX_FOUND
#
# $FBX_DIR is an environment variable that would
# correspond to the ./configure --prefix=$FBX_DIR

IF(APPLE)
    SET(FBX_LIBDIR "gcc4/ub")
ELSEIF(CMAKE_COMPILER_IS_GNUCXX)
    SET(FBX_LIBDIR "gcc4")
ELSEIF(MSVC80)
    SET(FBX_LIBDIR "vs2005")
ELSEIF(MSVC90)
    SET(FBX_LIBDIR "vs2008")
ELSEIF(MSVC10)
    SET(FBX_LIBDIR "vs2010")
ELSEIF(MSVC11)
    SET(FBX_LIBDIR "vs2012")
ELSEIF(MSVC12 OR MSVC_VERSION>1800)
    SET(FBX_LIBDIR "vs2013")
ENDIF()

IF(APPLE)
    # do nothing
ELSEIF(CMAKE_CL_64)
    SET(FBX_LIBDIR ${FBX_LIBDIR}/x64)
ELSEIF(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(FBX_LIBDIR ${FBX_LIBDIR}/x64)
ELSE()
    SET(FBX_LIBDIR ${FBX_LIBDIR}/x86)
ENDIF()

IF(APPLE)
    SET(FBX_LIBNAME "libfbxsdk")
ELSEIF(CMAKE_COMPILER_IS_GNUCXX)
    SET(FBX_LIBNAME "fbxsdk")
ELSE()
    SET(FBX_LIBNAME "libfbxsdk-md")
ENDIF()

SET(FBX_LIBNAME_DEBUG ${FBX_LIBNAME}d)

SET( FBX_SEARCH_PATHS
    $ENV{FBX_DIR}
    "$ENV{ProgramW6432}/Autodesk/FBX/FBX SDK/2015.1"
    "$ENV{PROGRAMFILES}/Autodesk/FBX/FBX SDK/2015.1"
    /Applications/Autodesk/FBXSDK20151
    /opt/fbx/
)

# search for headers & debug/release libraries
FIND_PATH(FBX_INCLUDE_DIR "fbxsdk.h"
    PATHS ${FBX_SEARCH_PATHS}
    PATH_SUFFIXES "include")
FIND_LIBRARY( FBX_LIBRARY_REL ${FBX_LIBNAME}
    PATHS ${FBX_SEARCH_PATHS}
    PATH_SUFFIXES "lib/${FBX_LIBDIR}/release" "lib/${FBX_LIBDIR}")

#Once one of the calls succeeds the result variable will be set and stored in the cache so that no call will search again.

#no debug d suffix, search in debug folder only
FIND_LIBRARY( FBX_LIBRARY_DEBUG ${FBX_LIBNAME}
    PATHS ${FBX_SEARCH_PATHS}
    PATH_SUFFIXES "lib/${FBX_LIBDIR}/debug")
FIND_LIBRARY( FBX_LIBRARY_DEBUG ${FBX_LIBNAME_DEBUG}
    PATHS ${FBX_SEARCH_PATHS}
    PATH_SUFFIXES "lib/${FBX_LIBDIR}")

IF(FBX_LIBRARY_DEBUG AND FBX_LIBRARY_REL)
	SET(FBX_LIBRARY optimized ${FBX_LIBRARY_REL} debug ${FBX_LIBRARY_DEBUG})
ENDIF()
FIND_PACKAGE(PackageHandleStandardArgs REQUIRED)

find_package_handle_standard_args(FBX REQUIRED_VARS FBX_INCLUDE_DIR FBX_LIBRARY)

MARK_AS_ADVANCED(FBX_LIBRARY_DEBUG  FBX_LIBRARY FBX_INCLUDE_DIR)