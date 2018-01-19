/**
 * @file	msiXmlDocSchemaValidate.cpp
 *
 */ 

/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include <libxml/uri.h>
#include "apiHeaderAll.hpp"
#include "msParam.hpp"
#include "reGlobalsExtern.hpp"
#include "irods_ms_plugin.hpp"
#include "irods_server_api_call.hpp"
#include "microservice.hpp"
#include "objMetaOpr.hpp"
#include "miscUtil.h"




extern "C" {


/* custom handler to catch XML validation errors and print them to a buffer */
static int 
myErrorCallback(bytesBuf_t *errBuf, const char* errMsg, ...)
{
	va_list ap;
	char tmpStr[MAX_NAME_LEN];
	int written;

	va_start(ap, errMsg);
	written = vsnprintf(tmpStr, MAX_NAME_LEN, errMsg, ap);
	va_end(ap);

	appendToByteBuf(errBuf, tmpStr);
	return (written);
}


/**
 * \fn msiXmlDocSchemaValidate(msParam_t *xmlObj, msParam_t *xsdObj, msParam_t *status, ruleExecInfo_t *rei)
 *
 * \brief  This microservice validates an XML file against an XSD schema, both iRODS objects.
 * 
 * \module xml
 * 
 * \since pre-2.1
 * 
 * \author  Antoine de Torcy
 * \date    2008/05/29
 * 
 * \usage See clients/icommands/test/rules3.0/
 *
 * \param[in] xmlObj - a msParam of type DataObjInp_MS_T or STR_MS_T which is irods path of the XML object.
 * \param[in] xsdObj - a msParam of type DataObjInp_MS_T or STR_MS_T which is irods path of the XSD object.
 * \param[out] status - a msParam of type INT_MS_T which is a validation result.
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence None
 * \DolVarModified None
 * \iCatAttrDependence None
 * \iCatAttrModified None
 * \sideeffect None
 *
 * \return integer
 * \retval 0 on success
 * \pre None
 * \post None
 * \sa None
**/
int 
msiXmlDocSchemaValidate(msParam_t *xmlObj, msParam_t *xsdObj, msParam_t *status, ruleExecInfo_t *rei) 
{
	/* for parsing msParams and to open iRODS objects */
	dataObjInp_t xmlObjInp, *myXmlObjInp;
	dataObjInp_t xsdObjInp, *myXsdObjInp;
	int xmlObjID, xsdObjID;

	/* for getting size of objects to read from */
	rodsObjStat_t *rodsObjStatOut = NULL;

	/* for reading from iRODS objects */
	openedDataObjInp_t openedDataObjInp;
	bytesBuf_t *xmlBuf = NULL;
	xmlChar* xmlXmlChar;
	xmlChar* xsdXmlChar;

	/* for xml parsing and validating */
	xmlDocPtr doc, xsd_doc;
	xmlSchemaParserCtxtPtr parser_ctxt;
	xmlSchemaPtr schema;
	xmlSchemaValidCtxtPtr valid_ctxt;
	bytesBuf_t *errBuf;

	/* misc. to avoid repeating rei->rsComm */
	rsComm_t *rsComm;



	/*************************************  USUAL INIT PROCEDURE **********************************/
	
	/* For testing mode when used with irule --test */
	RE_TEST_MACRO ("    Calling msiXmlDocSchemaValidate")


	/* Sanity checks */
	if (rei == NULL || rei->rsComm == NULL)
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: input rei or rsComm is NULL.");
		return (SYS_INTERNAL_NULL_INPUT_ERR);
	}

	rsComm = rei->rsComm;



	/************************************ ADDITIONAL INIT SETTINGS *********************************/

	/* XML constants */
	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;


	/* allocate memory for output error buffer */
	errBuf = (bytesBuf_t *)malloc(sizeof(bytesBuf_t));
	errBuf->buf = strdup("");
	errBuf->len = strlen((char*)errBuf->buf);

	/* Default status is failure, overwrite if success */
	fillBufLenInMsParam (status, -1, NULL);

	
	/********************************** RETRIEVE INPUT PARAMS **************************************/

	/* Get path of XML document */
	rei->status = parseMspForDataObjInp (xmlObj, &xmlObjInp, &myXmlObjInp, 0);
	if (rei->status < 0)
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: input xmlObj error. status = %d", rei->status);
		free(errBuf);
		return (rei->status);
	}


	/* Get path of schema */
	rei->status = parseMspForDataObjInp (xsdObj, &xsdObjInp, &myXsdObjInp, 0);
	if (rei->status < 0)
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: input xsdObj error. status = %d", rei->status);
		free(errBuf);
		return (rei->status);
	}


	/******************************** OPEN AND READ FROM XML OBJECT ********************************/

	/* Open XML file */
	if ((xmlObjID = rsDataObjOpen(rsComm, &xmlObjInp)) < 0) 
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: Cannot open XML data object. status = %d", xmlObjID);
		free(errBuf);
		return (xmlObjID);
	}


	/* Get size of XML file */
	rei->status = rsObjStat (rsComm, &xmlObjInp, &rodsObjStatOut);
	if (rei->status < 0 || !rodsObjStatOut)
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: Cannot stat XML data object. status = %d", rei->status);
		free(errBuf);
		return (rei->status);
	}


	/* xmlBuf init */
	/* memory for xmlBuf->buf is allocated in rsFileRead() */
	xmlBuf = (bytesBuf_t *) malloc (sizeof (bytesBuf_t));
	memset (xmlBuf, 0, sizeof (bytesBuf_t));


	/* Read content of XML file */
	memset (&openedDataObjInp, 0, sizeof (openedDataObjInp_t));
	openedDataObjInp.l1descInx = xmlObjID;
	openedDataObjInp.len = (int)rodsObjStatOut->objSize;

	rei->status = rsDataObjRead (rsComm, &openedDataObjInp, xmlBuf);

	xmlXmlChar = xmlCharStrndup((char*)xmlBuf->buf, xmlBuf->len);

	/* Close XML file */
	rei->status = rsDataObjClose (rsComm, &openedDataObjInp);

	/* cleanup */
	freeRodsObjStat (rodsObjStatOut);

	/* content of xmlBuf can be discarded */
	clearBBuf(xmlBuf);


	/*************************************** PARSE XML DOCUMENT **************************************/

	/* Parse xmlBuf.buf into an xmlDocPtr */
	doc = xmlParseDoc(xmlXmlChar);

	if (doc == NULL)
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: XML document cannot be loaded or is not well-formed.");
		free(errBuf);
	        /* Don't call xmlCleanupParser when libxml2 will be used in other microservice calls
  		xmlCleanupParser();
		*/

	    return (USER_INPUT_FORMAT_ERR);
	}



	/******************************** OPEN AND READ FROM XSD OBJECT ********************************/

	/* Open schema file */
	if ((xsdObjID = rsDataObjOpen(rsComm, &xsdObjInp)) < 0) 
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: Cannot open XSD data object. status = %d", xsdObjID);
		free(errBuf);
		xmlFreeDoc(doc);
	        /* Don't call xmlCleanupParser when libxml2 will be used in other microservice calls
  		xmlCleanupParser();
		*/
		return (xsdObjID);
	}


	/* Get size of schema file */
	rei->status = rsObjStat (rsComm, &xsdObjInp, &rodsObjStatOut);


	/* Read entire schema file */
	memset (&openedDataObjInp, 0, sizeof (openedDataObjInp_t));
	openedDataObjInp.l1descInx = xsdObjID;
	openedDataObjInp.len = (int)rodsObjStatOut->objSize;

	rei->status = rsDataObjRead (rsComm, &openedDataObjInp, xmlBuf);

	xsdXmlChar = xmlCharStrndup((char*)xmlBuf->buf, xmlBuf->len);

	/* Close schema file */
	rei->status = rsDataObjClose (rsComm, &openedDataObjInp);

	/* cleanup */
	freeRodsObjStat (rodsObjStatOut);

	/* xmlBuf is no longer needed */
	freeBBuf(xmlBuf);

	/*************************************** PARSE XSD DOCUMENT **************************************/

	xsd_doc = xmlParseDoc(xsdXmlChar);

	if (xsd_doc == NULL)
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: XML Schema cannot be loaded or is not well-formed.");
		free(errBuf);
		xmlFreeDoc(doc);
	        /* Don't call xmlCleanupParser when libxml2 will be used in other microservice calls
		xmlCleanupParser();
		*/
	    return (USER_INPUT_FORMAT_ERR);
	}



	/**************************************** VALIDATE DOCUMENT **************************************/

	/* Create a parser context */
	parser_ctxt = xmlSchemaNewDocParserCtxt(xsd_doc);
	if (parser_ctxt == NULL)
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: Unable to create a parser context for the schema.");
		free(errBuf);
		xmlFreeDoc(xsd_doc);
		xmlFreeDoc(doc);
	        /* Don't call xmlCleanupParser when libxml2 will be used in other microservice calls
		xmlCleanupParser();
		*/
	    return (USER_INPUT_FORMAT_ERR);
	}


	/* Parse the XML schema */
	schema = xmlSchemaParse(parser_ctxt);
	if (schema == NULL) 
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: Invalid schema.");
		free(errBuf);
		xmlSchemaFreeParserCtxt(parser_ctxt);
		xmlFreeDoc(doc);
		xmlFreeDoc(xsd_doc);

	        /* Don't call xmlCleanupParser when libxml2 will be used in other microservice calls
		xmlCleanupParser();
		*/

	    return (USER_INPUT_FORMAT_ERR);
	}


	/* Create a validation context */
	valid_ctxt = xmlSchemaNewValidCtxt(schema);
	if (valid_ctxt == NULL) 
	{
		rodsLog (LOG_ERROR, "msiXmlDocSchemaValidate: Unable to create a validation context for the schema.");
		free(errBuf);
		xmlSchemaFree(schema);
		xmlSchemaFreeParserCtxt(parser_ctxt);
		xmlFreeDoc(xsd_doc);
		xmlFreeDoc(doc);
		/* Don't call when libxml2 is used by other microservice calls
		xmlCleanupParser();
		*/
	    return (USER_INPUT_FORMAT_ERR);
	}


	/* Set myErrorCallback() as the default handler for error messages and warnings */
	xmlSchemaSetValidErrors(valid_ctxt, (xmlSchemaValidityErrorFunc)myErrorCallback, (xmlSchemaValidityWarningFunc)myErrorCallback, errBuf);


	/* Validate XML doc */
	rei->status = xmlSchemaValidateDoc(valid_ctxt, doc);



	/******************************************* WE'RE DONE ******************************************/

	/* return both error code and messages through status */
	resetMsParam (status);
	fillBufLenInMsParam (status, rei->status, errBuf);


	/* cleanup of all xml parsing stuff */
	xmlSchemaFreeValidCtxt(valid_ctxt);
	xmlSchemaFree(schema);
	xmlSchemaFreeParserCtxt(parser_ctxt);
	xmlFreeDoc(doc);
	xmlFreeDoc(xsd_doc);
	
	/* Don't call when libxml2 is used by other microservice calls
	xmlCleanupParser();
	*/

	return (rei->status);
}


irods::ms_table_entry * plugin_factory() {
    irods::ms_table_entry* msvc = new irods::ms_table_entry(3);
    msvc->add_operation("msiXmlDocSchemaValidate","msiXmlDocSchemaValidate");
    return msvc;
}

} // extern "C"

