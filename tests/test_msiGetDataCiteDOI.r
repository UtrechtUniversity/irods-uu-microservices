testRule {

	msiGetDataCiteDOI(*url, *username, *password, *result, *httpCode);
	writeLine("stdout", *result);
	writeLine("stdout", *httpCode);

}
input *url="", *username="", *password=""
output ruleExecOut
