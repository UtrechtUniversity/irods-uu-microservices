/**
 * \file
 * \brief     iRODS microservice to generate a UUID.
 * \author    Felix Croes
 * \copyright Copyright (c) 2018, Utrecht University
 *
 * This file is part of irods-uu-microservices.
 *
 * irods-uu-microservices is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * irods-uu-microservices is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with irods-uu-microservices.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "irods_includes.hh"

//#include <fstream>
//#include <streambuf>
#include <uuid/uuid.h>

extern "C" {

  int msiGenerateUUID(msParam_t* UUIDOut,
		      ruleExecInfo_t *rei)
  {
    /* generate UUID. */
    uuid_t uuid;
    char buffer[37];
    uuid_generate(uuid);
    uuid_unparse_upper(uuid, buffer);
    fillStrInMsParam(UUIDOut, buffer);

    return 0;
  }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(1);

    msvc->add_operation<
        msParam_t*,
        ruleExecInfo_t*>("msiGenerateEpicPID",
                         std::function<int(
                             msParam_t*,
                             ruleExecInfo_t*)>(msiGenerateUUID));

    return msvc;
  }
}
