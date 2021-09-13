/**
 * \file
 * \brief     ArchiveIndex
 * \author    Felix Croes
 * \copyright Copyright (c) 2021, Wageningen University & Research
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "irods_includes.hh"
#include "Archive.hh"

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iomanip>

extern "C" {

  int msiArchiveIndex(msParam_t* archiveIn,
                      msParam_t* indexOut,
                      ruleExecInfo_t *rei)
  {
    /* Check input parameters. */
    if (strcmp(archiveIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string archive = parseMspForStr(archiveIn);

    Archive *a = Archive::open(rei->rsComm, archive, "");
    std::string output = a->indexItems();
    delete a;
    fillStrInMsParam(indexOut, output.c_str());

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
