/**
 * \file
 * \brief     ArchiveExtract
 * \author    Felix Croes
 * \copyright Copyright (c) 2021, Wageningen University & Research
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "irods_includes.hh"
#include "Archive.hh"

#include "rsGenQuery.hpp"
#include "rsModDataObjMeta.hpp"
#include "rsModAVUMetadata.hpp"


/*
 * obtain free space on resource, if set
 */
static long long freeSpace(rsComm_t *rsComm, const char *resource)
{
    char rescQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    long long space;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(rescQCond, MAX_NAME_LEN, "='%s'", resource);
    addInxVal(&genQueryInp.sqlCondInp, COL_R_RESC_NAME, rescQCond);
    addInxIval(&genQueryInp.selectInp, COL_R_FREE_SPACE, 1);
    genQueryInp.maxRows = 1;
    genQueryOut = NULL;
    space = rsGenQuery(rsComm, &genQueryInp, &genQueryOut);
    clearGenQueryInp(&genQueryInp);

    if (space == 0) {
        if (genQueryOut->rowCnt != 1) {
            space = SYS_RESC_DOES_NOT_EXIST;
        } else {
            /*
             * the resource exists, but the free space might still be unset
             */
            space = strtoll(getSqlResultByInx(genQueryOut,
                                              COL_R_FREE_SPACE)->value,
                            NULL, 10);
        }
    }
    freeGenQueryOut(&genQueryOut);

    return space;
}

/*
 * change the modification time for an extracted DataObj
 */
static void modify(rsComm_t *rsComm, std::string &file, json_t *json)
{
    modDataObjMeta_t modDataObj;
    dataObjInfo_t dataObjInfo;
    keyValPair_t regParam;
    char tmpStr[MAX_NAME_LEN];

    memset(&modDataObj, '\0', sizeof(modDataObjMeta_t));
    memset(&dataObjInfo, '\0', sizeof(dataObjInfo_t));
    memset(&regParam, '\0', sizeof(keyValPair_t));
    rstrcpy(dataObjInfo.objPath, file.c_str(), MAX_NAME_LEN);
    modDataObj.dataObjInfo = &dataObjInfo;
    snprintf(tmpStr, MAX_NAME_LEN, "%lld",
             json_integer_value(json_object_get(json, "modified")));
    addKeyVal(&regParam, DATA_MODIFY_KW, tmpStr);
    modDataObj.regParam = &regParam;
    rsModDataObjMeta(rsComm, &modDataObj);      /* allowed to fail */
}

/*
 * set the attributes of a collection or DataObj
 */
static void attributes(rsComm_t *rsComm, std::string &file, const char *type,
                       json_t *list)
{
    modAVUMetadataInp_t modAVUInp;
    size_t sz, i;
    const char *name;
    json_t *json;

    memset(&modAVUInp, '\0', sizeof(modAVUMetadataInp_t));
    modAVUInp.arg1 = (char *) type;
    modAVUInp.arg2 = (char *) file.c_str();
    name = NULL;
    sz = json_array_size(list);
    for (i = 0; i < sz; i++) {
        json = json_array_get(list, i);
        modAVUInp.arg3 = (char *)
                         json_string_value(json_object_get(json, "name"));
        modAVUInp.arg4 = (char *)
                         json_string_value(json_object_get(json, "value"));
        modAVUInp.arg5 = (char *)
                         json_string_value(json_object_get(json, "unit"));
        modAVUInp.arg0 = (name == NULL || strcmp(modAVUInp.arg3, name) != 0) ?
                          (char *) "set" : (char *) "add";
        name = modAVUInp.arg3;
        rsModAVUMetadata(rsComm, &modAVUInp);   /* allowed to fail */
    }
}

extern "C" {

int msiArchiveExtract(msParam_t* archiveIn,
                      msParam_t* pathIn,
                      msParam_t* extractIn,
                      msParam_t* resourceIn,
                      msParam_t* statusOut,
                      ruleExecInfo_t *rei)
{
    collInp_t collCreateInp;
    json_t *json;
    int status;
    long long space;

    /* Check input parameters. */
    if (archiveIn->type == NULL || strcmp(archiveIn->type, STR_MS_T)) {
        return SYS_INVALID_INPUT_PARAM;
    }
    if (pathIn->type == NULL || strcmp(pathIn->type, STR_MS_T)) {
        return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string archive  = parseMspForStr(archiveIn);
    std::string path     = parseMspForStr(pathIn);
    const char *extract = NULL;
    if (extractIn->type != NULL && strcmp(extractIn->type, STR_MS_T) == 0) {
        extract = parseMspForStr(extractIn);
    }
    const char *resource = NULL;
    if (resourceIn->type != NULL && strcmp(resourceIn->type, STR_MS_T) == 0) {
        resource = parseMspForStr(resourceIn);
    }

    Archive *a = Archive::open(rei->rsComm, archive, resource);
    if (a == NULL) {
        status = SYS_TAR_OPEN_ERR;
    } else {
        status = 0;
        space = 0;
        if (resource != NULL) {
            /*
             * see if there is enough free space
             */
            space = freeSpace(rei->rsComm, resource);
            if (space < 0) {
                status = (int) space;
            } else if (space != 0 && extract == NULL &&
                       a->size() > (size_t) (space - space / 10)) {
                /*
                 * the choice of status code is rather a shot in the dark
                 */
                status = SYS_RESC_QUOTA_EXCEEDED;
            }
            if (status < 0) {
                delete a;
                fillIntInMsParam(statusOut, status);
                return status;
            }
        }

        /*
         * create extraction location (allowed to fail)
         */
        memset(&collCreateInp, '\0', sizeof(collInp_t));
        rstrcpy(collCreateInp.collName, path.c_str(), MAX_NAME_LEN);
        rsCollCreate(rei->rsComm, &collCreateInp);

        /*
         * extract items
         */
        while ((json=a->nextItem()) != NULL) {
            std::string file;
            const char *type;
            json_t *list;

            file = json_string_value(json_object_get(json, "name"));
            if (extract == NULL || file.compare(extract) == 0) {
                if (extract != NULL) {
                    if (space != 0 &&
                       json_integer_value(json_object_get(json, "size")) >
                                                        space - space / 10) {
                        /*
                         * single-file space check failed
                         */
                        status = SYS_RESC_QUOTA_EXCEEDED;
                        break;
                    }

                    std::string::size_type found = file.rfind("/");
                    if (found != std::string::npos) {
                        file = file.substr(found + 1);
                    }
                }
                file = path + "/" + file;

                status = a->extractItem(file);
                if (status < 0) {
                    break;
                }

                /*
                 * Set metadata and attributes. This is subject to all sorts
                 * of policies, and thus allowed to fail.
                 */
                type = json_string_value(json_object_get(json, "type"));
                list = json_object_get(json, "attributes");
                if (strcmp(type, "coll") == 0) {
                    if (list != NULL) {
                        attributes(rei->rsComm, file, "-C", list);
                    }
                } else {
                    modify(rei->rsComm, file, json);
                    if (list != NULL) {
                        attributes(rei->rsComm, file, "-d", list);
                    }
                }

                if (extract != NULL) {
                    break;
                }
            }
        }
        delete a;
    }

    fillIntInMsParam(statusOut, status);
    return status;
}

irods::ms_table_entry *plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(5);

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        msParam_t*,
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiArchiveExtract",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiArchiveExtract));

    return msvc;
}

}
