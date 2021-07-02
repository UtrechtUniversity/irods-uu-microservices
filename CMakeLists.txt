cmake_minimum_required(VERSION 2.8.12)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeModules")

find_package(IRODS 4.2.8 EXACT REQUIRED)

message(STATUS "Building for irods-server version ${IRODS_VERSION}")

# Note: Microservices for iRODS >= 4.2 need to be built with the compiler and
# standard libarary provided by iRODS-externals packages.
# The following packages need to be installed to build the uu microservices:
#
# irods-externals-clang-runtime3.8-0
# irods-externals-clang6.0-0

set(RELEASE_VERSION "0.9.0")

set(CMAKE_CXX_COMPILER ${IRODS_EXTERNALS_FULLPATH_CLANG}/bin/clang++)

project(wur-msis CXX)

add_compile_options(-std=c++14
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
include_directories("/usr/include/irods")

find_package(Boost REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

find_package(CURL REQUIRED)
include_directories(SYSTEM ${CURL_INCLUDE_DIRS})

find_package(Jansson REQUIRED)
include_directories(SYSTEM ${JANSSON_INCLUDE_DIRS})

include_directories(SYSTEM "/usr/include/irods")

add_library(msiCreateEpicPID     SHARED src/msiCreateEpicPID.cc)
add_library(msiUpdateEpicPID     SHARED src/msiUpdateEpicPID.cc)
add_library(msiGetEpicPID        SHARED src/msiGetEpicPID.cc)
add_library(msiDeleteEpicPID     SHARED src/msiDeleteEpicPID.cc)

target_link_libraries(msiCreateEpicPID     LINK_PUBLIC ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
target_link_libraries(msiUpdateEpicPID     LINK_PUBLIC ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
target_link_libraries(msiGetEpicPID        LINK_PUBLIC ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
target_link_libraries(msiDeleteEpicPID     LINK_PUBLIC ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})

install(TARGETS
        msiCreateEpicPID
        msiUpdateEpicPID
        msiGetEpicPID
        msiDeleteEpicPID
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

# Run custom CMake module to detect OS family
include(${CMAKE_MODULE_PATH}/DetectOS.cmake)
message(STATUS "Found Linux distro ${DETECTED_OS}")

# Determine which type of package should be generated
if(DETECTED_OS MATCHES "^(Ubuntu|Debian)$")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_VERSION "${IRODS_VERSION}-${RELEASE_VERSION}")
elseif(DETECTED_OS MATCHES "^(CentOS|RedHatCompatible)$")
    set(CPACK_GENERATOR "RPM")
    set(CPACK_PACKAGE_VERSION "${IRODS_VERSION}_${RELEASE_VERSION}")
endif()

set(CPACK_PACKAGE_NAME "irods-microservices")
set(CPACK_PACKAGE_VENDOR "Wageningen University Research <iemand@wur.nl>")
set(CPACK_PACKAGE_CONTACT "Wageningen University Research <iemand@wur.nl>")

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package/description.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Miscellaneous iRODS microservices.")

set(CPACK_RESOURCE_FILE_README  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_PACKAGE_LICENSE "LGPLv3")
set(CPACK_RPM_PACKAGE_REQUIRES "irods-server = ${IRODS_VERSION}, boost-locale >= 1.51, libcurl >= 7.29.0, jansson >= 2.10")
set(CPACK_RPM_PACKAGE_CONFLICTS "rit-irods-microservices, irods-microservice-plugins-loadmeta")
set(CPACK_RPM_PACKAGE_URL "https://git.wur.nl/rdm-infrastructure/irods-microservices")
set(CPACK_RPM_PACKAGE_AUTOREQ 0)
set(CPACK_RPM_PACKAGE_AUTOPROV 0)

set(CPACK_DEBIAN_PACKAGE_DEPENDS "irods-server = ${IRODS_VERSION}, irods-runtime = ${IRODS_VERSION}")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://git.wur.nl/rdm-infrastructure/irods-microservices")
set(CPACK_DEBIAN_PACKAGE_SECTION "contrib/science")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}")

include(CPack)

# }}}
