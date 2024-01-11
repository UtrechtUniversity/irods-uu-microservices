/*

 * This microservice performs a stat on a filename within a unixfilesystem
 * resource vault, checking whether the filename refers to an existing file
 * and returning its size if present. It can be used to help verify integrity
 * of data objects.
 *
 * Example code to invoke this microservice:
 *
 *
 * # \file
 * # \brief job
 * # \author Sietse Snel
 * # \copyright Copyright (c) 2024, Utrecht university. All rights reserved
 * # \license GPLv3, see LICENSE
 * #
 * # Run vault stat service for testing
 * #
 *
 *
 * runVaultStat {
         * *filename='/etc/hostname';
         * *rescname='demoResc';
         * *exists='';
         * *size='';
         * writeLine("serverLog","Running vault stat for *filename");
         * msi_stat_vault(*filename, *rescname, *exists, *size);
         * writeLine("serverLog","Ran vault stat for *filename");
 *
 * }
 *
 * input null
 * output ruleExecOut
 *
 */

#include "irods_error.hpp"
#include "irods_log.hpp"
#include "irods_ms_plugin.hpp"
#include "irods/resource_administration.hpp"
#include <string_view>

int msiStatVault(
    msParam_t* _resource_name,
    msParam_t* _physical_path_name,
    msParam_t* _exists,
    msParam_t* _size,
    ruleExecInfo_t* _rei ) {

    char *resource_name_str = parseMspForStr( _resource_name );
    if( !resource_name_str ) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char *physical_path_str = parseMspForStr( _physical_path_name );
    if( !physical_path_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char *exists_str = parseMspForStr( _exists );
    if( !exists_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char *size_str = parseMspForStr( _size );
    if( !size_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    /* Check that user is rodsadmin
    */
    if (_rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
      return SYS_USER_NO_PERMISSION;
    }

    /*
     * std::string_view resource_name_view = sv(resource_name_str);
     * bool resc_exists = resource_exists(_rei->rsComm, resource_name_view);
    */

    rodsLog(LOG_NOTICE, "msiStatVault is running!");

    _rei->status = 1;

    return _rei->status;

}

extern "C"
irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry* msvc = new irods::ms_table_entry(4);

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiStatVault",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiStatVault));
    return msvc;
}
