testRule {
	if (*xsltObj=="" || *xmlObj=="") {
		writeLine("stdout", "\*xsltObj and \*xmlObj are required parameters");
	} else {
		msiXsltApply(*xsltObj, *xmlObj, *outputBuf);
		writeBytesBuf("stdout", *outputBuf);
	}
}
input *xsltObj="", *xmlObj=""
output ruleExecOut
