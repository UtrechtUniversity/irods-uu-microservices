/**
 * \file
 * \brief     iRODS microservice to update metadata for a PID at EPIC.
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
static size_t length;

bool checkCurlResponse(long http_code) {
  switch (http_code) {
  case 200:
  case 201:        /* 201 Created */
    return true;

  case 400:        /* 400 Bad Request */
    rodsLog(LOG_ERROR, "msiUpdateEpicPID: Invalid handle");
    break;

  case 401:        /* 401 Unauthorized */
    rodsLog( LOG_ERROR, "msiUpdateEpicPID: Authentication needed");
    break;

  case 403:        /* 403 Forbidden */
    rodsLog(LOG_ERROR, "msiUpdateEpicPID: Permission denied");
    break;

  case 404:        /* 404 Not Found */
    rodsLog(LOG_ERROR, "msiUpdateEpicPID: Handle not found");
    break;

  case 409:        /* 409 Conflict */
    rodsLog(LOG_ERROR, "msiUpdateEpicPID: Handle or value already exists");
    break;

  case 500:        /* 500 Internal Server Error */
    rodsLog(LOG_ERROR, "msiUpdateEpicPID: Server internal error");
    break;

  default:
    rodsLog(LOG_ERROR, "msiUpdateEpicPID: HTTP error code: %lu", http_code);
    break;
  }

  return false;
}

json_t *updateMetadata(json_t *array, std::string &key, std::string *value) {
  json_t *item, *json;
  json_int_t index, last;

  last = 0;
  for (size_t i = 0; i < json_array_size(array); i++) {
    item = json_array_get(array, i);
    index = json_integer_value(json_object_get(item, "index"));
    if (index == last + 1) {
      last = index;	/* update insertion point */
    }

    if (index != 1 && index != 100) {
      if (strcmp(json_string_value(json_object_get(item, "type")), key.c_str()) == 0) {
        if (value != NULL) {
	  /* update metadata */
          json = json_object_get(item, "data");
          json_object_set_new(json, "value", json_string(value->c_str()));
        } else {
	  /* delete metadata */
          json_array_remove(array, i);
        }

        return array;
      }
    }
  }

  if (value != NULL) {
    /*
     * add new metadata
     */
    item = json_object();
    json_array_insert_new(array, (size_t) last, item);
    json_object_set_new(item, "index", json_integer(last + 1));
    json_object_set_new(item, "type", json_string(key.c_str()));
    json = json_object();
    json_object_set_new(item, "data", json);
    json_object_set_new(json, "format", json_string("string"));
    json_object_set_new(json, "value", json_string(value->c_str()));
  }

  return array;
}

extern "C" {
  /* Curl requires a static callback function to write the output of a request to.
   * This callback gathers everything in the payload. */
  static size_t receive(void *contents, size_t size, size_t nmemb, void *userp)
  {
    payload.append((const char *) contents, size * nmemb);
    return size * nmemb;
  }

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


  int msiUpdateEpicPID(msParam_t* handleIn,
                       msParam_t* typeIn,
                       msParam_t* valueIn,
                       msParam_t* httpCodeOut,
                       ruleExecInfo_t *rei)
  {
    CURL *curl;
    CURLcode res;

    /* Check if user is privileged. */
    if (rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
      return SYS_USER_NO_PERMISSION;
    }

    /* Bail early if the credentials store could not be loaded */
    if (!credentials.isLoaded()) {
      return SYS_CONFIG_FILE_ERR;
    }

    /* Check input parameters. */
    if (strcmp(handleIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(typeIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* valueIn can be unset, which means delete. */
    std::string value, *valueRef;
    if (valueIn->type == NULL) {
      valueRef = NULL;
    } else if (strcmp(typeIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    } else {
      value = parseMspForStr(valueIn);
      valueRef = &value;
    }

    /* Parse input paramaters. */
    std::string handle    = parseMspForStr(handleIn);
    std::string type      = parseMspForStr(typeIn);

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

      /* Set HTTP headers. */
      struct curl_slist *list = NULL;
      list = curl_slist_append(list, "Content-Type: application/json");
      list = curl_slist_append(list, "Authorization: Handle clientCert=\"true\"");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

      /* Specify the key and certificate. */
      curl_easy_setopt(curl, CURLOPT_SSLKEY, key.c_str());
      curl_easy_setopt(curl, CURLOPT_SSLCERT, certificate.c_str());

      /* Don't verify the server certificate. */
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      payload = "";
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
        rodsLog(LOG_ERROR, "msiUpdateEpicPID: curl error: %s", curl_easy_strerror(res));

        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return SYS_INTERNAL_NULL_INPUT_ERR;
      } else {
        long http_code = 0;
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (!checkCurlResponse(http_code)) {
          fillStrInMsParam(httpCodeOut, std::to_string(http_code).c_str());

          curl_easy_cleanup(curl);
          curl_global_cleanup();
          return 0;
        }
      }

      json_error_t error;
      json_t *result = json_loads(payload.c_str(), 0, &error);
      if (result == NULL) {
        rodsLog(LOG_ERROR, "msiUpdateEpicPID: Invalid JSON");

        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 0;
      }

      /* Create payload. */
      char *str = json_dumps(updateMetadata(json_object_get(result, "values"), type, valueRef), 0);
      payload = std::string("{\"values\":") + str + "}";
      free(str);
      json_decref(result);

      curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
      curl_easy_setopt(curl, CURLOPT_READDATA, &payload);
      length = payload.length();
      curl_easy_setopt(curl, CURLOPT_INFILESIZE, (long) length);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);

      /* Use the PUT command. */
      curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

      /* Perform the request, res will get the return code. */
      res = curl_easy_perform(curl);

      /* Check for errors. */
      if(res != CURLE_OK) {
        rodsLog(LOG_ERROR, "msiUpdateEpicPID: curl error: %s", curl_easy_strerror(res));

        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return SYS_INTERNAL_NULL_INPUT_ERR;
      } else {
        long http_code = 0;
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
        fillStrInMsParam(httpCodeOut, std::to_string(http_code).c_str());
        checkCurlResponse(http_code);
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
        ruleExecInfo_t*>("msiUpdateEpicPID",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiUpdateEpicPID));

    return msvc;
  }
}
