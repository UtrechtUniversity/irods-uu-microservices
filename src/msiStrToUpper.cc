/**
 * \file
 * \brief     iRODS microservice to convert a string to uppercase.
 * \author    Lazlo Westerhof
 * \copyright Copyright (c) 2017, Utrecht University
 *
 * Copyright (c) 2017, Utrecht University
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
#include "reGlobalsExtern.hpp"
#include "genQuery.h"

#include <codecvt>
#include <iostream>
#include <locale>


std::locale const utf8("en_US.UTF-8");

/* Convert UTF-8 byte string to wstring. */
std::wstring toWstring( std::string const& s ) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
  return conv.from_bytes(s);
}

/* Convert wstring to UTF-8 byte string. */
std::string toString( std::wstring const& s ) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
  return conv.to_bytes(s);
}

/* Converts a UTF-8 encoded string to upper case. */
std::string toUpper( std::string const& s ) {
  auto ss = toWstring(s);
  for (auto& c : ss) {
    c = std::toupper(c, utf8);
  }
  return toString(ss);
}

extern "C" {
  int msiStrToUpper( msParam_t* in, msParam_t* out, ruleExecInfo_t* rei ) {
        std::string inStr = parseMspForStr( in );

        std::string outStr = toUpper(inStr);

        fillStrInMsParam(out, outStr.c_str());

        return 0;
  }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(2);

    msvc->add_operation("msiStrToUpper", "msiStrToUpper");

    return msvc;
  }
}
