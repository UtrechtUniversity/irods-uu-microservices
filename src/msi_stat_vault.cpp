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
   # \author Sirjan Kaur
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
#include "Archive.hh"
#include "rsFileStat.hpp"
#include "rsGenQuery.hpp"

#define IRODS_QUERY_ENABLE_SERVER_SIDE_API
#include "irods/query_builder.hpp"

#include <irods/filesystem.hpp>
#include <boost/filesystem/path.hpp>
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
                                         "RESC_TYPE_NAME, "
                                         "RESC_VAULT_PATH "
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
            json_object_set(json, "resc_type", json_string(row[1].c_str()));
            json_object_set(json, "resc_vault_path", json_string(row[2].c_str()));

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

    json_t* list = NULL;
    size_t i;
    char resource_id_str[MAX_NAME_LEN];
    memset(&resource_id_str, '\0', MAX_NAME_LEN);

    char resource_type[MAX_NAME_LEN];
    memset(&resource_type, '\0', MAX_NAME_LEN);

    char resource_vault_path[MAX_NAME_LEN + 1];
    memset(&resource_vault_path, '\0', MAX_NAME_LEN + 1);

    list = get_resource_info(*_rei->rsComm, resource_name_str);

    for (i = 0; i < json_array_size(list); i++) {
        json_t* json;

        json = json_object();
        json = json_array_get(list, i);
        strcpy(resource_id_str, json_string_value(json_object_get(json, "resc_id")));
        strcpy(resource_type, json_string_value(json_object_get(json, "resc_type")));
        strcpy(resource_vault_path, json_string_value(json_object_get(json, "resc_vault_path")));
    }

    // Convert resource ID C string to long value
    char* end_resource_id_str = end_resource_id_str;
    long resource_id = strtol(resource_id_str, &end_resource_id_str, 10);
    if (*end_resource_id_str != '\0') {
        rodsLog(LOG_ERROR, "msi_stat_vault: failed to convert resource ID %s to long value.", resource_id_str);
        return CAT_UNKNOWN_RESOURCE;
    }

    if (strcmp("unixfilesystem", resource_type) != 0 and strcmp("unix file system", resource_type) != 0) {
        rodsLog(LOG_ERROR,
                "msi_stat_vault: unable to stat files on resource %s. Not a unixfilesystem resource",
                resource_name_str);
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
