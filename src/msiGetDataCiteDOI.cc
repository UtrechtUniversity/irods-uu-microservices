/**
 * \file
 * \brief     iRODS microservice to resolve a DOI with DataCite.
 * \author    Lazlo Westerhof
 * \author    Paul Frederiks
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
#include "CredentialsStore.hh"

#include <string>
#include <fstream>
#include <streambuf>
#include <curl/curl.h>


namespace DataCite {

static CredentialsStore credentials;

  /* Curl requires a static callback function to write the output of a GET request to.
   * This callback simply puts the result in a std::string at *userp */
  static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
  {
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
  }


  int getDOI(msParam_t* doiIn,
	     msParam_t* resultOut,
             msParam_t* httpCodeOut,
             ruleExecInfo_t *rei)
  {
    CURL *curl;
    CURLcode res;


    /* Check input parameters. */
    if (strcmp(doiIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string doi      = parseMspForStr(doiIn);

    /* obtain parameters from the credentials store */
    std::string url(credentials.get("datacite_url"));
    url += "/doi/" + doi;
    std::string username(credentials.get("datacite_username"));
    std::string password(credentials.get("datacite_password"));

    /* declare result buffer for result */ 
    std::string resultBuffer;

    /* Get a curl handle. */
    curl = curl_easy_init();

    if(curl) {
      /* First set the URL . */
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      /* Set http authentication to basic. */
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);

      /* Set username and password for http authentication. */
      curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
      curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());

     /* Add write callback for result */
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resultBuffer);

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
	rodsLog(LOG_ERROR, "msiGetDataCiteDOI: curl error: %s", curl_easy_strerror(res));
	return SYS_INTERNAL_NULL_INPUT_ERR;
      } else {
        /* get HTTP return code */
	long http_code = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        /* fill output params */
        fillStrInMsParam(httpCodeOut, std::to_string(http_code).c_str());
        fillStrInMsParam(resultOut, resultBuffer.c_str());

	/* 200  */
	if (http_code == 200) {
	  // Operation successful.
	}
	/* 400 Bad Request */
	else if (http_code == 204) {
       	    rodsLog(LOG_ERROR,
		    "msiGetDataCiteDOI: DOI is known to MDS, but is not minted (or not resolvable e.g. due to handle's latency)");
	}
	/* 401 Unauthorized */
	else if (http_code == 401) {
	  rodsLog( LOG_ERROR,
		   "msiGetDataCiteDOI: No login");
	}
	/* 403 Forbidden */
	else if (http_code == 403) {
	  rodsLog(LOG_ERROR,
		   "msiGetDataCiteDOI: Login problem, quota exceeded or dataset belongs to another party");
	}
        /* 410 Gone */
        else if (http_code == 404) {
          rodsLog(LOG_ERROR,
                  "msiGetDataCiteDOI: Not Found");
        }
        /* 500 Internal Server Error */
        else if (http_code == 500) {
	  rodsLog(LOG_ERROR,
		   "msiGetDataCiteDOI: server internal error, try later and if problem persists please contact DataCite");
        }
	else {
	  rodsLog(LOG_ERROR,
		  "msiGetDataCiteDOI: HTTP error code: %lu", http_code);
  	}
      }

      /* Cleanup. */
      curl_easy_cleanup(curl);
    }
    /* Cleanup. */
    curl_global_cleanup();

    return 0;
  }

}

extern "C" {

  int msiGetDataCiteDOI(msParam_t* doiIn,
			msParam_t* resultOut,
                        msParam_t* httpCodeOut,
		        ruleExecInfo_t *rei) {
       return DataCite::getDOI(doiIn, resultOut, httpCodeOut, rei);
  }


  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(3);

    msvc->add_operation<
      msParam_t*,
      msParam_t*,
      msParam_t*,
      ruleExecInfo_t*>("msiGetDataCiteDOI",
		       std::function<int(
					 msParam_t*,
					 msParam_t*,
					 msParam_t*,
					 ruleExecInfo_t*)>(msiGetDataCiteDOI));
    return msvc;
  }
}
