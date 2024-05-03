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
#include "irods/resource_administration.hpp"
#include "rsFileStat.hpp"
#include "rsGenQuery.hpp"

#include <boost/filesystem.hpp>

/** Internal function to get an attribute of a resource, based on its name
 */
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

/** This function converts a resource name to a resource ID. The resource ID is written as char array
 *  to resource_id_out
 */
static int get_resource_id(rsComm_t* rsComm, char* resource_name, char* resource_id_out)
{
    return get_resource_attribute_by_name(rsComm, resource_name, resource_id_out, COL_R_RESC_ID);
}

/** This function retrieves the vault path of a resource based on its name
 *  *  to resource_id_out
 *   */
static int get_resource_vault_path(rsComm_t* rsComm, char* resource_name, char* resource_vault_path_out)
{
    return get_resource_attribute_by_name(rsComm, resource_name, resource_vault_path_out, COL_R_VAULT_PATH);
}

/** This function retrieves the location of a resource based on its name
 *  *  to resource_loc_out
 *   */
static int get_resource_loc(rsComm_t* rsComm, char* resource_name, char* resource_loc_out)
{
    return get_resource_attribute_by_name(rsComm, resource_name, resource_loc_out, COL_R_LOC);
}

int msiFileChecksum(msParam_t* _path, msParam_t* _rescName, msParam_t* _checksum, ruleExecInfo_t* _rei)
{
    // Convert parameter values to C strings.
    char* path = parseMspForStr(_path);
    if (!path) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char* rescName = parseMspForStr(_rescName);
    if (!rescName) {
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

    // Look up resource ID of resource.
    char resource_id_str[MAX_NAME_LEN];
    memset(&resource_id_str, '\0', MAX_NAME_LEN);
    int status_resource_id = get_resource_id(_rei->rsComm, rescName, resource_id_str);

    // Return error if resource does not exist
    if (status_resource_id == CAT_NO_ROWS_FOUND) {
        rodsLog(LOG_ERROR, "msi_file_checksum: could not find resource [%s]", rescName);
        return CAT_UNKNOWN_RESOURCE;
    }
    else if (status_resource_id < 0) {
        rodsLog(LOG_ERROR,
                "msi_file_checksum: error while looking up resource ID of resource [%s]: %d",
                rescName,
                status_resource_id);
        return status_resource_id;
    }

    // Convert resource ID C string to long value
    char* end_resource_id_str = end_resource_id_str;
    long resource_id = strtol(resource_id_str, &end_resource_id_str, 10);
    if (*end_resource_id_str != '\0') {
        rodsLog(LOG_ERROR, "msi_file_checksum: failed to convert resource ID %s to long value.", resource_id_str);
        return CAT_UNKNOWN_RESOURCE;
    }

    // Retrieve resource vault path
    char resource_vault_path[MAX_NAME_LEN + 1];
    memset(&resource_vault_path, '\0', MAX_NAME_LEN + 1);
    int status_resource_vault_path = get_resource_vault_path(_rei->rsComm, rescName, resource_vault_path);
    if (status_resource_vault_path < 0) {
        rodsLog(LOG_ERROR,
                "msi_file_checksum: error while looking up resource vault path of resource [%s]: %d",
                rescName,
                status_resource_vault_path);
        return status_resource_vault_path;
    }

    // Check that canonical physical path is in resource vault path. Return error if not.
    boost::filesystem::path physical_path_bp(path);
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

    // Retrieve resource location
    char resource_loc[MAX_NAME_LEN + 1];
    memset(&resource_loc, '\0', MAX_NAME_LEN + 1);
    int status_resource_loc = get_resource_loc(_rei->rsComm, rescName, resource_loc);
    if (status_resource_loc < 0) {
        rodsLog(LOG_ERROR,
                "msi_file_checksum: error while looking up resource vault path of resource [%s]: %d",
                rescName,
                status_resource_loc);
        return status_resource_loc;
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
                path,
                resource_id,
                status_rsFileStat);
        return status_rsFileStat;
    }
    else {
        // If the hostname and resource location is same, then
        // compute SHA256 checksum of file.
        if (strcmp(_rei->rsComm->myEnv.rodsHost, resource_loc) == 0) {
            char checksum[NAME_LEN];
            _rei->status = chksumLocFile(path, checksum, irods::SHA256_NAME.c_str());

            if (_rei->status < 0) {
                rodsLog(LOG_ERROR, "msi_file_checksum: failed to calculate checksum for file: %s", path);
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
