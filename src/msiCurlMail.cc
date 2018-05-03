/**
 * \file
 * \brief     iRODS microservice to send an email.
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

#include <ctime>
#include <string>
#include <curl/curl.h>

std::string dateTimeNow()
{
  const int RFC5322_TIME_LEN = 32;

  std::string ret;
  ret.resize(RFC5322_TIME_LEN);

  tm tv, *t = &tv;
  time_t tt = time(&tt);
  localtime_r(&tt, t);

  strftime(&ret[0], RFC5322_TIME_LEN, "%a, %d %b %Y %H:%M:%S %z", t);

  return ret;
}

std::string messageId()
{
  const int MESSAGE_ID_LEN = 37;

  tm t;
  time_t tt;
  time(&tt);

  std::string ret;
  ret.resize(15);

  gmtime_r(&tt, &t);

  strftime(const_cast<char *>(ret.c_str()),
	   MESSAGE_ID_LEN,
	   "%Y%m%d%H%M%S.",
	   &t);

  ret.reserve(MESSAGE_ID_LEN);

  static const char alphaNum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

  while (ret.size() < MESSAGE_ID_LEN) {
    ret += alphaNum[(size_t) rand() % (sizeof(alphaNum) - 1)];
  }

  return ret;
}

std::string setPayloadText(const std::string &to,
			   const std::string &from,
			   const std::string &nameFrom,
			   const std::string &subject,
			   const std::string &body)
{
  std::string ret;

  ret += "Date: "  + dateTimeNow() + ">\r\n";
  ret += "To: <"   + to   + ">\r\n";
  ret += "From: <" + from + "> (" + nameFrom + ")\r\n";
  ret += "Message-ID: <"  + messageId() + "@" + from.substr(from.find('@') + 1) + ">\r\n";
  ret += "Subject: "      + subject + "\r\n";
  ret += "\r\n";
  ret += body + "\r\n";
  ret += "\r\n";
  ret += "\r\n";
  ret += "\r\n";

  return ret;
}

struct uploadStatus { int linesRead; };

struct StringData
{
  std::string msg;
  size_t bytesleft;

  StringData(std::string &&m) : msg{ m }, bytesleft{ msg.size() } {}
  StringData(std::string  &m) = delete;
};

size_t payloadSource(void *ptr, size_t size, size_t nmemb, void *userp)
{
  StringData *text = reinterpret_cast<StringData *>(userp);

  if ((size == 0) || (nmemb == 0) || ((size*nmemb) < 1) || (text->bytesleft == 0)) {
    return 0;
  }

  if ((nmemb * size) >= text->msg.size()) {
    text->bytesleft = 0;
    return text->msg.copy(reinterpret_cast<char *>(ptr), text->msg.size());
  }

  return 0;
}

CURLcode sendMail(const std::string to,
		  const std::string from,
		  const std::string nameFrom,
		  const std::string subject,
		  const std::string body,
		  const std::string smtpServer,
		  const std::string userName,
		  const std::string password)
{
  CURLcode res = CURLE_OK;

  struct curl_slist *recipients = NULL;

  /* Get a curl handle. */
  CURL *curl = curl_easy_init();

  StringData textData { setPayloadText(to, from, nameFrom, subject, body) };

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, userName.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, smtpServer.c_str());

    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + from + ">").c_str());
    recipients = curl_slist_append(recipients, ("<" + to   + ">").c_str());

    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payloadSource);
    curl_easy_setopt(curl, CURLOPT_READDATA, &textData);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* Perform the request, res will get the return code. */
    res = curl_easy_perform(curl);

    /* Check for errors. */
    if (res != CURLE_OK) {
      rodsLog(LOG_ERROR, "msiCurlMail: curl error: %s", curl_easy_strerror(res));
    }

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
  }

  return res;
}

extern "C" {
  int msiCurlMail(msParam_t* toIn,
		  msParam_t* fromIn,
		  msParam_t* nameFromIn,
		  msParam_t* subjectIn,
		  msParam_t* bodyIn,
		  msParam_t* smtpServerIn,
		  msParam_t* userNameIn,
		  msParam_t* passwordIn,
	          msParam_t* curlCodeOut,
		  ruleExecInfo_t *rei)
  {
    int curlCode = 0;

    /* Check if user is priviliged. */
    if (rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
      return SYS_USER_NO_PERMISSION;
    }

    /* Check input parameters. */
    if (strcmp(toIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(fromIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(nameFromIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(subjectIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(bodyIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(smtpServerIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(userNameIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }
    if (strcmp(passwordIn->type, STR_MS_T)) {
      return SYS_INVALID_INPUT_PARAM;
    }

    /* Parse input paramaters. */
    std::string to         = parseMspForStr(toIn);
    std::string from       = parseMspForStr(fromIn);
    std::string nameFrom   = parseMspForStr(nameFromIn);
    std::string subject    = parseMspForStr(subjectIn);
    std::string body       = parseMspForStr(bodyIn);
    std::string smtpServer = parseMspForStr(smtpServerIn);
    std::string userName   = parseMspForStr(userNameIn);
    std::string password   = parseMspForStr(passwordIn);

    curlCode = sendMail(to, from, nameFrom, subject, body, smtpServer, userName, password);

    fillStrInMsParam(curlCodeOut, std::to_string(curlCode).c_str());

    return 0;
  }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry *msvc = new irods::ms_table_entry(9);

    msvc->add_operation<
      msParam_t*,
      msParam_t*,
      msParam_t*,
      msParam_t*,
      msParam_t*,
      msParam_t*,
      msParam_t*,
      msParam_t*,
      msParam_t*,
      ruleExecInfo_t*>("msiCurlMail",
                         std::function<int(
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             msParam_t*,
                             ruleExecInfo_t*)>(msiCurlMail));

    return msvc;
  }
}
