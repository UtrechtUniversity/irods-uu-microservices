/**
 * \file
 * \brief     iRODS microservice to compute SHA256 checksum of a data object replica
 * \author    Sirjan Kaur
 * \author    Lazlo Westerhof
 * \copyright Copyright (c) 2024, Utrecht University
 *
 * This file is part of irods-uu-microservices.
 *
 * irods-uu-microservices is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * irods-uu-microservices is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with irods-uu-microservices.
 * If not, see <http://www.gnu.org/licenses/>.
 */
#include "irods_includes.hh"
#include "irods/SHA256Strategy.hpp"
#include "irods/checksum.h"

#include <boost/filesystem.hpp>

int msiFileChecksum(msParam_t* _path, msParam_t* _checksum, ruleExecInfo_t* _rei)
{
    // Convert parameter values to C strings.
    char* path = parseMspForStr(_path);
    if (!path) {
        return SYS_INVALID_INPUT_PARAM;
    }

    // Check that user is rodsadmin.
    if (_rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
        return SYS_USER_NO_PERMISSION;
    }

    // Check that file exists in path.
    if (!boost::filesystem::is_regular_file(path)) {
        rodsLog(LOG_ERROR, "msi_file_checksum: path <%s> is not a file", path);
        return SYS_INVALID_FILE_PATH;
    }

    // Compute SHA256 checksum of file.
    char checksum[NAME_LEN];
    int retCode = chksumLocFile(path, checksum, irods::SHA256_NAME.c_str());

    if (retCode < 0) {
        rodsLog(LOG_ERROR, "msi_file_checksum: failed to calculate checksum for file: %s", path);
    }
    else {
        fillStrInMsParam(_checksum, checksum);
    }

    return retCode;
}

extern "C" irods::ms_table_entry* plugin_factory()
{
    irods::ms_table_entry* msvc = new irods::ms_table_entry(2);

    msvc->add_operation<msParam_t*, msParam_t*, ruleExecInfo_t*>(
        "msiFileChecksum", std::function<int(msParam_t*, msParam_t*, ruleExecInfo_t*)>(msiFileChecksum));
    return msvc;
}
