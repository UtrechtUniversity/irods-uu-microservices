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

int calculate_checksum(rsComm_t* rsComm, std::string const& dataObjInp, std::string replNum, char* outChksum)
{
    char pathQCond[MAX_NAME_LEN];
    char replQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t* genQueryOut;
    char myColl[MAX_NAME_LEN], myData[MAX_NAME_LEN];
    sqlResult_t* path;
    std::string phyPath;
    int status;
    int ret = 0;

    memset(myColl, 0, MAX_NAME_LEN);
    memset(myData, 0, MAX_NAME_LEN);

    if ((status = splitPathByKey(dataObjInp.c_str(), myColl, MAX_NAME_LEN, myData, MAX_NAME_LEN, '/')) < 0) {
        ret = OBJ_PATH_DOES_NOT_EXIST;
    }

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));

    snprintf(pathQCond, MAX_NAME_LEN, "='%s'", myColl);
    addInxVal(&genQueryInp.sqlCondInp, COL_COLL_NAME, pathQCond);
    snprintf(pathQCond, MAX_NAME_LEN, "='%s'", myData);
    addInxVal(&genQueryInp.sqlCondInp, COL_DATA_NAME, pathQCond);
    snprintf(replQCond, MAX_NAME_LEN, "='%s'", replNum.c_str());
    addInxVal(&genQueryInp.sqlCondInp, COL_DATA_REPL_NUM, replQCond);

    addInxIval(&genQueryInp.selectInp, COL_D_DATA_PATH, 1);
    genQueryInp.maxRows = 1;
    genQueryOut = NULL;
    status = rsGenQuery(rsComm, &genQueryInp, &genQueryOut);

    if (status >= 0) {
        if (genQueryOut->rowCnt != 1) {
            rodsLog(LOG_ERROR, "_msi_dataObj_checksum: Unknown File");
            ret = CAT_UNKNOWN_FILE;
        }
        else {
            if ((path = getSqlResultByInx(genQueryOut, COL_D_DATA_PATH)) == NULL) {
                rodsLog(LOG_ERROR,
                        "_msi_dataObj_checksum: getSqlResultByInx for "
                        "COL_D_DATA_PATH failed");
                ret = UNMATCHED_KEY_OR_INDEX;
            }
            else {
                phyPath = path->value;
            }
        }
    }

    clearGenQueryInp(&genQueryInp);

    freeGenQueryOut(&genQueryOut);

    int retCode = chksumLocFile(phyPath.c_str(), outChksum, irods::SHA256_NAME.c_str());

    if (retCode < 0) {
        rodsLog(LOG_ERROR,
                "_msi_dataObj_checksum: Failed to calculate checksum for file: "
                "[{}], status = {}",
                myData,
                retCode);
        ret = retCode;
    }

    return ret;
}

extern "C" {

int msidataObjchecksum(msParam_t* dataObjInp, msParam_t* replNum, msParam_t* statusOut, ruleExecInfo_t* rei)
{
    std::string repl_num = parseMspForStr(replNum);
    std::string dataObj = parseMspForStr(dataObjInp);
    char chksum[NAME_LEN];

    int ret = calculate_checksum(rei->rsComm, dataObj, repl_num, chksum);

    if (ret < 0) {
        rodsLog(LOG_ERROR, "_msi_dataObj_checksum: Failed to calculate checksum");
    }
    else {
        fillStrInMsParam(statusOut, chksum);
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