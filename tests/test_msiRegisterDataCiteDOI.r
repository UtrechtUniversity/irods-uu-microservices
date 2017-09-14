testRule {
	*err = msiRegisterDataCiteDOI(*url, *username, *password, *payload, *httpCode);
	if (*err < 0) {
	  writeLine("stdout", "Registered DOI with DataCite failed: *err");	
	} else { 
	  writeLine("stdout", "Registration returned HTTP CODE: *httpCode");
        }
}
input *url="", *username="", *password="", *payload=""
output ruleExecOut
