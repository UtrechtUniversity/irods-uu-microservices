/**
 * \file
 * \brief     ArchiveIndex
 * \author    Felix Croes
 * \copyright Copyright (c) 2021, Wageningen University & Research
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "irods_includes.hh"
#include "Archive.hh"

extern "C" {

  int msiArchiveIndex(msParam_t* archiveIn,
                      msParam_t* indexOut,
                      ruleExecInfo_t *rei)
  {
    /* Check input parameters. */
    if (archiveIn->type == NULL || strcmp(archiveIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string archive = parseMspForStr(archiveIn);

    Archive *a = Archive::open(rei->rsComm, archive, "");
    if (a == NULL) {
	return SYS_TAR_OPEN_ERR;
    }
    fillStrInMsParam(indexOut, a->indexItems().c_str());
    delete a;

    return 0;
  }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(2);

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiArchiveIndex",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiArchiveIndex));

    return msvc;
  }
}
