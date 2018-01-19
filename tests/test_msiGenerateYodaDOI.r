testRule {
	msiGenerateYodaDOI(*datacitePrefixIn, *yodaPrefixIn, *yodaDoiOut);
	writeLine("stdout", "Generated Yoda minted DOI: *yodaDoiOut");	
}
input *datacitePrefixIn="", *yodaPrefixIn=""
output ruleExecOut
