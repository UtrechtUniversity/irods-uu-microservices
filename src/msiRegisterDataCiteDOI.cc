/**
 * \file
 * \brief     iRODS microservice to register a DOI with DataCite.
 * \author    Lazlo Westerhof
 * \copyright Copyright (c) 2017-2018, Utrecht University
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
#include "CredentialsStore.hh"

#include <string>
#include <fstream>
#include <streambuf>
#include <curl/curl.h>

static CredentialsStore credentials;

extern "C" {
  int msiRegisterDataCiteDOI(msParam_t* payloadIn,
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
    if (strcmp(payloadIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string payload  = parseMspForStr(payloadIn);

    /* Check if payload is XML */
    bool isXml;
    std::string xmlPrefix = "<?xml";
    if (payload.substr(0, xmlPrefix.size()) == xmlPrefix) {
      isXml = true;
    } else {
      isXml = false;
    }

    /* Get parameters from credentials store. */
    std::string url(credentials.get("datacite_url"));
    url += (isXml) ? "/metadata" : "/doi";
    std::string username(credentials.get("datacite_username"));
    std::string password(credentials.get("datacite_password"));

    /* Get a curl handle. */
    curl = curl_easy_init();

    if(curl) {
      /* First set the URL that is about to receive our POST. */
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      /* Set http authentication to basic. */
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);

      /* Set username and password for http authentication. */
      curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
      curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());

      /* Set HTTP header Content-Type. */
      struct curl_slist *list = NULL;
      if (isXml) {
        list = curl_slist_append(list, "Content-Type:application/xml;charset=UTF-8");
      } else {
        list = curl_slist_append(list, "Content-Type:text/plain;charset=UTF-8");
      }
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      /* Add DataCite payload to POST. */
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
	rodsLog(LOG_ERROR, "msiRegisterDataCiteDOI: curl error: %s", curl_easy_strerror(res));
	return SYS_INTERNAL_NULL_INPUT_ERR;
      } else {
	long http_code = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
        fillStrInMsParam(httpCodeOut, std::to_string(http_code).c_str());

	/* 201 Created */
	if (http_code == 201) {
	  /* Operation successful.*/
	}
	/* 400 Bad Request */
	else if (http_code == 400) {
          if (isXml) {
	    rodsLog(LOG_ERROR,
		    "msiRegisterDataCiteDOI: invalid XML, wrong prefix");
          } else {
	    rodsLog(LOG_ERROR,
		    "msiRegisterDataCiteDOI:request body must be exactly two lines: DOI and URL; wrong domain, wrong prefix");
          }
	}
	/* 401 Unauthorized */
	else if (http_code == 401) {
	  rodsLog( LOG_ERROR,
		   "msiRegisterDataCiteDOI: No login");
	}
	/* 403 Forbidden */
	else if (http_code == 403) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterDataCiteDOI: Login problem, quota exceeded or dataset belongs to another party");
	}
        /* 410 Gone */
        else if (http_code == 410) {
          rodsLog(LOG_ERROR,
                  "msiRegisterDataCiteDOI: the requested dataset was marked inactive (using DELETE method");
        }
        /* 412 Precondition Failed */
        else if (http_code == 412) {
          rodsLog(LOG_ERROR,
                  "msiRegisterDataCiteDOI: Metadata must be uploaded first");
        }
	/* 415 Unsupported Media Type */
	else if (http_code == 415) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterDataCiteDOI: Not including content type in the header");
	}
        /* 500 Internal Server Error */
        else if (http_code == 500) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterDataCiteDOI: server internal error, try later and if problem persists please contact DataCite");
        }
	else {
	  rodsLog(LOG_ERROR,
		  "msiRegisterDataCiteDOI: HTTP error code: %lu", http_code);
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
    irods::ms_table_entry *msvc = new irods::ms_table_entry(2);

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiRegisterDataCiteDOI",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiRegisterDataCiteDOI));

    return msvc;
  }
}
