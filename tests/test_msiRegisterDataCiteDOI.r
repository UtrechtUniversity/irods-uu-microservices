testRule {
	*err = msiRegisterDataCiteDOI(*url, *username, *password, *xml);
	writeLine("stdout", "Registered DOI with DataCite: *err");	
}
input *url="", *username="", *password="", *xml=""
output ruleExecOut
