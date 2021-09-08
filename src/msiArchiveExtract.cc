/**
 * \file
 * \brief     ArchiveExtract
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

  int msiArchiveExtract(msParam_t* archiveIn,
                        msParam_t* pathIn,
                        ruleExecInfo_t *rei)
  {
    collInp_t collCreateInp;

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
    std::string file;

    memset(&collCreateInp, '\0', sizeof(collInp_t));
    rstrcpy(collCreateInp.collName, path.c_str(), MAX_NAME_LEN);
    rsCollCreate(rei->rsComm, &collCreateInp);

    Archive *a = Archive::open(rei->rsComm, archive);
    while ((file=a->nextItem()).length() != 0) {
	a->extractItem(path + "/" + file);
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
