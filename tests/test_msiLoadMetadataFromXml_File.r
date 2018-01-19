testRule {
	if (*objPath=="" || *avuXml =="") {
		writeLine("stdout", "\*objPath and \*avuXml are a required parameters");
		fail;
	}

	msiLoadMetadataFromXml(*objPath, *avuXml);
}
input *objPath="", *avuXml=""
output ruleExecOut
