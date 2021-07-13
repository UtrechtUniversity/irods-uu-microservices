/**
 * \file
 * \brief     ArchiveExtract
 * \author    Felix Croes
 * \copyright Copyright (c) 2021, Wageningen University & Research
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "irods_includes.hh"

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iomanip>

extern "C" {

  int msiArchiveExtract(msParam_t* nameIn,
                        msParam_t* printOut,
                        ruleExecInfo_t *rei)
  {

    /* Check if user is privileged. */
    if (rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
      return SYS_USER_NO_PERMISSION;
    }

    /* Check input parameters. */
    if (strcmp(nameIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string name        = parseMspForStr(nameIn);

    std::string output      = "Hello "+name+"!";

    if(name.length() == 0){
       output      = "Hello World!";
    }
    std::cout << output;
    fillStrInMsParam(printOut, output.c_str());

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
