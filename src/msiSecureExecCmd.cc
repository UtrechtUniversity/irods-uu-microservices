/**
 * \file
 * \brief     iRODS microservice to securely execute a command in the msiExecCmd_bin1 directory.
 * \author    Felix Croes
 * \copyright Copyright (c) 2018, Utrecht University
 *
 * This file, part of irods-uu-microservices, is derived from original iRODS
 * source code.
 *
 *
 * Copyright (c) 2005-2016, Regents of the University of California, the University of North Carolina at Chapel Hill, and the Data Intensive Cyberinfrastructure Foundation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the University of California, San Diego (UCSD), the University of North Carolina at Chapel Hill nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "irods_includes.hh"
#include <dataObjOpr.hpp>
#include <icatDefines.h>
#include <rsExecCmd.hpp>
#include <irods_resource_backport.hpp>
#include <irods_resource_redirect.hpp>


static int
rsSecureExecCmd( rsComm_t *rsComm, execCmd_t *execCmdInp, execCmdOut_t **execCmdOut ) {
    int status;
    dataObjInfo_t *dataObjInfoHead = NULL;
    rodsServerHost_t *rodsServerHost;
    int remoteFlag;
    rodsHostAddr_t addr;
    irods::error err = SUCCESS();

    /* Also check for anonymous.  As an additional safety precaution,
       by default, do not allow the anonymous user (if defined) to
       execute commands via rcExecCmd.  If your site needs to allow
       this for some particular feature, you can remove the
       following check.
    */
    if ( strncmp( ANONYMOUS_USER, rsComm->clientUser.userName, NAME_LEN ) == 0 ) {
        return USER_NOT_ALLOWED_TO_EXEC_CMD;
    }

    memset( &addr, 0, sizeof( addr ) );
    if ( *execCmdInp->hintPath != '\0' ) {
        dataObjInp_t dataObjInp;
        memset( &dataObjInp, 0, sizeof( dataObjInp ) );
        rstrcpy( dataObjInp.objPath, execCmdInp->hintPath, MAX_NAME_LEN );

        // =-=-=-=-=-=-=-
        // determine the resource hierarchy if one is not provided
        std::string resc_hier;
        char*       resc_hier_ptr = getValByKey( &dataObjInp.condInput, RESC_HIER_STR_KW );
        if ( resc_hier_ptr == NULL ) {
            irods::error ret = irods::resolve_resource_hierarchy( irods::OPEN_OPERATION,
                               rsComm, &dataObjInp, resc_hier );
            if ( !ret.ok() ) {
                std::stringstream msg;
                msg << "failed in irods::resolve_resource_hierarchy for [";
                msg << dataObjInp.objPath << "]";
                irods::log( PASSMSG( msg.str(), ret ) );
                return (int) ret.code();
            }

            // =-=-=-=-=-=-=-
            // we resolved the redirect and have a host, set the hier str for subsequent
            // api calls, etc.
            addKeyVal( &dataObjInp.condInput, RESC_HIER_STR_KW, resc_hier.c_str() );
            addKeyVal( &execCmdInp->condInput, RESC_HIER_STR_KW, resc_hier.c_str() );
        }
        else {
            resc_hier = resc_hier_ptr;

        }

        status = getDataObjInfo( rsComm, &dataObjInp, &dataObjInfoHead, (char *) ACCESS_READ_OBJECT, 0 );
        if ( status < 0 ) {
            rodsLog( LOG_ERROR,
                     "rsSecureExecCmd: getDataObjInfo error for hintPath %s",
                     execCmdInp->hintPath );
            return status;
        }

        status = sortObjInfoForOpen( &dataObjInfoHead, &execCmdInp->condInput, 0 );

        if ( status < 0 || NULL == dataObjInfoHead ) {
            return status;    // JMC cppcheck nullptr
        }

        if ( execCmdInp->addPathToArgv > 0 ) {
            char tmpArgv[HUGE_NAME_LEN];
            rstrcpy( tmpArgv, execCmdInp->cmdArgv, HUGE_NAME_LEN );
            snprintf( execCmdInp->cmdArgv, HUGE_NAME_LEN, "%s %s",
                      dataObjInfoHead->filePath, tmpArgv );
        }

        // =-=-=-=-=-=-=-
        // extract the host location from the resource hierarchy
        std::string location;
        err = irods::get_loc_for_hier_string( dataObjInfoHead->rescHier, location );
        if ( !err.ok() ) {
            irods::log( PASSMSG( "rsSecureExecCmd - failed in get_loc_for_hier_string", err ) );
            return (int) err.code();
        }

        // =-=-=-=-=-=-=-
        // extract zone name from resource hierarchy
        std::string zone_name;
        err = irods::get_resource_property<std::string>( dataObjInfoHead->rescId, irods::RESOURCE_ZONE, zone_name );
        if ( !err.ok() ) {
            irods::log( PASSMSG( "rsSecureExecCmd - failed in get_resc_hier_property", err ) );
            return (int) err.code();
        }

        rstrcpy( addr.zoneName, zone_name.c_str(), NAME_LEN );
        rstrcpy( addr.hostAddr, location.c_str(), LONG_NAME_LEN );

        /* just in case we have to do it remote */
        *execCmdInp->hintPath = '\0';	/* no need for hint */
        rstrcpy( execCmdInp->execAddr, location.c_str(),
                 LONG_NAME_LEN );
        freeAllDataObjInfo( dataObjInfoHead );
        remoteFlag = resolveHost( &addr, &rodsServerHost );
    }
    else if ( *execCmdInp->execAddr  != '\0' ) {
        rstrcpy( addr.hostAddr, execCmdInp->execAddr, LONG_NAME_LEN );
        remoteFlag = resolveHost( &addr, &rodsServerHost );
    }
    else {
        rodsServerHost = LocalServerHost;
        remoteFlag = LOCAL_HOST;
    }

    if ( remoteFlag == LOCAL_HOST ) {
        status = _rsExecCmd( execCmdInp, execCmdOut );
    }
    else if ( remoteFlag == REMOTE_HOST ) {
        status = remoteExecCmd( rsComm, execCmdInp, execCmdOut,
                                rodsServerHost );
    }
    else {
        rodsLog( LOG_NOTICE,
                 "rsSecureExecCmd: resolveHost of %s error, status = %d",
                 addr.hostAddr, remoteFlag );
        status = SYS_UNRECOGNIZED_REMOTE_FLAG;
    }

    return status;
}

extern "C" {
  static int
  msiSecureExecCmd( msParam_t *inpParam1, msParam_t *inpParam2,
		    msParam_t *inpParam3, msParam_t *inpParam4,
		    msParam_t *inpParam5, msParam_t *outParam,
		    ruleExecInfo_t *rei ) {
    rsComm_t *rsComm;
    execCmd_t execCmdInp, *myExecCmdInp;
    execCmdOut_t *execCmdOut = NULL;
    char *tmpPtr;

    if ( rei == NULL || rei->rsComm == NULL ) {
        rodsLog( LOG_ERROR,
                 "msiSecureExecCmd: input rei or rsComm is NULL" );
        return SYS_INTERNAL_NULL_INPUT_ERR;
    }

    rsComm = rei->rsComm;

    /* parse inpParam1 */
    rei->status = parseMspForExecCmdInp( inpParam1, &execCmdInp,
                                         &myExecCmdInp );

    if ( rei->status < 0 ) {
        rodsLogAndErrorMsg( LOG_ERROR, &rsComm->rError, rei->status,
                            "msiSecureExecCmd: input inpParam1 error. status = %d", rei->status );
        return rei->status;
    }

    char cmd[LONG_NAME_LEN];
    rstrcpy(cmd, myExecCmdInp->cmd, LONG_NAME_LEN);

    /* some sanity check on the cmd path */
    if ( strchr( cmd, '/' ) != NULL ) {
	rodsLog( LOG_ERROR, "rsSecureExecCmd: bad cmd path %s", cmd );
	return BAD_EXEC_CMD_PATH;
    }

    snprintf(myExecCmdInp->cmd, LONG_NAME_LEN, "../msiExecCmd_bin1/%s", cmd);

    const char *args[4];
    args[0] = cmd;

    if ( ( tmpPtr = parseMspForStr( inpParam2 ) ) != NULL ) {
        rstrcpy( myExecCmdInp->cmdArgv, tmpPtr, HUGE_NAME_LEN );
	args[1] = myExecCmdInp->cmdArgv;
    } else {
	args[1] = "";
    }

    if ( ( tmpPtr = parseMspForStr( inpParam3 ) ) != NULL ) {
        rstrcpy( myExecCmdInp->execAddr, tmpPtr, LONG_NAME_LEN );
	args[2] = myExecCmdInp->execAddr;
    } else {
	args[2] = "";
    }

    if ( ( tmpPtr = parseMspForStr( inpParam4 ) ) != NULL ) {
        rstrcpy( myExecCmdInp->hintPath, tmpPtr, MAX_NAME_LEN );
	args[3] = myExecCmdInp->hintPath;
    } else {
	args[3] = "";
    }

    if ( parseMspForPosInt( inpParam5 ) > 0 ) {
        myExecCmdInp->addPathToArgv = 1;
    }

    if ( strlen( rei->ruleName ) > 0 &&
            strcmp( rei->ruleName, EXEC_MY_RULE_KW ) != 0 ) {
        /* does not come from rsExecMyRule */
        addKeyVal( &myExecCmdInp->condInput, EXEC_CMD_RULE_KW, rei->ruleName );
    }

    int status = applyRuleArg ("acPreProcForExecCmd", args, 4, rei, NO_SAVE_REI);
    if (status < 0) {
        rodsLog( LOG_ERROR, "msiSecureExecCmd: acPreProcForExecCmd error, status = %d", status);
        return (status);
    }

    rei->status = rsSecureExecCmd( rsComm, myExecCmdInp, &execCmdOut );

    if ( myExecCmdInp == &execCmdInp ) {
        clearKeyVal( &myExecCmdInp->condInput );
    }

    if ( execCmdOut != NULL ) {	/* something was written to it */
        fillMsParam( outParam, NULL, ExecCmdOut_MS_T, execCmdOut, NULL );
    }

    if ( rei->status < 0 ) {
        rodsLogAndErrorMsg( LOG_ERROR, &rsComm->rError, rei->status,
                            "msiSecureExecCmd: rsExecCmd failed for %s, status = %d",
                            myExecCmdInp->cmd,
                            rei->status );
    }

    return rei->status;
  }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(6);
    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        msParam_t*,
        msParam_t*,
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiSecureExecCmd",
                         std::function<int(
                             msParam_t*,
			     msParam_t*,
			     msParam_t*,
			     msParam_t*,
			     msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiSecureExecCmd));

    return msvc;
  }
}
