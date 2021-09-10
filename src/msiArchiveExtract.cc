/**
 * \file
 * \brief     ArchiveExtract
 * \author    Felix Croes
 * \copyright Copyright (c) 2021, Wageningen University & Research
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "irods_includes.hh"
#include "Archive.hh"
#include "rsModDataObjMeta.hpp"
#include "rsModAVUMetadata.hpp"

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iomanip>

static void modify(rsComm_t *rsComm, std::string file, json_t *json)
{
    modDataObjMeta_t modDataObj;
    dataObjInfo_t dataObjInfo;
    keyValPair_t regParam;
    char tmpStr[MAX_NAME_LEN];
    json_int_t modified;

    memset(&modDataObj, '\0', sizeof(modDataObjMeta_t));
    memset(&dataObjInfo, '\0', sizeof(dataObjInfo_t));
    memset(&regParam, '\0', sizeof(keyValPair_t));
    rstrcpy(dataObjInfo.objPath, file.c_str(), MAX_NAME_LEN);
    modDataObj.dataObjInfo = &dataObjInfo;
    modified = json_integer_value(json_object_get(json, "modified"));
    snprintf(tmpStr, MAX_NAME_LEN, "%lld", modified);
    addKeyVal(&regParam, DATA_MODIFY_KW, tmpStr);
    modDataObj.regParam = &regParam;
    rsModDataObjMeta(rsComm, &modDataObj);
}

static void attributes(rsComm_t *rsComm, std::string file, const char *type,
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
	name = json_string_value(json_object_get(json, "name"));

	modAVUInp.arg3 = (char *) json_string_value(json_object_get(json, "name"));
	modAVUInp.arg4 = (char *) json_string_value(json_object_get(json, "value"));
	modAVUInp.arg5 = (char *) json_string_value(json_object_get(json, "unit"));
	modAVUInp.arg0 = (strcmp(modAVUInp.arg3, name) == 0) ?
			  (char *) "add" : (char *) "set";
	name = modAVUInp.arg3;
	rsModAVUMetadata(rsComm, &modAVUInp);
    }
}

extern "C" {

  int msiArchiveExtract(msParam_t* archiveIn,
                        msParam_t* pathIn,
                        ruleExecInfo_t *rei)
  {
    collInp_t collCreateInp;
    json_t *json;

    /* Check input parameters. */
    if (strcmp(archiveIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(pathIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string archive = parseMspForStr(archiveIn);
    std::string path    = parseMspForStr(pathIn);

    memset(&collCreateInp, '\0', sizeof(collInp_t));
    rstrcpy(collCreateInp.collName, path.c_str(), MAX_NAME_LEN);
    rsCollCreate(rei->rsComm, &collCreateInp);

    Archive *a = Archive::open(rei->rsComm, archive);
    while ((json=a->nextItem()) != NULL) {
	std::string file;
	json_t *list;

	file = path + "/" + json_string_value(json_object_get(json, "name"));
	a->extractItem(file);
	list = json_object_get(json, "attributes");

	if (strcmp(json_string_value(json_object_get(json, "type")), "coll") == 0) {
	    if (list != NULL) {
		attributes(rei->rsComm, file, "-C", list);
	    }
	} else {
	    modify(rei->rsComm, file, json);
	    if (list != NULL) {
		attributes(rei->rsComm, file, "-d", list);
	    }
	}
    }
    delete a;

    return 0;
  }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(2);

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiArchiveExtract",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiArchiveExtract));

    return msvc;
  }
}
