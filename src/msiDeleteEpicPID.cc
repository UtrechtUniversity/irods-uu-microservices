/**
 * \file
 * \brief     iRODS microservice to delete a PID from EPIC.
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

std::string escapeJson(const std::string &s) {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        switch (*c) {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if ('\x00' <= *c && *c <= '\x1f') {
                o << "\\u"
                  << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
            } else {
                o << *c;
            }
        }
    }
    return o.str();
}

extern "C" {
  /* Curl requires a static callback function to write the output of a request to.
   * This callback simply discards everything. */
  static size_t discard(void *contents, size_t size, size_t nmemb, void *userp)
  {
      return size * nmemb;
  }


  int msiDeleteEpicPID(msParam_t* idIn,
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
    if (strcmp(idIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string id        = parseMspForStr(idIn);

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

    /* Obtain PID. */
    std::string pid(prefix + "/" + id);

    /* Get a curl handle. */
    curl = curl_easy_init();

    if(curl) {
      /* First set the URL that is about to receive our DELETE. */
      url += "/" + pid;
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      /* Set HTTP headers. */
      struct curl_slist *list = NULL;
      list = curl_slist_append(list, "Authorization: Handle clientCert=\"true\"");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      /* Specify the key and certificate. */
      curl_easy_setopt(curl, CURLOPT_SSLKEY, key.c_str());
      curl_easy_setopt(curl, CURLOPT_SSLCERT, certificate.c_str());

      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);

      /* Don't verify the server certificate. */
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      /* Use the DELETE command. */
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
	rodsLog(LOG_ERROR, "msiDeleteEpicPID: curl error: %s", curl_easy_strerror(res));
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
		  "msiDeleteEpicPID: Invalid handle");
	}
	/* 401 Unauthorized */
	else if (http_code == 401) {
	  rodsLog( LOG_ERROR,
		   "msiDeleteEpicPID: Authentication needed");
	}
	/* 403 Forbidden */
	else if (http_code == 403) {
	  rodsLog(LOG_ERROR,
		   "msiDeleteEpicPID: Permission denied");
	}
	/* 404 Not Found */
	else if (http_code == 404) {
	  rodsLog(LOG_ERROR,
		   "msiDeleteEpicPID: Handle not found");
	}
	/* 409 Conflict */
	else if (http_code == 409) {
	  rodsLog(LOG_ERROR,
		   "msiDeleteEpicPID: Handle or value already exists");
	}
        /* 500 Internal Server Error */
        else if (http_code == 500) {
	  rodsLog(LOG_ERROR,
		   "msiDeleteEpicPID: Server internal error");
        }
	else {
	  rodsLog(LOG_ERROR,
		  "msiDeleteEpicPID: HTTP error code: %lu", http_code);
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
        ruleExecInfo_t*>("msiDeleteEpicPID",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiDeleteEpicPID));

    return msvc;
  }
}
