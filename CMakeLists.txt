cmake_minimum_required(VERSION 2.8.12)

find_package(IRODS 4.2.1 EXACT REQUIRED)

message(STATUS "Building for irods-server version ${IRODS_VERSION}")

# Note: Microservices for iRODS >=4.2 need to be built with the compiler and
# standard libarary provided by iRODS-externals packages.
# The following packages need to be installed to build the uu microservices:
#
# irods-externals-clang-runtime3.8-0
# irods-externals-clang3.8-0

set(CMAKE_CXX_COMPILER ${IRODS_EXTERNALS_FULLPATH_CLANG}/bin/clang++)

project(uu-msis CXX)

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

include_directories("${IRODS_EXTERNALS_FULLPATH_CLANG}/include/c++/v1")
include_directories("/usr/include/irods")

add_library(msiSetUpperCaseWhereQuery     SHARED src/msiSetUpperCaseWhereQuery.cc)
add_library(msiStrToUpper                 SHARED src/msiStrToUpper.cc)

install(TARGETS
        msiSetUpperCaseWhereQuery
        msiStrToUpper
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
#set(CPACK_GENERATOR "DEB")
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "irods-uu-microservices")
set(CPACK_PACKAGE_VENDOR "Utrecht University <fbyoda@uu.nl>")
set(CPACK_PACKAGE_CONTACT "Utrecht University <fbyoda@uu.nl>")
set(CPACK_PACKAGE_VERSION "4.2.1_0.1.0")

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package/description.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Miscellaneous microservices developed by Utrecht university.")

set(CPACK_RESOURCE_FILE_README  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_PACKAGE_LICENSE "LGPLv3")
set(CPACK_RPM_PACKAGE_REQUIRES "irods-server = ${IRODS_VERSION}")
set(CPACK_RPM_PACKAGE_URL "https://github.com/UtrechtUniversity/irods-uu-microservices")
set(CPACK_RPM_PACKAGE_AUTOREQ 0)
set(CPACK_RPM_PACKAGE_AUTOPROV 0)

set(CPACK_DEBIAN_PACKAGE_DEPENDS "irods-server = ${IRODS_VERSION}, irods-runtime = ${IRODS_VERSION}")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/UtrechtUniversity/irods-uu-microservices")
set(CPACK_DEBIAN_PACKAGE_SECTION "contrib/science")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}")

include(CPack)

# }}}
