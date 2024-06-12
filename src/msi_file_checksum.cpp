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
#include "irods_ms_plugin.hpp"
#include "irods/rcMisc.h"
#include "Archive.hh"
#include "irods/resource_administration.hpp"
#include "rsFileStat.hpp"
#include "rsGenQuery.hpp"

#define IRODS_QUERY_ENABLE_SERVER_SIDE_API
#include "irods/query_builder.hpp"

#include <boost/filesystem.hpp>
#include <string_view>
#include <vector>
#include <string>
#include <fmt/format.h>

/** Internal function to get attributes of a resource, based on its name
 */

static json_t* get_resource_info_by_name(RsComm& _comm, char* resource_name)
{
    json_t* list;
    const std::string qstr = fmt::format("SELECT RESC_ID, "
                                         "RESC_VAULT_PATH, "
                                         "RESC_LOC"
                                         "WHERE RESC_NAME = '{}'",
                                         resource_name);

    list = NULL;

    irods::experimental::query_builder qb;
    auto q = qb.build<RsComm>(_comm, qstr);
    if (q.empty()) {
        THROW(CAT_UNKNOWN_RESOURCE, "msi_stat_vault: cannot find the resource.");
    }
    else {
        for (auto&& row : q) {
            json_t* json;

            json = json_object();

            json_object_set(json, "resc_id", json_string(row[0].c_str()));
            json_object_set(json, "resc_vault_path", json_string(row[1].c_str()));
            json_object_set(json, "resc_loc", json_string(row[2].c_str()));

            if (list == NULL) {
                list = json_array();
            }
            json_array_append(list, json);
        }
    }

    qb.clear();

    return list;
}

/** This function gets the resource attributes by its name. The result is stored into
 *  json_array.
 */
static json_t* get_resource_info(RsComm& _comm, char* resource_name)
{
    return get_resource_info_by_name(_comm, resource_name);
}

int msiFileChecksum(msParam_t* _physical_path_name,
                    msParam_t* _resource_name,
                    msParam_t* _checksum,
                    ruleExecInfo_t* _rei)
{
    // Convert parameter values to C strings.
    char* physical_path_str = parseMspForStr(_physical_path_name);
    if (!physical_path_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char* resource_name_str = parseMspForStr(_resource_name);
    if (!resource_name_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    // Check that user is rodsadmin.
    if (_rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
        return SYS_USER_NO_PERMISSION;
    }

    // Check that file exists in path.
    if (!boost::filesystem::is_regular_file(physical_path_str)) {
        rodsLog(LOG_ERROR, "msi_file_checksum: path <%s> is not a file", physical_path_str);
        return SYS_INVALID_FILE_PATH;
    }

    json_t* list = NULL;
    size_t i;

    char resource_id_str[MAX_NAME_LEN];
    memset(&resource_id_str, '\0', MAX_NAME_LEN);

    char resource_vault_path[MAX_NAME_LEN + 1];
    memset(&resource_vault_path, '\0', MAX_NAME_LEN + 1);

    char resource_loc[MAX_NAME_LEN + 1];
    memset(&resource_loc, '\0', MAX_NAME_LEN + 1);

    list = get_resource_info(*_rei->rsComm, resource_name_str);

    for (i = 0; i < json_array_size(list); i++) {
        json_t* json;

        json = json_object();
        json = json_array_get(list, i);
        strcpy(resource_id_str, json_string_value(json_object_get(json, "resc_id")));
        strcpy(resource_vault_path, json_string_value(json_object_get(json, "resc_vault_path")));
        strcpy(resource_loc, json_string_value(json_object_get(json, "resc_loc")));
    }

    // Convert resource ID C string to long value
    char* end_resource_id_str = end_resource_id_str;
    long resource_id = strtol(resource_id_str, &end_resource_id_str, 10);
    if (*end_resource_id_str != '\0') {
        rodsLog(LOG_ERROR, "msi_file_checksum: failed to convert resource ID %s to long value.", resource_id_str);
        return CAT_UNKNOWN_RESOURCE;
    }

    // Check that canonical physical path is in resource vault path. Return error if not.
    boost::filesystem::path physical_path_bp(physical_path_str);
    boost::filesystem::path normalized_physical_path_bp = physical_path_bp.lexically_normal();
    const char* normalized_physical_path_str = normalized_physical_path_bp.c_str();
    if (strncmp(normalized_physical_path_str, resource_vault_path, strlen(resource_vault_path)) ||
        normalized_physical_path_str[strlen(resource_vault_path)] != '/')
    {
        rodsLog(LOG_ERROR,
                "msi_file_checksum: physical path is not inside resource vault for %s",
                normalized_physical_path_str);
        return SYS_INVALID_FILE_PATH;
    }

    rodsLog(LOG_ERROR, "The resource location is <%s>", resource_loc);

    // Call rsFileStat to determine size and type
    fileStatInp_t fileStatInp;
    rodsStat_t* fileStatOut = NULL;
    rstrcpy(fileStatInp.fileName, normalized_physical_path_str, sizeof(fileStatInp.fileName));
    fileStatInp.rescId = resource_id;
    const int status_rsFileStat = rsFileStat(_rei->rsComm, &fileStatInp, &fileStatOut);

    if (status_rsFileStat == -516002) {
        rodsLog(LOG_ERROR, "msi_file_checksum: no such file or directory.");
    }
    else if (status_rsFileStat < 0) {
        rodsLog(LOG_ERROR,
                "msi_file_checksum: unexpected error during rsFileStat of path %s in resource %s (%d)",
                physical_path_str,
                resource_id,
                status_rsFileStat);
        return status_rsFileStat;
    }
    else {
        // If the hostname and resource location is same, then
        // compute SHA256 checksum of file.
        if (strcmp(_rei->rsComm->myEnv.rodsHost, resource_loc) == 0) {
            char checksum[NAME_LEN];
            _rei->status = chksumLocFile(physical_path_str, checksum, irods::SHA256_NAME.c_str());

            if (_rei->status < 0) {
                rodsLog(LOG_ERROR, "msi_file_checksum: failed to calculate checksum for file: %s", physical_path_str);
            }
            else {
                fillStrInMsParam(_checksum, checksum);
            }

            return _rei->status;
        }
        else {
            rodsLog(LOG_ERROR,
                    "msi_file_checksum: failed to calculate checksum as hostname is different from location of the "
                    "given resource.");
            return USER_INVALID_RESC_INPUT;
        }
    }

    return _rei->status;
}

extern "C" irods::ms_table_entry* plugin_factory()
{
    irods::ms_table_entry* msvc = new irods::ms_table_entry(3);

    msvc->add_operation<msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*>(
        "msiFileChecksum", std::function<int(msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*)>(msiFileChecksum));
    return msvc;
}
