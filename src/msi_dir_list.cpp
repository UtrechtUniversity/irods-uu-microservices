/**
 * \file
 * \brief     iRODS microservice to list all files and subdirectories in a directory
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
#include "irods_ms_plugin.hpp"
#include "irods/rcMisc.h"
#include "irods/resource_administration.hpp"
#include "rsFileStat.hpp"
#include "rsGenQuery.hpp"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>
#include <vector>

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

/** Internal function to get an attribute of a resource, based on its name. */
static int get_resource_attribute_by_name(rsComm_t* rsComm,
                                          char* resource_name,
                                          char* resource_value_out,
                                          int column_number)
{
    genQueryInp_t genQueryInp;
    genQueryOut_t* genQueryOut = NULL;
    sqlResult_t* resource_data;
    char equalsResourceName[MAX_NAME_LEN + 1];
    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    memset(&equalsResourceName, '\0', MAX_NAME_LEN + 1);

    snprintf(equalsResourceName, MAX_NAME_LEN + 1, "='%s'", resource_name);
    addInxVal(&genQueryInp.sqlCondInp, COL_R_RESC_NAME, equalsResourceName);
    addInxIval(&genQueryInp.selectInp, column_number, 1);

    genQueryInp.maxRows = 1;
    genQueryOut = NULL;
    int status = rsGenQuery(rsComm, &genQueryInp, &genQueryOut);

    int out_status = 0;

    if (status >= 0) {
        if (genQueryOut->rowCnt != 1) {
            out_status = CAT_UNKNOWN_RESOURCE;
        }
        else {
            if ((resource_data = getSqlResultByInx(genQueryOut, column_number)) == NULL) {
                rodsLog(LOG_ERROR,
                        "msi_file_checksum: getSqlResultByInx for column %d failed on lookup of %s",
                        column_number,
                        resource_name);
                out_status = UNMATCHED_KEY_OR_INDEX;
            }
            else {
                strncpy(resource_value_out, resource_data->value, MAX_NAME_LEN + 1);
                out_status = 0;
            }
        }
    }
    else {
        out_status = status;
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);

    return out_status;
}

/** This function converts a resource name to a resource ID.
 *  The resource ID is written as char array to resource_id_out.
 */
static int get_resource_id(rsComm_t* rsComm, char* resource_name, char* resource_id_out)
{
    return get_resource_attribute_by_name(rsComm, resource_name, resource_id_out, COL_R_RESC_ID);
}

/** This function retrieves the vault path of a resource based on its name
 *  to resource_id_out.
 */
static int get_resource_vault_path(rsComm_t* rsComm, char* resource_name, char* resource_vault_path_out)
{
    return get_resource_attribute_by_name(rsComm, resource_name, resource_vault_path_out, COL_R_VAULT_PATH);
}

int msiDirList(msParam_t* _path, msParam_t* _rescName, msParam_t* _list, ruleExecInfo_t* _rei)
{
    // Convert parameter values to C strings.
    char* path_str = parseMspForStr(_path);
    if (!path_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char* rescName = parseMspForStr(_rescName);
    if (!rescName) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char* list_str = parseMspForStr(_list);
    if (!list_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    // Check that user is rodsadmin.
    if (_rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
        return SYS_USER_NO_PERMISSION;
    }

    // Look up resource ID of resource.
    char resource_id_str[MAX_NAME_LEN];
    memset(&resource_id_str, '\0', MAX_NAME_LEN);
    int status_resource_id = get_resource_id(_rei->rsComm, rescName, resource_id_str);

    // Return error if resource does not exist
    if (status_resource_id == CAT_NO_ROWS_FOUND) {
        rodsLog(LOG_ERROR, "msi_dir_list: could not find resource [%s]", rescName);
        return CAT_UNKNOWN_RESOURCE;
    }
    else if (status_resource_id < 0) {
        rodsLog(LOG_ERROR,
                "msi_dir_list: error while looking up resource ID of resource [%s]: %d",
                rescName,
                status_resource_id);
        return status_resource_id;
    }

    fs::path physical_path_bp(path_str);
    // Check that physical path exists.
    if (!fs::exists(physical_path_bp)) {
        rodsLog(LOG_ERROR, "msi_dir_list: physical path <%s> does not exist", path_str);
        return SYS_INVALID_FILE_PATH;
    }

    // Check that physical path is a directory.
    if (!fs::is_directory(physical_path_bp)) {
        rodsLog(LOG_ERROR, "msi_dir_list: physical path <%s> is not a directory", path_str);
        return SYS_INVALID_FILE_PATH;
    }

    // Retrieve resource vault path
    char resource_vault_path[MAX_NAME_LEN + 1];
    memset(&resource_vault_path, '\0', MAX_NAME_LEN + 1);
    int status_resource_vault_path = get_resource_vault_path(_rei->rsComm, rescName, resource_vault_path);
    if (status_resource_vault_path < 0) {
        rodsLog(LOG_ERROR,
                "msi_dir_list: error while looking up resource vault path of resource [%s]: %d",
                rescName,
                status_resource_vault_path);
        return status_resource_vault_path;
    }

    // Check that canonical physical path is in resource vault path. Return error if not.
    fs::path normalized_physical_path_bp = physical_path_bp.lexically_normal();
    const char* normalized_physical_path_str = normalized_physical_path_bp.c_str();
    if (strncmp(normalized_physical_path_str, resource_vault_path, strlen(resource_vault_path)) ||
        normalized_physical_path_str[strlen(resource_vault_path)] != '/')
    {
        rodsLog(
            LOG_ERROR, "msi_dir_list: physical path is not inside resource vault for %s", normalized_physical_path_str);
        return SYS_INVALID_FILE_PATH;
    }

    try {
        pt::ptree jsonResult;
        fs::directory_iterator endIter;

        // Iterate through directory.
        for (fs::directory_iterator iter(normalized_physical_path_bp); iter != endIter; ++iter) {
            const fs::directory_entry entry = *iter;
            pt::ptree entryJson;

            entryJson.put("name", entry.path().filename().string());

            if (fs::is_regular_file(entry.status())) {
                entryJson.put("type", "file");
            }
            else if (fs::is_directory(entry.status())) {
                entryJson.put("type", "directory");
            }
            else if (fs::is_symlink(entry.symlink_status())) {
                entryJson.put("type", "symlink");
            }

            if (fs::is_regular_file(entry.status())) {
                entryJson.put("filesize", fs::file_size(entry.path()));
            }

            if (fs::is_directory(entry.status())) {
                int numEntries = std::distance(fs::directory_iterator(entry.path()), fs::directory_iterator{});
                entryJson.put("subdirectory_entries", numEntries);
            }

            std::time_t modifiedTime = fs::last_write_time(entry.path());
            char* localModifiedTime = std::asctime(std::localtime(&modifiedTime));

            // Remove trailing newline.
            localModifiedTime[strcspn(localModifiedTime, "\n")] = '\0';
            std::string modifiedTimestamp(localModifiedTime);

            entryJson.put("modified_timestamp", modifiedTimestamp);
            jsonResult.push_back(std::make_pair(entry.path().string(), entryJson));
        }

        std::stringstream ss;
        pt::write_json(ss, jsonResult);
        fillStrInMsParam(_list, ss.str().c_str());
    }
    catch (const fs::filesystem_error& error) {
        rodsLog(LOG_ERROR, "msi_dir_list: filesystem error for <%s> - %s", path_str, error.what());
        return SYS_INVALID_FILE_PATH;
    }

    _rei->status = 0;

    return _rei->status;
}

extern "C" irods::ms_table_entry* plugin_factory()
{
    irods::ms_table_entry* msvc = new irods::ms_table_entry(3);

    msvc->add_operation<msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*>(
        "msiDirList", std::function<int(msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*)>(msiDirList));

    return msvc;
}
