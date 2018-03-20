/**
 * \file
 * \brief     iRODS microservice to remove metadata from a DOI at DataCite.
 * \author    Lazlo Westerhof
 * \author    Felix Croes
 * \copyright Copyright (c) 2017-2018, Utrecht University
 *
 * Copyright (c) 2017-2018, Utrecht University
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

#include <string>
#include <fstream>
#include <streambuf>
#include <curl/curl.h>

extern "C" {
  int msiRemoveDataCiteMetadata(msParam_t* urlIn,
				msParam_t* usernameIn,
				msParam_t* passwordIn,
				msParam_t* httpCodeOut,
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

    /* Parse input paramaters. */
    std::string url      = parseMspForStr(urlIn);
    std::string username = parseMspForStr(usernameIn);
    std::string password = parseMspForStr(passwordIn);

    /* Get a curl handle. */
    curl = curl_easy_init();

    if(curl) {
      /* First set the URL of the DOI for which we remove metadata. */
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      /* Set http authentication to basic. */
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);

      /* Set username and password for http authentication. */
      curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
      curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());

      /* Use the DELETE command. */
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
	rodsLog(LOG_ERROR, "msiRemoveDataCiteMetadata: curl error: %s", curl_easy_strerror(res));
	return SYS_INTERNAL_NULL_INPUT_ERR;
      } else {
	long http_code = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
        fillStrInMsParam(httpCodeOut, std::to_string(http_code).c_str());

	/* 200 OK */
	if (http_code == 200) {
	  // Operation successful.
	}
	/* 401 Unauthorized */
	else if (http_code == 401) {
	  rodsLog( LOG_ERROR,
		   "msiRemoveDataCiteMetadata: No login");
	}
	/* 403 Forbidden */
	else if (http_code == 403) {
	  rodsLog(LOG_ERROR,
		   "msiRemoveDataCiteMetadata: Login problem or dataset belongs to another party");
	}
        /* 404 Not Found */
        else if (http_code == 404) {
          rodsLog(LOG_ERROR,
                  "msiRemoveDataCiteMetadata: DOI does not exist");
        }
        /* 500 Internal Server Error */
        else if (http_code == 500) {
	  rodsLog(LOG_ERROR,
		   "msiRemoveDataCiteMetadata: server internal error, try later and if problem persists please contact DataCite");
        }
	else {
	  rodsLog(LOG_ERROR,
		  "msiRemoveDataCiteMetadata: HTTP error code: %lu", http_code);
  	}
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

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiRemoveDataCiteMetadata",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiRemoveDataCiteMetadata));

    return msvc;
  }
}
