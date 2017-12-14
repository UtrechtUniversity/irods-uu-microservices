# irods-uu-microservices
Miscellaneous iRODS microservices developed or modified by Utrecht university.

### Included microservices
Developed at the UU:
  * msiGetDataCiteDOI: API call to retrieve record of a DOI from DataCite 
  * msiRegisterDataCiteDOI: API call to register a DOI with metadata or to mint a DOI
  * msiStrToUpper: Returns an uppercase string
  * msiSetUpperCaseWhereQuery: Set the UPPERCASE flag on a irods query.
  * msiGenerateRandomID: Generate a Random ID to use as basis for a new DOI


Forward ported from iRODS 3:
  * msiLoadMetadataFromXml:
     load metadata from an xml file. Modified to accept both irods data objects as string buffers. Also skips AVU's exceeding the iRODS byte limit
  * msiXsltApply: apply a XSLT to and XML. Modified to handle invalid input without crashing and added exslt support
  * msiXmlDocSchemaValidate: Validate an XML against an XSD

Developed at Donders Institute:
  * msi\_json\_objops: get, add and set values in a json object
  * msi\_json\_arrayops: get, add and set values in a json array. modified to handle arrays of arrays
		
msi\_json\_arrayops and msi\_json\_objops microservices are derived from
ork from the Donders Institute. The license in DONDERS-LICENSE applies
