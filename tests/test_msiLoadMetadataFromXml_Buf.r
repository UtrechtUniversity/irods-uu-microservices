testRule {
	if (*objPath=="") {
		writeLine("stdout", "\*objPath is a required parameter");
		fail;
	}

	msiGetIcatTime(*timestamp, 'unix');
	*str = 
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<metadata>
  <AVU>
    <Attribute>TestAttribute1</Attribute>
    <Value>TestValue *timestamp</Value>
  </AVU>
  <AVU>
  <Attribute></Attribute>
  <Value></Value>
  </AVU>
  <AVU>
    <Attribute>TestAttribute2</Attribute>
    <Value>TestValue *timestamp</Value>
    <Unit>km/h</Unit>
  </AVU>
</metadata>"

	msiStrToBytesBuf(*str, *buf);
	msiLoadMetadataFromXml(*objPath, *buf);
}
input *objPath=""
output ruleExecOut
