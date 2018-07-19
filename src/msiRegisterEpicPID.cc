/**
 * \file
 * \brief     iRODS microservice to register a PID with EPIC.
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
#include "CredentialsStore.hh"

#include <string>
#include <fstream>
#include <streambuf>
#include <curl/curl.h>

static CredentialsStore credentials;
static size_t length;

extern "C" {

  /* Curl requires a static callback function to read the input of a PUT request from.
   * This callback simply copies the payload. */
  static size_t readCallback(void *buffer, size_t size, size_t nmemb, void *userp)
  {
      std::string *payload = (std::string *) userp;
      size_t len = size * nmemb;
      if (len > length) {
          len = length;
      }
      memcpy(buffer, payload->c_str() + payload->length() - length, len);
      length -= len;

      return len;
  }

  /* Curl requires a static callback function to write the output of a request to.
   * This callback simply discards everything. */
  static size_t discard(void *contents, size_t size, size_t nmemb, void *userp)
  {
      return size * nmemb;
  }


  int msiRegisterEpicPID(msParam_t* valueIn,
			 msParam_t* UUIDIn,
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
    if (strcmp(valueIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string value       = parseMspForStr(valueIn);
    std::string uuid        = parseMspForStr(UUIDIn);

    /* Minimally verify that these will embed nicely in a payload. */
    if (value.find('"') != std::string::npos) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Retriece parameters from the credentials store. */
    std::string url(credentials.get("epic_url"));
    std::string prefix(credentials.get("epic_handle_prefix"));
    std::string key(credentials.get("epic_key"));
    std::string certificate(credentials.get("epic_certificate"));

    /* Obtain PID. */
    std::string pid(prefix + "/" + uuid);

    /* Get a curl handle. */
    curl = curl_easy_init();

    if(curl) {
      /* First set the URL that is about to receive our PUT. */
      url += "/" + pid;
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      /* Set HTTP headers. */
      struct curl_slist *list = NULL;
      list = curl_slist_append(list, "Content-Type: application/json");
      list = curl_slist_append(list, "Authorization: Handle clientCert=\"true\"");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      /* Specify the key and certificate. */
      curl_easy_setopt(curl, CURLOPT_SSLKEY, key.c_str());
      curl_easy_setopt(curl, CURLOPT_SSLCERT, certificate.c_str());

      /* Create payload. */
      std::string payload = "{\"values\":[{\"index\":1,\"type\":\"URL\",\"data\":{\"format\":\"string\",\"value\":\"" +
			    value +
			    "\"}},{\"index\":100,\"type\":\"HS_ADMIN\",\"data\":{\"format\":\"admin\",\"value\":{\"handle\":\"0.NA/" +
			    prefix +
			    "\",\"index\":200,\"permissions\":\"011111110011\"}}}]}";
      curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
      curl_easy_setopt(curl, CURLOPT_READDATA, &payload);
      length = payload.length();
      curl_easy_setopt(curl, CURLOPT_INFILESIZE, (long) length);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);
	
      /* Don't verify the server certificate. */
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      /* Use the PUT command. */
      curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
	rodsLog(LOG_ERROR, "msiRegisterEpicPID: curl error: %s", curl_easy_strerror(res));
	return SYS_INTERNAL_NULL_INPUT_ERR;
      } else {
	long http_code = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
        fillStrInMsParam(httpCodeOut, std::to_string(http_code).c_str());

	/* 201 Created */
	if (http_code == 200 || http_code == 201) {
	  /* Operation successful.*/
	}
	/* 400 Bad Request */
	else if (http_code == 400) {
	  rodsLog(LOG_ERROR,
		  "msiRegisterEpicPID: Invalid handle");
	}
	/* 401 Unauthorized */
	else if (http_code == 401) {
	  rodsLog( LOG_ERROR,
		   "msiRegisterEpicPID: Authentication needed");
	}
	/* 403 Forbidden */
	else if (http_code == 403) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterEpicPID: Permission denied");
	}
	/* 404 Not Found */
	else if (http_code == 404) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterEpicPID: Handle not found");
	}
	/* 409 Conflict */
	else if (http_code == 409) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterEpicPID: Handle or value already exists");
	}
        /* 500 Internal Server Error */
        else if (http_code == 500) {
	  rodsLog(LOG_ERROR,
		   "msiRegisterEpicPID: Server internal error");
        }
	else {
	  rodsLog(LOG_ERROR,
		  "msiRegisterEpicPID: HTTP error code: %lu", http_code);
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
    irods::ms_table_entry *msvc = new irods::ms_table_entry(3);

    msvc->add_operation<
        msParam_t*,
        msParam_t*,
        msParam_t*,
        ruleExecInfo_t*>("msiRegisterEpicPID",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiRegisterEpicPID));

    return msvc;
  }
}
