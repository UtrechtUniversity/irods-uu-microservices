/**
 * \file
 * \brief     iRODS microservice to register a DOI with DataCite.
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

#include <string>
#include <fstream>
#include <streambuf>
#include <curl/curl.h>

extern "C" {
  int msiRegisterDataCiteDOI(msParam_t* urlIn,
			     msParam_t* usernameIn,
			     msParam_t* passwordIn,
			     msParam_t* xmlIn,
			     ruleExecInfo_t *rei)
  {
    CURL *curl;
    CURLcode res;

    /* Check if user is priviliged. */
    if (rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
      return SYS_USER_NO_PERMISSION;
    }

    /* Check input parameters. */
    if (strcmp(urlIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(usernameIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(passwordIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(xmlIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string url      = parseMspForStr(urlIn);
    std::string username = parseMspForStr(usernameIn);
    std::string password = parseMspForStr(passwordIn);
    std::string xml      = parseMspForStr(xmlIn);

    /* Read XML file. */
    std::ifstream t(xml);
    std::string xmlStream((std::istreambuf_iterator<char>(t)),
                           std::istreambuf_iterator<char>());

    /* Get a curl handle. */
    curl = curl_easy_init();

    if(curl) {
      /* First set the URL that is about to receive our POST. */
      curl_easy_setopt(curl, CURLOPT_URL, url.c_string());

      /* Set http authentication to basic. */
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);

      /* Set username and password for http authentication. */
      curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_string());
      curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_string());

      /* Set HTTP header Content-Type. */
      struct curl_slist *list = NULL;
      list = curl_slist_append(list, "Content-Type:application/xml;charset=UTF-8");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      /* Add DataCite Metadata XML to POST. */
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xmlStream.c_str());

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
	rodsLog(LOG_ERROR, "msiRegisterDataCiteDOI: curl error: %s", curl_easy_strerror(res));
	return SYS_INTERNAL_NULL_INPUT_ERR;
      } else {
	long http_code = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);

	/* 201 Created */
	if (http_code == 201) {
	  // Operation successful.
	}
	/* 400 Bad Request */
	else if (http_code == 400) {
	  rodsLog(LOG_ERROR,
		  "msiRegisterDataCiteDOI: Invalid XML or wrong prefix");
	  return SYS_INTERNAL_NULL_INPUT_ERR;
	}
	/* 401 Unauthorized */
	else if (http_code == 401) {
	  rodsLog( LOG_ERROR,
		   "msiRegisterDataCiteDOI: No login");
	  return SYS_INTERNAL_NULL_INPUT_ERR;
	}
	/* 403 Forbidden */
	else if (http_code == 403) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterDataCiteDOI: Login problem, quota exceeded or dataset belongs to another party");
	  return SYS_INTERNAL_NULL_INPUT_ERR;
	}
	/* 415 Forbidden */
	else if (http_code == 415) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterDataCiteDOI: Not including content type in the header");
	  return SYS_INTERNAL_NULL_INPUT_ERR;
	}
	else {
	  rodsLog(LOG_ERROR,
		  "msiRegisterDataCiteDOI: HTTP error code: %lu", http_code);
	  return SYS_INTERNAL_NULL_INPUT_ERR;	}
      }

      /* Cleanup. */
      curl_easy_cleanup(curl);
    }
    /* Cleanup. */
    curl_global_cleanup();

    return 0;
  }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(4);

    msvc->add_operation("msiRegisterDataCiteDOI", "msiRegisterDataCiteDOI");

    return msvc;
  }
}
