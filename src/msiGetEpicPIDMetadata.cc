/**
 * \file
 * \brief     iRODS microservice to get a PID from EPIC.
 * \author    Felix Croes
 * \author    Lazlo Westerhof
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
#include "CredentialsStore.hh"

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>

static CredentialsStore credentials;
static std::string payload;

extern "C" {
  /* Curl requires a static callback function to write the output of a request to.
   * This callback gathers everything in the payload. */
  static size_t receive(void *contents, size_t size, size_t nmemb, void *userp)
  {
      payload.append((const char *) contents, size * nmemb);
      return size * nmemb;
  }


  int msiGetEpicPIDMetadata(msParam_t* handleIn,
		    	    msParam_t* metaNameIn,
		    	    msParam_t* metaValOut,
		    	    msParam_t* httpCodeOut,
		    	    ruleExecInfo_t *rei)
  {
    CURL *curl;
    CURLcode res;

    /* Check if user is priviliged. */
    if (rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
      return SYS_USER_NO_PERMISSION;
    }

    /* Bail early if the credentials store could not be loaded */
    if (!credentials.isLoaded()) {
      return SYS_CONFIG_FILE_ERR;
    }

    /* Check input parameters. */
    if (handleIn->type == NULL || strcmp(handleIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (metaNameIn->type == NULL || strcmp(metaNameIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    const char *handleStr = parseMspForStr(handleIn);
    const char *metaNameStr = parseMspForStr(metaNameIn);
    if (handleStr == NULL || metaNameStr == NULL) {
      return SYS_INVALID_INPUT_PARAM;
    }
    std::string handle = handleStr;
    std::string metaName = metaNameStr;

    /* Set default output */
    fillStrInMsParam(metaValOut, "");

    /* Bail if there is no EPIC server configured. */
    if (!credentials.has("epic_url")) {
      fillStrInMsParam(httpCodeOut, "0");
      return 0;
    }

    /* Retrieve parameters from the credentials store. */
    std::string url(credentials.get("epic_url"));
    std::string prefix(credentials.get("epic_handle_prefix"));
    std::string key(credentials.get("epic_key"));
    std::string certificate(credentials.get("epic_certificate"));

    /* Get a curl handle. */
    curl = curl_easy_init();

    if(curl) {
      /* First set the URL that is about to receive our GET. */
      url += "/" + handle;
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      /* Specify the key and certificate. */
      curl_easy_setopt(curl, CURLOPT_SSLKEY, key.c_str());
      curl_easy_setopt(curl, CURLOPT_SSLCERT, certificate.c_str());

      payload = "";
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);

      /* Don't verify the server certificate. */
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
	rodsLog(LOG_ERROR, "msiGetEpicPIDMetadata: curl error: %s", 
		           curl_easy_strerror(res));

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return SYS_INTERNAL_NULL_INPUT_ERR;
      } else {
	long http_code = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
        fillStrInMsParam(httpCodeOut, std::to_string(http_code).c_str());

	/* 201 Created */
	if (http_code == 200 || http_code == 201) {
	  /* Operation successful.*/
	  json_t *result, *json, *metaVal;
	  json_error_t error;

	  result = json_loads(payload.c_str(), 0, &error);
	  if (result == NULL) {
	    rodsLog(LOG_ERROR, "msiGetEpicPID: Invalid JSON");
	  } else {
	    json = json_object_get(result, "values");
	    /*Iterate over all entries, apart from HS_ADMIN (complex structure)*/
	    for(unsigned long i = 0; i < json_array_size(json); i++){
	      json_t *data;
              std::string key;
	      data = json_array_get(json, i);
	      key = json_string_value(json_object_get(data, "type"));
	      if(key != "HS_ADMIN" && key == metaName){
	        metaVal = json_object_get(json_object_get(data, "data"), "value");
		fillStrInMsParam(metaValOut, json_string_value(metaVal));
	      }
	    }
	    json_decref(result);
	  }
	}
	/* 400 Bad Request */
	else if (http_code == 400) {
	  rodsLog(LOG_ERROR,
		  "msiGetEpicPIDMetadata: Invalid handle");
	}
	/* 401 Unauthorized */
	else if (http_code == 401) {
	  rodsLog( LOG_ERROR,
		   "msiGetEpicPIDMetadata: Authentication needed");
	}
	/* 403 Forbidden */
	else if (http_code == 403) {
	  rodsLog(LOG_ERROR,
		   "msiGetEpicPIDMetadata: Permission denied");
	}
	/* 404 Not Found */
	else if (http_code == 404) {
	  rodsLog(LOG_ERROR,
		   "msiGetEpicPIDMetadata: Handle not found");
	}
	/* 409 Conflict */
	else if (http_code == 409) {
	  rodsLog(LOG_ERROR,
		   "msiGetEpicPIDMetadata: Handle or value already exists");
	}
        /* 500 Internal Server Error */
        else if (http_code == 500) {
	  rodsLog(LOG_ERROR,
		   "msiGetEpicPIDMetadata: Server internal error");
        }
	else {
	  rodsLog(LOG_ERROR,
		  "msiGetEpicPIDMetadata: HTTP error code: %lu", http_code);
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
        ruleExecInfo_t*>("msiGetEpicPIDMetadata",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
			     msParam_t*,
                             ruleExecInfo_t*)>(msiGetEpicPIDMetadata));

    return msvc;
  }
}
