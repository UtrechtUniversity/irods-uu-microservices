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
#include "irods/SHA256Strategy.hpp"
#include "irods/checksum.h"
#include "irods/dataObjInpOut.h"
#include "irods/rcMisc.h"
#include "irods/rods.h"
#include "rsGenQuery.hpp"

#include "irods_includes.hh"

#include <cstring>
#include <irods_error.hpp>
#include <stdio.h>
#include <string.h>

int calculate_checksum(rsComm_t* rsComm, std::string const& dataObjInp, std::string replNumInp, char* checksumOut)
{
    char pathQCond[MAX_NAME_LEN];
    char replQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t* genQueryOut;
    char coll[MAX_NAME_LEN], dataObj[MAX_NAME_LEN];
    sqlResult_t* path;
    std::string phyPath;
    int status;
    int ret = 0;

    memset(coll, 0, MAX_NAME_LEN);
    memset(dataObj, 0, MAX_NAME_LEN);

    if ((status = splitPathByKey(dataObjInp.c_str(), coll, MAX_NAME_LEN, dataObj, MAX_NAME_LEN, '/')) < 0) {
        ret = OBJ_PATH_DOES_NOT_EXIST;
    }

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));

    // Build query to find data object replica.
    snprintf(pathQCond, MAX_NAME_LEN, "='%s'", coll);
    addInxVal(&genQueryInp.sqlCondInp, COL_COLL_NAME, pathQCond);
    snprintf(pathQCond, MAX_NAME_LEN, "='%s'", dataObj);
    addInxVal(&genQueryInp.sqlCondInp, COL_DATA_NAME, pathQCond);
    snprintf(replQCond, MAX_NAME_LEN, "='%s'", replNumInp.c_str());
    addInxVal(&genQueryInp.sqlCondInp, COL_DATA_REPL_NUM, replQCond);
    addInxIval(&genQueryInp.selectInp, COL_D_DATA_PATH, 1);

    // Execute query to find data object replica.
    genQueryInp.maxRows = 1;
    genQueryOut = NULL;
    status = rsGenQuery(rsComm, &genQueryInp, &genQueryOut);
    if (status >= 0) {
        if (genQueryOut->rowCnt != 1) {
            rodsLog(LOG_ERROR, "msi_dataObj_checksum: unknown file");
            ret = CAT_UNKNOWN_FILE;
        }
        else {
            if ((path = getSqlResultByInx(genQueryOut, COL_D_DATA_PATH)) == NULL) {
                rodsLog(LOG_ERROR, "msi_dataObj_checksum: getSqlResultByInx for COL_D_DATA_PATH failed");
                ret = UNMATCHED_KEY_OR_INDEX;
            }
            else {
                phyPath = path->value;
            }
        }
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);

    // Compute checksum of data object replica.
    int retCode = chksumLocFile(phyPath.c_str(), checksumOut, irods::SHA256_NAME.c_str());

    if (retCode < 0) {
        rodsLog(LOG_ERROR,
                "msi_dataObj_checksum: failed to calculate checksum for file: [{}], status = {}",
                dataObj,
                retCode);
        ret = retCode;
    }

    return ret;
}

extern "C" {

int msidataObjchecksum(msParam_t* dataObjInp, msParam_t* replNumInp, msParam_t* statusOut, ruleExecInfo_t* rei)
{
    // Parse input parameters.
    std::string repl_num = parseMspForStr(replNumInp);
    std::string dataObj = parseMspForStr(dataObjInp);

    char checksum[NAME_LEN];
    int ret = calculate_checksum(rei->rsComm, dataObj, repl_num, checksum);

    if (ret < 0) {
        rodsLog(LOG_ERROR, "msi_dataObj_checksum: failed to calculate checksum");
    }
    else {
        fillStrInMsParam(statusOut, checksum);
    }

    return ret;
}

irods::ms_table_entry* plugin_factory()
{
    irods::ms_table_entry* msvc = new irods::ms_table_entry(3);

    msvc->add_operation<msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*>(
        "msidataObjchecksum",
        std::function<int(msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*)>(msidataObjchecksum));
    return msvc;
}
}
