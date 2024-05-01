/*

 * This microservice performs a stat on a filename within a unixfilesystem
 * resource vault, checking whether the filename refers to an existing file
 * and returning its size if present. It can be used to help verify integrity
 * of data objects.
 *
 * Example code to invoke this microservice:
 *
 *
   # \file
   # \brief job
   # \author Sietse Snel
   # \copyright Copyright (c) 2024, Utrecht university. All rights reserved
   # \license GPLv3, see LICENSE
   #
   # Run vault stat service for testing
   #
   # Input parameters:
   # * filename: the physical path of the file in the vault
   # * rescname: name of the resource (must be a unixfilesystem resource)
   #
   # Output parameters:
   # * type: can be "NOTEXIST", "FILE", "OTHER" or "DIRECTORY"
   # * size: size in bytes, as string (always "0" for type "NOTEXIST", "DIRECTORY" or "OTHER")
   #
   # Example code to call microservice:

   runVaultStat {
         *filename='/var/lib/irods/Vault1_2/yoda/licenses/GNU General Public License v3.0.txt';
         *rescname='dev001_2';
         *type='';
         *size='';
         writeLine("serverLog","Running vault stat for *filename");
         msi_stat_vault(*rescname, *filename, *type, *size);
         writeLine("serverLog","Ran vault stat for *filename. Result: *type / *size");
   }

   input null
   output ruleExecOut

   # If the rule is stored in a file named `rule.r`, it can be executed using:
   #
   # $ irule -r irods_rule_engine_plugin-irods_rule_language-instance -F rule.r
   #
   # The output of the rule is then logged in the rodsLog.
 *
 */

#include "irods_error.hpp"
#include "irods_log.hpp"
#include "irods_ms_plugin.hpp"
#include "irods/rcMisc.h"
#include "irods/resource_administration.hpp"
#include "rsFileStat.hpp"
#include "rsGenQuery.hpp"

#include <boost/filesystem/path.hpp>
#include <string_view>

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
                        "msi_stat_vault: getSqlResultByInx for column %d failed on lookup of %s",
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

/** This function retrieves the resource type of a resource based on its name.
 *   */
static int get_resource_type(rsComm_t* rsComm, char* resource_name, char* resource_type_out)
{
    return get_resource_attribute_by_name(rsComm, resource_name, resource_type_out, COL_R_TYPE_NAME);
}

/** This function retrieves the vault path of a resource based on its name
 *  *  to resource_id_out
 *   */
static int get_resource_vault_path(rsComm_t* rsComm, char* resource_name, char* resource_vault_path_out)
{
    return get_resource_attribute_by_name(rsComm, resource_name, resource_vault_path_out, COL_R_VAULT_PATH);
}

int msiStatVault(msParam_t* _resource_name,
                 msParam_t* _physical_path_name,
                 msParam_t* _type,
                 msParam_t* _size,
                 ruleExecInfo_t* _rei)
{
    // Convert parameter value to C strings
    char* resource_name_str = parseMspForStr(_resource_name);
    if (!resource_name_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char* physical_path_str = parseMspForStr(_physical_path_name);
    if (!physical_path_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char* type_str = parseMspForStr(_type);
    if (!type_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char* size_str = parseMspForStr(_size);
    if (!size_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    // Check that user is rodsadmin
    if (_rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
        return SYS_USER_NO_PERMISSION;
    }

    // Look up resource ID of resource.
    char resource_id_str[MAX_NAME_LEN];
    memset(&resource_id_str, '\0', MAX_NAME_LEN);
    int status_resource_id = get_resource_id(_rei->rsComm, resource_name_str, resource_id_str);

    // Return error if resource does not exist
    if (status_resource_id == CAT_NO_ROWS_FOUND) {
        rodsLog(LOG_ERROR, "msi_stat_vault: could not find resource [%s]", resource_name_str);
        return CAT_UNKNOWN_RESOURCE;
    }
    else if (status_resource_id < 0) {
        rodsLog(LOG_ERROR,
                "msi_stat_vault: error while looking up resource ID of resource [%s]: %d",
                resource_name_str,
                status_resource_id);
        return status_resource_id;
    }

    // Convert resource ID C string to long value
    char* end_resource_id_str = end_resource_id_str;
    long resource_id = strtol(resource_id_str, &end_resource_id_str, 10);
    if (*end_resource_id_str != '\0') {
        rodsLog(LOG_ERROR, "msi_stat_vault: failed to convert resource ID %s to long value.", resource_id_str);
        return CAT_UNKNOWN_RESOURCE;
    }

    // Check resource type. Return error if it is not UFS
    char resource_type[MAX_NAME_LEN];
    memset(&resource_type, '\0', MAX_NAME_LEN);
    int status_resource_type = get_resource_type(_rei->rsComm, resource_name_str, resource_type);
    if (status_resource_type < 0) {
        rodsLog(LOG_ERROR,
                "msi_stat_vault: error while looking up resource type of resource [%s]: %d",
                resource_name_str,
                status_resource_type);
        return status_resource_type;
    }
    else if (strcmp("unixfilesystem", resource_type) != 0 and strcmp("unix file system", resource_type) != 0) {
        rodsLog(LOG_ERROR,
                "msi_stat_vault: unable to stat files on resource %s. Not a unixfilesystem resource",
                resource_name_str);
        return CAT_UNKNOWN_RESOURCE;
    }

    // Retrieve resource vault path
    char resource_vault_path[MAX_NAME_LEN + 1];
    memset(&resource_vault_path, '\0', MAX_NAME_LEN + 1);
    int status_resource_vault_path = get_resource_vault_path(_rei->rsComm, resource_name_str, resource_vault_path);
    if (status_resource_vault_path < 0) {
        rodsLog(LOG_ERROR,
                "msi_stat_vault: error while looking up resource vault path of resource [%s]: %d",
                resource_name_str,
                status_resource_vault_path);
        return status_resource_vault_path;
    }

    // Check that canonical physical path is in resource vault path. Return error if not.
    boost::filesystem::path physical_path_bp(physical_path_str);
    boost::filesystem::path normalized_physical_path_bp = physical_path_bp.lexically_normal();
    const char* normalized_physical_path_str = normalized_physical_path_bp.c_str();
    if (strncmp(normalized_physical_path_str, resource_vault_path, strlen(resource_vault_path)) ||
        normalized_physical_path_str[strlen(resource_vault_path)] != '/')
    {
        rodsLog(LOG_ERROR,
                "msi_stat_vault: physical path is not inside resource vault for %s",
                normalized_physical_path_str);
        return SYS_INVALID_FILE_PATH;
    }

    // Call rsFileStat to determine size and type
    fileStatInp_t fileStatInp;
    rodsStat_t* fileStatOut = NULL;
    rstrcpy(fileStatInp.fileName, normalized_physical_path_str, sizeof(fileStatInp.fileName));
    fileStatInp.rescId = resource_id;
    const int status_rsFileStat = rsFileStat(_rei->rsComm, &fileStatInp, &fileStatOut);

    // Convert fileStatOut and rsFileStat status to string parameters for type and size
    char size_output[20];
    char type_output[20];
    memset(&size_output, '\0', 20);
    memset(&type_output, '\0', 20);
    if (status_rsFileStat == -516002) {
        strcpy(size_output, "0");
        strcpy(type_output, "NOTEXIST");
    }
    else if (status_rsFileStat < 0) {
        rodsLog(LOG_ERROR,
                "msi_stat_vault unexpected error during rsFileStat of path %s in resource %s (%d)",
                physical_path_str,
                resource_name_str,
                status_rsFileStat);
        return status_rsFileStat;
    }
    else if ((fileStatOut->st_mode & S_IFREG) != 0) {
        strcpy(type_output, "FILE");
        sprintf(size_output, "%lld", fileStatOut->st_size);
    }
    else if ((fileStatOut->st_mode & S_IFDIR) != 0) {
        strcpy(type_output, "DIR");
        strcpy(size_output, "0");
    }
    else {
        strcpy(type_output, "OTHER");
        strcpy(size_output, "0");
    }

    fillStrInMsParam(_type, type_output);
    fillStrInMsParam(_size, size_output);

    _rei->status = 0;

    return _rei->status;
}

extern "C" irods::ms_table_entry* plugin_factory()
{
    irods::ms_table_entry* msvc = new irods::ms_table_entry(4);

    msvc->add_operation<msParam_t*, msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*>(
        "msiStatVault",
        std::function<int(msParam_t*, msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*)>(msiStatVault));
    return msvc;
}
