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

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iomanip>

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

	file = path + "/" + json_string_value(json_object_get(json, "name"));
	a->extractItem(file);

	if (strcmp(json_string_value(json_object_get(json, "type")), "dataObj") == 0) {
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
	    rsModDataObjMeta(rei->rsComm, &modDataObj);
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
