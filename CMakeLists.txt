cmake_minimum_required(VERSION 2.8.12)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeModules")

find_package(IRODS 4.2.12 EXACT REQUIRED)

message(STATUS "Building for irods-server version ${IRODS_VERSION}")

# Note: Microservices for iRODS >= 4.2 need to be built with the compiler and
# standard libarary provided by iRODS-externals packages.
# The following packages need to be installed to build the uu microservices:
#
# irods-externals-clang-runtime3.8-0
# irods-externals-clang6.0-0

set(UU_RELEASE_VERSION "0.8.3")

set(CMAKE_CXX_COMPILER ${IRODS_EXTERNALS_FULLPATH_CLANG}/bin/clang++)

project(uu-msis CXX)

add_compile_options(-std=c++17
                    -Os
                    -fPIC
                    -DRODS_SERVER
                    -nostdinc++
                    -Wall
                    -Wextra
                    -Wpedantic
                    -Wcast-align
                    -Wredundant-decls
                    -Wuninitialized
                    -Wconversion
                    -Wno-missing-field-initializers
                    -Wno-unused-parameter)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
link_libraries(c++abi)
include_directories("${IRODS_EXTERNALS_FULLPATH_CLANG}/include/c++/v1")
include_directories("${IRODS_EXTERNALS_FULLPATH_JSON}/include")
include_directories("/usr/include/irods")

set(BOOST_ROOT "${IRODS_EXTERNALS_FULLPATH_BOOST}")

find_package(Boost REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

find_package(CURL REQUIRED)
include_directories(SYSTEM ${CURL_INCLUDE_DIRS})

find_package(Jansson REQUIRED)
include_directories(SYSTEM ${JANSSON_INCLUDE_DIRS})

find_package(uuid REQUIRED)
include_directories(SYSTEM ${UUID_INCLUDE_DIR})

find_package(LibArchive REQUIRED)
include_directories(SYSTEM ${LibArchive_INCLUDE_DIRS})

include_directories(SYSTEM "/usr/include/irods")

add_library(msiRegisterEpicPID        SHARED src/msiRegisterEpicPID.cc)
add_library(msiArchiveCreate          SHARED src/msiArchiveCreate.cc)
add_library(msiArchiveExtract         SHARED src/msiArchiveExtract.cc)
add_library(msiArchiveIndex           SHARED src/msiArchiveIndex.cc)
add_library(msi_json_arrayops         SHARED src/msi_json_arrayops.cc)
add_library(msi_json_objops           SHARED src/msi_json_objops.cc)
add_library(msi_add_avu               SHARED src/msi_add_avu.cpp)
add_library(msi_rmw_avu               SHARED src/msi_rmw_avu.cpp)

target_link_libraries(msiRegisterEpicPID        LINK_PUBLIC ${CURL_LIBRARIES} ${JANSSON_LIBRARIES} ${UUID_LIBRARIES})
target_link_libraries(msiArchiveCreate          LINK_PUBLIC ${LibArchive_LIBRARIES} ${JANSSON_LIBRARIES} )
target_link_libraries(msiArchiveExtract         LINK_PUBLIC ${LibArchive_LIBRARIES} ${JANSSON_LIBRARIES} )
target_link_libraries(msiArchiveIndex           LINK_PUBLIC ${LibArchive_LIBRARIES} ${JANSSON_LIBRARIES} )
target_link_libraries(msi_json_arrayops         LINK_PUBLIC ${JANSSON_LIBRARIES} ${Boost_LIBRARIES})
target_link_libraries(msi_json_objops           LINK_PUBLIC ${JANSSON_LIBRARIES} ${Boost_LIBRARIES})
target_link_libraries(msi_add_avu               LINK_PUBLIC ${Boost_LIBRARIES})
target_link_libraries(msi_rmw_avu               LINK_PUBLIC ${Boost_LIBRARIES})

install(TARGETS
        msiRegisterEpicPID
        msiArchiveCreate
        msiArchiveExtract
        msiArchiveIndex
        msi_json_arrayops
        msi_json_objops
        msi_add_avu
        msi_rmw_avu
        DESTINATION
        /usr/lib/irods/plugins/microservices)

# Packaging {{{

# Make sure your umask is set correctly when packaging:
# The default umask on CentOS 7 is 0002, which results in e.g. rwxrwxr-x
# directories within generated RPMs, which will conflict with existing
# directories from irods packages, which will be rwxr-xr-x.
# To create packages on CentOS 7, set your umask to 0022 first (`umask 0022`).

set(CPACK_MONOLITHIC_INSTALL 1)
set(CPACK_CMAKE_GENERATOR "Unix Makefiles")

execute_process(
  COMMAND "python" "-c" "from __future__ import print_function; import platform; print(platform.linux_distribution()[0].split()[0].strip().lower(), end='')"
  RESULT_VARIABLE IRODS_EXECUTE_PROCESS_RESULT_LINUX_DISTRIBUTION_NAME
  OUTPUT_VARIABLE IRODS_LINUX_DISTRIBUTION_NAME
)

message(STATUS "Found Linux distro ${IRODS_LINUX_DISTRIBUTION_NAME}")

# Determine which type of package should be generated
if(IRODS_LINUX_DISTRIBUTION_NAME MATCHES "^(ubuntu|debian)$")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_VERSION "${IRODS_VERSION}-${UU_RELEASE_VERSION}")
elseif(IRODS_LINUX_DISTRIBUTION_NAME MATCHES "^(centos)$")
    set(CPACK_GENERATOR "RPM")
    set(CPACK_PACKAGE_VERSION "${IRODS_VERSION}_${UU_RELEASE_VERSION}")
endif()

set(CPACK_PACKAGE_NAME "irods-uu-microservices")
set(CPACK_PACKAGE_VENDOR "Utrecht University <fbyoda@uu.nl>")
set(CPACK_PACKAGE_CONTACT "Utrecht University <fbyoda@uu.nl>")

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package/description.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Miscellaneous iRODS microservices developed or modified by Utrecht University.")

set(CPACK_RESOURCE_FILE_README  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_RPM_PACKAGE_RELEASE "0")
set(CPACK_RPM_PACKAGE_LICENSE "LGPLv3")
set(CPACK_RPM_PACKAGE_REQUIRES "irods-server = ${IRODS_VERSION}, boost-locale >= 1.51, libcurl >= 7.29.0, libxml2 >= 2.9.1, libxslt >= 1.1.28, jansson >= 2.10, libarchive >= 3.1.2")
set(CPACK_RPM_PACKAGE_CONFLICTS "rit-irods-microservices, irods-microservice-plugins-loadmeta")
set(CPACK_RPM_PACKAGE_URL "https://github.com/UtrechtUniversity/irods-uu-microservices")
set(CPACK_RPM_PACKAGE_AUTOREQ 0)
set(CPACK_RPM_PACKAGE_AUTOPROV 0)

set(CPACK_DEBIAN_PACKAGE_DEPENDS "irods-server ( = ${IRODS_VERSION}-1~bionic ), irods-runtime ( = ${IRODS_VERSION}-1~bionic )")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/UtrechtUniversity/irods-uu-microservices")
set(CPACK_DEBIAN_PACKAGE_SECTION "contrib/science")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}")

include(CPack)

# }}}
