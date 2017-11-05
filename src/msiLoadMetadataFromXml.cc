/**
 * @file	msiLoadMetadataFromXml.cc
 *
 */ 

/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
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



/* get the first child node with a given name */
static xmlNodePtr
getChildNodeByName(xmlNodePtr cur, char *name)
{
        if (cur == NULL || name == NULL)
	{
		return NULL;
	}

        for(cur=cur->children; cur != NULL; cur=cur->next)
	{
                if(cur->name && (strcmp((char*)cur->name, name) == 0))
		{
                        return cur;
                }
        }

	return NULL;
}



/**
 * \fn msiLoadMetadataFromXml (msParam_t *targetObj, msParam_t *xmlObj, ruleExecInfo_t *rei)
 *
 * \brief   This microservice parses an XML iRODS file to extract metadata tags.
 *
 * \module XML
 *
 * \since pre-2.1
 *
 * \author  Antoine de Torcy
 * \date    2008/05/29
 *
 * \usage See clients/icommands/test/rules3.0/
 *  
 * \param[in] targetObj - Optional - a msParam of type DataObjInp_MS_T or STR_MS_T
 * \param[in] xmlObj - a msParam of type DataObjInp_MS_T or STR_MS_T
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

extern "C" {
#if defined(LIBXML_XPATH_ENABLED) && \
    defined(LIBXML_SAX1_ENABLED) && \
    defined(LIBXML_OUTPUT_ENABLED)

int 
msiLoadMetadataFromXml(msParam_t *targetObj, msParam_t *xmlParam, ruleExecInfo_t *rei) 
{
	/* for parsing msParams and to open iRODS objects */
	dataObjInp_t xmlDataObjInp, *myXmlDataObjInp;
	dataObjInp_t targetObjInp, *myTargetObjInp;
	int xmlObjID;
	bytesBuf_t * xmlBuf;

	/* for getting size of objects to read from */
	rodsObjStat_t *rodsObjStatOut = NULL;

	/* for reading from iRODS objects */
	openedDataObjInp_t openedDataObjInp;

	/* misc. to avoid repeating rei->rsComm */
	rsComm_t *rsComm;

	/* for xml parsing */
	xmlDocPtr doc;
        xmlChar* xmlXmlChar;

	/* for XPath evaluation */
	xmlXPathContextPtr xpathCtx;
	xmlXPathObjectPtr xpathObj;
	xmlChar xpathExpr[] = "//AVU";
	xmlNodeSetPtr nodes;
	int avuNbr, i;
	
	/* for new AVU creation */
	modAVUMetadataInp_t modAVUMetadataInp;
	char attribute[] = "Attribute";
	char value[] = "Value";
	char unit[] = "Unit";
	char target[] = "Target";
	char add[] = "add";
	char collection[] = "-C";
	char dataObject[] = "-d";
	const size_t MAX_ATTR_NAME_LEN = 2700;
        const size_t MAX_ATTR_VALUE_LEN = 2700;
	const size_t MAX_ATTR_UNIT_LEN = 250;
	char* attrName;
	char* attrValue;
	char* attrUnit;
	size_t attrNameLen, attrValueLen, attrUnitLen;
	rodsLong_t coll_id;



	/*********************************  USUAL INIT PROCEDURE **********************************/
	
	/* For testing mode when used with irule --test */
	RE_TEST_MACRO ("    Calling msiLoadMetadataFromXml")


	/* Sanity checks */
	if (rei == NULL || rei->rsComm == NULL)
	{
		rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: input rei or rsComm is NULL.");
		return (SYS_INTERNAL_NULL_INPUT_ERR);
	}

	rsComm = rei->rsComm;


	
	/********************************** RETRIEVE INPUT PARAMS **************************************/

	/* Get path of target object */
	rei->status = parseMspForDataObjInp (targetObj, &targetObjInp, &myTargetObjInp, 0);
	if (rei->status < 0)
	{
		rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: input targetObj error. status = %d", rei->status);
		return (rei->status);
	}





	if ( strcmp( xmlParam->type, BUF_LEN_MS_T) == 0 && xmlParam->inpOutBuf != NULL) {

		xmlBuf = xmlParam->inpOutBuf;	
		xmlXmlChar = xmlCharStrndup((char*)xmlBuf->buf, xmlBuf->len);
		
	} else if ( strcmp( xmlParam->type, STR_MS_T) == 0 || strcmp( xmlParam->type, DataObjInp_MS_T) == 0) {

		/* Get path of XML document */
		rei->status = parseMspForDataObjInp (xmlParam, &xmlDataObjInp, &myXmlDataObjInp, 0);
		if (rei->status < 0)
		{
			rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: input xmlObj error. status = %d", rei->status);
			return (rei->status);
		}

		/******************************** OPEN AND READ FROM XML OBJECT ********************************/

		/* Open XML file */
		if ((xmlObjID = rsDataObjOpen(rsComm, &xmlDataObjInp)) < 0) 
		{
			rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: Cannot open XML data object. status = %d", xmlObjID);
			return (xmlObjID);
		}

		/* Get size of XML file */
		rei->status = rsObjStat (rsComm, &xmlDataObjInp, &rodsObjStatOut);
		if (rei->status < 0 || !rodsObjStatOut)
		{
			rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: Cannot stat XML data object. status = %d", rei->status);
			return (rei->status);
		}

		/* xmlBuf init */
		/* memory for xmlBuf->buf is allocated in rsFileRead() */
		xmlBuf = (bytesBuf_t *) malloc (sizeof (bytesBuf_t));
		memset (xmlBuf, 0, sizeof (bytesBuf_t));

		/* Read XML file */
		memset (&openedDataObjInp, 0, sizeof (openedDataObjInp_t));
		openedDataObjInp.l1descInx = xmlObjID;
		openedDataObjInp.len = (int)rodsObjStatOut->objSize;

		rei->status = rsDataObjRead (rsComm, &openedDataObjInp, xmlBuf);

		xmlXmlChar = xmlCharStrndup((char*)xmlBuf->buf, xmlBuf->len);

		/* Close XML file */
		rei->status = rsDataObjClose (rsComm, &openedDataObjInp);
		/* cleanup */
		freeRodsObjStat (rodsObjStatOut);

	} else {
		rei->status = USER_PARAM_TYPE_ERR;
		return (rei->status);
	}
	/******************************** PARSE XML DOCUMENT ********************************/

	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;

	
	/* Parse xmlBuf.buf into an xmlDocPtr */
	doc = xmlParseDoc(xmlXmlChar);
	free(xmlXmlChar);

	/* Create xpath evaluation context */
	xpathCtx = xmlXPathNewContext(doc);
	if(xpathCtx == NULL) 
	{
		rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: Unable to create new XPath context.");
		xmlFreeDoc(doc); 
		return(-1);
	}


	/* Evaluate xpath expression */
	xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
	if(xpathObj == NULL) 
	{
		rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: Unable to evaluate XPath expression \"%s\".", xpathExpr);
		xmlXPathFreeContext(xpathCtx); 
		xmlFreeDoc(doc); 
		return(-1);
	}

	/* How many AVU nodes did we get? */
	if ((nodes = xpathObj->nodesetval) != NULL)
	{
		avuNbr = nodes->nodeNr;
	}
	else 
	{
		avuNbr = 0;
	}



	/******************************** CREATE AVU TRIPLETS ********************************/

	/* Add a new AVU for each node. It's ok to process the nodes in forward order since we're not modifying them */
	for(i = 0; i < avuNbr; i++) 
	{
		if (nodes->nodeTab[i])
		{

			attrName = (char*)xmlNodeGetContent(getChildNodeByName(nodes->nodeTab[i], attribute));
			if (!attrName) {
				rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: AVU does not contain an Attribute element");
				continue;
			}
			attrNameLen = strlen(attrName);
			if (attrNameLen > MAX_ATTR_NAME_LEN) {
				rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: attribute name for AVU #%d is too large (%d>%d)",
					i + 1, attrNameLen, MAX_ATTR_NAME_LEN, attrName);
				continue;
			} else if (attrNameLen == 0) {
				rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: attribute name for AVU #%d is empty",
					i + 1);
			}

			attrValue = (char*)xmlNodeGetContent(getChildNodeByName(nodes->nodeTab[i], value));
			if (!attrValue) {
				rodsLog(LOG_ERROR, "msiLoadMetadataFromXml: AVU #%d does not contain a Value element",
					i + 1);
				continue;
			}
			attrValueLen = strlen(attrValue);
			if (attrValueLen > MAX_ATTR_VALUE_LEN) {
				rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: attribute value is too large (%d>%d) - %s",
					attrValueLen, MAX_ATTR_VALUE_LEN, attrValue);
				continue;
			} else if (attrValueLen == 0) {
				rodsLog(LOG_ERROR, "msiLoadMetadataFromXml: attribute value in AVU #%d with name '%s' is empty",
					i + 1, attrName);
				continue;
			}
	
			attrUnit = (char*)xmlNodeGetContent(getChildNodeByName(nodes->nodeTab[i], unit));
			/* attrUnit can be null */
			if (attrUnit) {
				attrUnitLen = strlen(attrUnit);
				if (attrUnitLen > MAX_ATTR_UNIT_LEN) {
					rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: attribute unit in AVU #%d is too large (%d>%d)",
					i + 1, attrUnitLen, MAX_ATTR_UNIT_LEN, attrUnit);
					continue;
				}
			}

			/* init modAVU input */
			memset (&modAVUMetadataInp, 0, sizeof(modAVUMetadataInp_t));
			modAVUMetadataInp.arg0 = add;

			/* Use target object if one was provided, otherwise look for it in the XML doc */
			if (myTargetObjInp->objPath != NULL && strlen(myTargetObjInp->objPath) > 0)
			{
				modAVUMetadataInp.arg2 = myTargetObjInp->objPath;
			}
			else
			{
				modAVUMetadataInp.arg2 = xmlURIUnescapeString((char*)xmlNodeGetContent(getChildNodeByName(nodes->nodeTab[i], target)),
						MAX_NAME_LEN, NULL);
			}

			/* check if data or collection */
			if (isColl(rsComm, modAVUMetadataInp.arg2, &coll_id) < 0) {
				modAVUMetadataInp.arg1 = dataObject;
			} else {
				modAVUMetadataInp.arg1 = collection;
			}

			modAVUMetadataInp.arg3 = attrName;

			modAVUMetadataInp.arg4 = attrValue;
			modAVUMetadataInp.arg5 = attrUnit;			
			/* invoke rsModAVUMetadata() */
			rei->status = rsModAVUMetadata (rsComm, &modAVUMetadataInp);
			if(rei->status < 0)
			{
				rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: rsModAVUMetadata error for %s, status=%d",
						modAVUMetadataInp.arg2, rei->status);
			}
		}
	}



	/************************************** WE'RE DONE **************************************/

	/* cleanup of all xml doc */
	xmlFreeDoc(doc);
        /* Don't call xmlCleanupParser when libxml2 is used again.
 	 	 xmlCleanupParser(); 
	*/

	return (rei->status);
}


/* Default stub if no XPath support */
#else
/**
 * \fn msiLoadMetadataFromXml (msParam_t *targetObj, msParam_t *xmlObj, ruleExecInfo_t *rei)
**/
int 
msiLoadMetadataFromXml(msParam_t *targetObj, msParam_t *xmlObj, ruleExecInfo_t *rei) 
{
	rodsLog (LOG_ERROR, "msiLoadMetadataFromXml: XPath support not compiled in.");
	return (SYS_NOT_SUPPORTED);
}
#endif

irods::ms_table_entry * plugin_factory() {
    irods::ms_table_entry* msvc = new irods::ms_table_entry(2);
    msvc->add_operation("msiLoadMetadataFromXml","msiLoadMetadataFromXml");
    return msvc;
}

}
