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

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iomanip>

static long long collID(rsComm_t *rsComm, std::string coll)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *result;
    long long collId;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(collQCond, MAX_NAME_LEN, "='%s'", coll.c_str());
    addInxVal(&genQueryInp.sqlCondInp, COL_COLL_NAME, collQCond);
    addInxIval(&genQueryInp.selectInp, COL_COLL_ID, 1);
    genQueryInp.maxRows = 1;
    genQueryOut = NULL;
    rsGenQuery(rsComm, &genQueryInp, &genQueryOut);
    clearGenQueryInp(&genQueryInp);

    result = getSqlResultByInx(genQueryOut, COL_COLL_ID);
    collId = strtoll(result->value, NULL, 10);
    freeGenQueryOut(&genQueryOut);

    return collId;
}

static json_t *attrDataObj(rsComm_t *rsComm, long long id)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *names, *values, *units;
    char *name, *value, *unit;
    json_t *list, *json;

    memset(&genQueryInp, '\0', sizeof(genQueryInp_t));
    snprintf(collQCond, MAX_NAME_LEN, "='%lld'", id);
    addInxVal(&genQueryInp.sqlCondInp, COL_D_DATA_ID, collQCond);
    addInxIval(&genQueryInp.selectInp, COL_META_DATA_ATTR_NAME, 1);
    addInxIval(&genQueryInp.selectInp, COL_META_DATA_ATTR_VALUE, 1);
    addInxIval(&genQueryInp.selectInp, COL_META_DATA_ATTR_UNITS, 1);
    genQueryInp.maxRows = MAX_SQL_ROWS;
    genQueryOut = NULL;

    list = NULL;
    for (rsGenQuery(rsComm, &genQueryInp, &genQueryOut);
	 genQueryOut->rowCnt != 0;
	 rsGenQuery(rsComm, &genQueryInp, &genQueryOut)) {
	names = getSqlResultByInx(genQueryOut, COL_META_DATA_ATTR_NAME);
	values = getSqlResultByInx(genQueryOut, COL_META_DATA_ATTR_VALUE);
	units = getSqlResultByInx(genQueryOut, COL_META_DATA_ATTR_UNITS);

	for (int i = 0; i < genQueryOut->rowCnt; i++) {
	    name = &names->value[names->len * i];
	    value = &values->value[values->len * i];
	    unit = &units->value[units->len * i];
	    json = json_object();
	    json_object_set_new(json, "name", json_string(name));
	    json_object_set_new(json, "value", json_string(value));
	    json_object_set_new(json, "unit", json_string(unit));
	    if (list == NULL) {
		list = json_array();
	    }
	    json_array_append_new(list, json);
	}

	if (genQueryOut->continueInx > 0) {
	    genQueryInp.continueInx = genQueryOut->continueInx;
	} else {
	    genQueryInp.maxRows = -1;
	}
	freeGenQueryOut(&genQueryOut);
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);
    return list;
}

static json_t *attrColl(rsComm_t *rsComm, long long id)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *names, *values, *units;
    char *name, *value, *unit;
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
    for (rsGenQuery(rsComm, &genQueryInp, &genQueryOut);
	 genQueryOut->rowCnt != 0;
	 rsGenQuery(rsComm, &genQueryInp, &genQueryOut)) {
	names = getSqlResultByInx(genQueryOut, COL_META_COLL_ATTR_NAME);
	values = getSqlResultByInx(genQueryOut, COL_META_COLL_ATTR_VALUE);
	units = getSqlResultByInx(genQueryOut, COL_META_COLL_ATTR_UNITS);

	for (int i = 0; i < genQueryOut->rowCnt; i++) {
	    name = &names->value[names->len * i];
	    value = &values->value[values->len * i];
	    unit = &units->value[units->len * i];
	    json = json_object();
	    json_object_set_new(json, "name", json_string(name));
	    json_object_set_new(json, "value", json_string(value));
	    json_object_set_new(json, "unit", json_string(unit));
	    if (list == NULL) {
		list = json_array();
	    }
	    json_array_append_new(list, json);
	}

	if (genQueryOut->continueInx > 0) {
	    genQueryInp.continueInx = genQueryOut->continueInx;
	} else {
	    genQueryInp.maxRows = -1;
	}
	freeGenQueryOut(&genQueryOut);
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);
    return list;
}

static void dirDataObj(Archive *a, rsComm_t *rsComm, std::string coll, long long collId)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *names, *ids, *sizes, *owners, *zones, *ctimes, *mtimes, *checksums;
    long long id, size, ctime, mtime;
    char *name, *owner, *zone, *checksum;

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

    for (rsGenQuery(rsComm, &genQueryInp, &genQueryOut);
	 genQueryOut->rowCnt != 0;
	 rsGenQuery(rsComm, &genQueryInp, &genQueryOut)) {
	names = getSqlResultByInx(genQueryOut, COL_DATA_NAME);
	ids = getSqlResultByInx(genQueryOut, COL_D_DATA_ID);
	sizes = getSqlResultByInx(genQueryOut, COL_DATA_SIZE);
	owners = getSqlResultByInx(genQueryOut, COL_D_OWNER_NAME);
	zones = getSqlResultByInx(genQueryOut, COL_D_OWNER_ZONE);
	ctimes = getSqlResultByInx(genQueryOut, COL_D_CREATE_TIME);
	mtimes = getSqlResultByInx(genQueryOut, COL_D_MODIFY_TIME);
	checksums = getSqlResultByInx(genQueryOut, COL_D_DATA_CHECKSUM);

	for (int i = 0; i < genQueryOut->rowCnt; i++) {
	    name = &names->value[names->len * i];
	    id = strtoll(&ids->value[ids->len * i], NULL, 10);
	    size = strtoll(&sizes->value[sizes->len * i], NULL, 10);
	    owner = &owners->value[owners->len * i];
	    zone = &zones->value[zones->len * i];
	    ctime = strtoll(&ctimes->value[ctimes->len * i], NULL, 10);
	    mtime = strtoll(&mtimes->value[mtimes->len * i], NULL, 10);
	    checksum = &checksums->value[checksums->len * i];
	    a->addDataObj(coll + name, size, ctime, mtime, owner, zone, checksum, attrDataObj(rsComm, id));
	}

	if (genQueryOut->continueInx > 0) {
	    genQueryInp.continueInx = genQueryOut->continueInx;
	} else {
	    genQueryInp.maxRows = -1;
	}
	freeGenQueryOut(&genQueryOut);
    }

    freeGenQueryOut(&genQueryOut);
    freeGenQueryOut(&genQueryOut);
}

static void dirColl(Archive *a, rsComm_t *rsComm, std::string coll, std::string path)
{
    char collQCond[MAX_NAME_LEN];
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut;
    sqlResult_t *names, *ids, *owners, *zones, *ctimes, *mtimes;
    long long id, ctime, mtime;
    char *name, *owner, *zone;
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

    for (rsGenQuery(rsComm, &genQueryInp, &genQueryOut);
	 genQueryOut->rowCnt != 0;
	 rsGenQuery(rsComm, &genQueryInp, &genQueryOut)) {
	names = getSqlResultByInx(genQueryOut, COL_COLL_NAME);
	ids = getSqlResultByInx(genQueryOut, COL_COLL_ID);
	owners = getSqlResultByInx(genQueryOut, COL_COLL_OWNER_NAME);
	zones = getSqlResultByInx(genQueryOut, COL_COLL_OWNER_ZONE);
	ctimes = getSqlResultByInx(genQueryOut, COL_COLL_CREATE_TIME);
	mtimes = getSqlResultByInx(genQueryOut, COL_COLL_MODIFY_TIME);

	for (int i = 0; i < genQueryOut->rowCnt; i++) {
	    name = &names->value[names->len * i];
	    id = strtoll(&ids->value[ids->len * i], NULL, 10);
	    owner = &owners->value[owners->len * i];
	    zone = &zones->value[zones->len * i];
	    ctime = strtoll(&ctimes->value[ctimes->len * i], NULL, 10);
	    mtime = strtoll(&mtimes->value[mtimes->len * i], NULL, 10);
	    a->addColl(name + strlen(coll.c_str()) + 1, ctime, mtime, owner, zone, attrColl(rsComm, id));

	    dirs.push_back(std::make_pair(name, id));
	}

	if (genQueryOut->continueInx > 0) {
	    genQueryInp.continueInx = genQueryOut->continueInx;
	} else {
	    genQueryInp.maxRows = -1;
	}
	freeGenQueryOut(&genQueryOut);
    }

    clearGenQueryInp(&genQueryInp);
    freeGenQueryOut(&genQueryOut);

    for (auto dir = dirs.begin(); dir != dirs.end(); dir++) {
	dirColl(a, rsComm, coll, (*dir).first);
	dirDataObj(a, rsComm, (*dir).first.substr(coll.length() + 1) + "/", (*dir).second);
    }
}

extern "C" {

  int msiArchiveCreate(msParam_t* archiveIn,
                       msParam_t* collectionIn,
                       ruleExecInfo_t *rei)
  {
    long long id;

    /* Check input parameters. */
    if (strcmp(archiveIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(collectionIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string archive = parseMspForStr(archiveIn);
    std::string collection = parseMspForStr(collectionIn);

    id = collID(rei->rsComm, collection);
    Archive *a = Archive::create(rei->rsComm, archive, collection);
    dirColl(a, rei->rsComm, collection, collection);
    dirDataObj(a, rei->rsComm, "", id);
    delete a;

    return 0;
  }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(2);

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiArchiveCreate",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiArchiveCreate));

    return msvc;
  }
}
