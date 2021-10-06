/**
 * \file
 * \brief     ArchiveCreate
 * \author    Felix Croes
 * \copyright Copyright (c) 2021, Wageningen University & Research
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "irods_includes.hh"
#include "Archive.hh"

#include "rsGenQuery.hpp"

/*
 * obtain ID of a collection, or a negative error status
 */
static long long collID(rsComm_t *rsComm, std::string &coll)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    long long collId;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(collQCond, MAX_NAME_LEN, "='%s'", coll.c_str());
    addInxVal(&genQueryInp.sqlCondInp, COL_COLL_NAME, collQCond);
    addInxIval(&genQueryInp.selectInp, COL_COLL_ID, 1);
    genQueryInp.maxRows = 1;
    genQueryOut = NULL;
    collId = rsGenQuery(rsComm, &genQueryInp, &genQueryOut);
    clearGenQueryInp(&genQueryInp);

    if (collId == 0) {
        if (genQueryOut->rowCnt != 1) {
            collId = CAT_UNKNOWN_COLLECTION;
        } else {
            collId = strtoll(getSqlResultByInx(genQueryOut, COL_COLL_ID)->value,
                             NULL, 10);
        }
    }
    freeGenQueryOut(&genQueryOut);

    return collId;
}

/*
 * obtain attribute metadata for a DataObj
 */
static json_t *attrDataObj(rsComm_t *rsComm, long long id)
{
    char dataQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *names, *values, *units;
    json_t *list, *json;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(dataQCond, MAX_NAME_LEN, "='%lld'", id);
    addInxVal(&genQueryInp.sqlCondInp, COL_D_DATA_ID, dataQCond);
    addInxIval(&genQueryInp.selectInp, COL_META_DATA_ATTR_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_META_DATA_ATTR_VALUE, 1);
    addInxIval(&genQueryInp.selectInp, COL_META_DATA_ATTR_UNITS, 1);
    genQueryInp.maxRows = MAX_SQL_ROWS;
    genQueryOut = NULL;

    list = NULL;
    while (rsGenQuery(rsComm, &genQueryInp, &genQueryOut) == 0 &&
           genQueryOut->rowCnt != 0) {
        names = getSqlResultByInx(genQueryOut, COL_META_DATA_ATTR_NAME);
        values = getSqlResultByInx(genQueryOut, COL_META_DATA_ATTR_VALUE);
        units = getSqlResultByInx(genQueryOut, COL_META_DATA_ATTR_UNITS);

        for (int i = 0; i < genQueryOut->rowCnt; i++) {
            json = json_object();
            json_object_set_new(json, "name",
                                json_string(&names->value[names->len * i]));
            json_object_set_new(json, "value",
                                json_string(&values->value[values->len * i]));
            json_object_set_new(json, "unit",
                                json_string(&units->value[units->len * i]));
            if (list == NULL) {
                list = json_array();
            }
            json_array_append_new(list, json);
        }

        genQueryInp.continueInx = genQueryOut->continueInx;
        if (genQueryInp.continueInx == 0) {
            break;
        }
        freeGenQueryOut(&genQueryOut);
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);
    return list;
}

/*
 * obtain attribute metadata for a collection
 */
static json_t *attrColl(rsComm_t *rsComm, long long id)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *names, *values, *units;
    json_t *list, *json;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(collQCond, MAX_NAME_LEN, "='%lld'", id);
    addInxVal(&genQueryInp.sqlCondInp, COL_COLL_ID, collQCond);
    addInxIval(&genQueryInp.selectInp, COL_META_COLL_ATTR_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_META_COLL_ATTR_VALUE, 1);
    addInxIval(&genQueryInp.selectInp, COL_META_COLL_ATTR_UNITS, 1);
    genQueryInp.maxRows = MAX_SQL_ROWS;
    genQueryOut = NULL;

    list = NULL;
    while (rsGenQuery(rsComm, &genQueryInp, &genQueryOut) == 0 &&
           genQueryOut->rowCnt != 0) {
        names = getSqlResultByInx(genQueryOut, COL_META_COLL_ATTR_NAME);
        values = getSqlResultByInx(genQueryOut, COL_META_COLL_ATTR_VALUE);
        units = getSqlResultByInx(genQueryOut, COL_META_COLL_ATTR_UNITS);

        for (int i = 0; i < genQueryOut->rowCnt; i++) {
            json = json_object();
            json_object_set_new(json, "name",
                                json_string(&names->value[names->len * i]));
            json_object_set_new(json, "value",
                                json_string(&values->value[values->len * i]));
            json_object_set_new(json, "unit",
                                json_string(&units->value[units->len * i]));
            if (list == NULL) {
                list = json_array();
            }
            json_array_append_new(list, json);
        }

        genQueryInp.continueInx = genQueryOut->continueInx;
        if (genQueryInp.continueInx == 0) {
            break;
        }
        freeGenQueryOut(&genQueryOut);
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);
    return list;
}

/*
 * obtain ACLs for a DataObj
 */
static json_t *aclDataObj(rsComm_t *rsComm, long long id)
{
    char tmpStr[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *users, *zones, *access;
    json_t *list;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(tmpStr, MAX_NAME_LEN, "='%lld'", id);
    addInxVal(&genQueryInp.sqlCondInp, COL_DATA_ACCESS_DATA_ID, tmpStr);
    addInxVal(&genQueryInp.sqlCondInp, COL_DATA_TOKEN_NAMESPACE,
              "='access_type'");
    addInxIval(&genQueryInp.selectInp, COL_USER_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_USER_ZONE, 1);
    addInxIval(&genQueryInp.selectInp, COL_DATA_ACCESS_NAME, 1);
    genQueryInp.maxRows = MAX_SQL_ROWS;
    genQueryOut = NULL;

    list = NULL;
    while (rsGenQuery(rsComm, &genQueryInp, &genQueryOut) == 0 &&
           genQueryOut->rowCnt != 0) {
        users = getSqlResultByInx(genQueryOut, COL_USER_NAME);
        zones = getSqlResultByInx(genQueryOut, COL_USER_ZONE);
        access = getSqlResultByInx(genQueryOut, COL_DATA_ACCESS_NAME);

        for (int i = 0; i < genQueryOut->rowCnt; i++) {
            snprintf(tmpStr, MAX_NAME_LEN, "%s#%s:%s",
                     &users->value[users->len * i],
                     &zones->value[zones->len * i],
                     &access->value[access->len * i]);
            if (list == NULL) {
                list = json_array();
            }
            json_array_append_new(list, json_string(tmpStr));
        }

        genQueryInp.continueInx = genQueryOut->continueInx;
        if (genQueryInp.continueInx == 0) {
            break;
        }
        freeGenQueryOut(&genQueryOut);
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);
    return list;
}

/*
 * obtain ACLs for a collection
 */
static json_t *aclColl(rsComm_t *rsComm, long long id)
{
    char tmpStr[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *users, *zones, *access;
    json_t *list;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(tmpStr, MAX_NAME_LEN, "='%lld'", id);
    addInxVal(&genQueryInp.sqlCondInp, COL_COLL_ACCESS_COLL_ID, tmpStr);
    addInxVal(&genQueryInp.sqlCondInp, COL_COLL_TOKEN_NAMESPACE,
              "='access_type'");
    addInxIval(&genQueryInp.selectInp, COL_COLL_USER_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_COLL_USER_ZONE, 1);
    addInxIval(&genQueryInp.selectInp, COL_COLL_ACCESS_NAME, 1);
    genQueryInp.maxRows = MAX_SQL_ROWS;
    genQueryOut = NULL;

    list = NULL;
    while (rsGenQuery(rsComm, &genQueryInp, &genQueryOut) == 0 &&
           genQueryOut->rowCnt != 0) {
        users = getSqlResultByInx(genQueryOut, COL_COLL_USER_NAME);
        zones = getSqlResultByInx(genQueryOut, COL_COLL_USER_ZONE);
        access = getSqlResultByInx(genQueryOut, COL_COLL_ACCESS_NAME);

        for (int i = 0; i < genQueryOut->rowCnt; i++) {
            snprintf(tmpStr, MAX_NAME_LEN, "%s#%s:%s",
                     &users->value[users->len * i],
                     &zones->value[zones->len * i],
                     &access->value[access->len * i]);
            if (list == NULL) {
                list = json_array();
            }
            json_array_append_new(list, json_string(tmpStr));
        }

        genQueryInp.continueInx = genQueryOut->continueInx;
        if (genQueryInp.continueInx == 0) {
            break;
        }
        freeGenQueryOut(&genQueryOut);
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);
    return list;
}

/*
 * pass on metadata from DataObjs in a given location to the archive
 */
static void dirDataObj(Archive *a, rsComm_t *rsComm, std::string coll,
                       long long collId)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *names, *ids, *sizes, *owners, *zones, *ctimes, *mtimes,
                *checksums;
    long long dataId;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(collQCond, MAX_NAME_LEN, "='%lld'", collId);
    addInxVal(&genQueryInp.sqlCondInp, COL_D_COLL_ID, collQCond);
    addInxIval(&genQueryInp.selectInp, COL_DATA_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_D_DATA_ID, 1);
    addInxIval(&genQueryInp.selectInp, COL_DATA_SIZE, 1);
    addInxIval(&genQueryInp.selectInp, COL_D_OWNER_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_D_OWNER_ZONE, 1);
    addInxIval(&genQueryInp.selectInp, COL_D_CREATE_TIME, 1);
    addInxIval(&genQueryInp.selectInp, COL_D_MODIFY_TIME, 1);
    addInxIval(&genQueryInp.selectInp, COL_D_DATA_CHECKSUM, 1);
    genQueryInp.maxRows = MAX_SQL_ROWS;
    genQueryOut = NULL;

    while (rsGenQuery(rsComm, &genQueryInp, &genQueryOut) == 0 &&
           genQueryOut->rowCnt != 0) {
        names = getSqlResultByInx(genQueryOut, COL_DATA_NAME);
        ids = getSqlResultByInx(genQueryOut, COL_D_DATA_ID);
        sizes = getSqlResultByInx(genQueryOut, COL_DATA_SIZE);
        owners = getSqlResultByInx(genQueryOut, COL_D_OWNER_NAME);
        zones = getSqlResultByInx(genQueryOut, COL_D_OWNER_ZONE);
        ctimes = getSqlResultByInx(genQueryOut, COL_D_CREATE_TIME);
        mtimes = getSqlResultByInx(genQueryOut, COL_D_MODIFY_TIME);
        checksums = getSqlResultByInx(genQueryOut, COL_D_DATA_CHECKSUM);

        for (int i = 0; i < genQueryOut->rowCnt; i++) {
            dataId = strtoll(&ids->value[ids->len * i], NULL, 10);
            a->addDataObj(coll + &names->value[names->len * i],
                          (size_t) strtoll(&sizes->value[sizes->len * i], NULL,
                                           10),
                          strtoll(&ctimes->value[ctimes->len * i], NULL, 10),
                          strtoll(&mtimes->value[mtimes->len * i], NULL, 10),
                          &owners->value[owners->len * i],
                          &zones->value[zones->len * i],
                          &checksums->value[checksums->len * i],
                          attrDataObj(rsComm, dataId),
                          aclDataObj(rsComm, dataId));
        }

        genQueryInp.continueInx = genQueryOut->continueInx;
        if (genQueryInp.continueInx == 0) {
            break;
        }
        freeGenQueryOut(&genQueryOut);
    }

    freeGenQueryOut(&genQueryOut);
    freeGenQueryOut(&genQueryOut);
}

/*
 * recursively pass on metadata for collections to the archive
 */
static void dirColl(Archive *a, rsComm_t *rsComm, std::string &coll,
                    std::string &path)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *names, *ids, *owners, *zones, *ctimes, *mtimes;
    long long id;
    char *name;
    std::list<std::pair<std::string, long long>> dirs = { };

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(collQCond, MAX_NAME_LEN, "='%s'", path.c_str());
    addInxVal(&genQueryInp.sqlCondInp, COL_COLL_PARENT_NAME, collQCond);
    addInxIval(&genQueryInp.selectInp, COL_COLL_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_COLL_ID, 1);
    addInxIval(&genQueryInp.selectInp, COL_COLL_OWNER_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_COLL_OWNER_ZONE, 1);
    addInxIval(&genQueryInp.selectInp, COL_COLL_CREATE_TIME, 1);
    addInxIval(&genQueryInp.selectInp, COL_COLL_MODIFY_TIME, 1);
    genQueryInp.maxRows = MAX_SQL_ROWS;
    genQueryOut = NULL;

    while (rsGenQuery(rsComm, &genQueryInp, &genQueryOut) == 0 &&
           genQueryOut->rowCnt != 0) {
        names = getSqlResultByInx(genQueryOut, COL_COLL_NAME);
        ids = getSqlResultByInx(genQueryOut, COL_COLL_ID);
        owners = getSqlResultByInx(genQueryOut, COL_COLL_OWNER_NAME);
        zones = getSqlResultByInx(genQueryOut, COL_COLL_OWNER_ZONE);
        ctimes = getSqlResultByInx(genQueryOut, COL_COLL_CREATE_TIME);
        mtimes = getSqlResultByInx(genQueryOut, COL_COLL_MODIFY_TIME);

        for (int i = 0; i < genQueryOut->rowCnt; i++) {
            name = &names->value[names->len * i];
            id = strtoll(&ids->value[ids->len * i], NULL, 10);
            a->addColl(name + strlen(coll.c_str()) + 1,
                       strtoll(&ctimes->value[ctimes->len * i], NULL, 10),
                       strtoll(&mtimes->value[mtimes->len * i], NULL, 10),
                       &owners->value[owners->len * i],
                       &zones->value[zones->len * i],
                       attrColl(rsComm, id),
                       aclColl(rsComm, id));

            /*
             * maintain a list of collections to recursively query
             */
            dirs.push_back(std::make_pair(name, id));
        }

        genQueryInp.continueInx = genQueryOut->continueInx;
        if (genQueryInp.continueInx == 0) {
            break;
        }
        freeGenQueryOut(&genQueryOut);
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);

    /*
     * Also add what's inside those collections. This is done separately so
     * that the maximum number of open queries is not exceeded.
     */
    for (auto dir = dirs.begin(); dir != dirs.end(); dir++) {
        dirColl(a, rsComm, coll, dir->first);
        dirDataObj(a, rsComm, dir->first.substr(coll.length() + 1) + "/",
                   dir->second);
    }
}

extern "C" {

int msiArchiveCreate(msParam_t *archiveIn,
                     msParam_t *collectionIn,
                     msParam_t *resourceIn,
                     msParam_t *statusOut,
                     ruleExecInfo_t *rei)
{
    long long id;
    int status;

    /* Check input parameters. */
    if (archiveIn->type == NULL || strcmp(archiveIn->type, STR_MS_T)) {
        return SYS_INVALID_INPUT_PARAM;
    }
    if (collectionIn->type == NULL || strcmp(collectionIn->type, STR_MS_T)) {
        return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string archive    = parseMspForStr(archiveIn);
    std::string collection = parseMspForStr(collectionIn);
    const char *resource   = NULL;
    if (resourceIn->type != NULL && strcmp(resourceIn->type, STR_MS_T) == 0) {
        resource = parseMspForStr(resourceIn);
    }

    id = collID(rei->rsComm, collection);
    if (id < 0) {
        /*
         * no such collection
         */
        status = (int) id;
    } else {
        /*
         * create archive
         */
        Archive *a = Archive::create(rei->rsComm, archive, collection,
                                     resource);
        if (a == NULL) {
            status = SYS_TAR_OPEN_ERR;
        } else {
            /*
             * add collections and DataObjs to archive
             */
            dirColl(a, rei->rsComm, collection, collection);
            dirDataObj(a, rei->rsComm, "", id);

            /*
             * actually construct the archive
             */
            status = a->construct();
            delete a;
        }
    }

    fillIntInMsParam(statusOut, status);
    return status;
}

irods::ms_table_entry *plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(4);

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiArchiveCreate",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiArchiveCreate));

    return msvc;
}

}
