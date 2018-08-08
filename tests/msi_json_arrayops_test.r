myTestRule {

   *json_str = '[]';

   *item1 = 'test & and %';
   *item2 = 'U123456-ru.nl';
   *item3 = '{"type":"PMID","id":"12345"}';
   *item4 = '{"type":"arXiv","id":"xyz/abc"}';
   *item5 = 'null';
   *item6 = 'true';
   *item7 = 'false';
   *item8 = '["DICOM","test"]';

   ## build JSON array
   *size = 0;
   *ec = errorcode( msi_json_arrayops(*json_str, *item1, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item2, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item3, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item4, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item8, "add", *size) );
   writeLine("stdout", "Check 1: adding new values to JSON array. Outcome:");
   writeLine("stdout", "    *json_str size: *size");

   # adding 'null' is not supported and will not extend the JSON array
   *ec = errorcode( msi_json_arrayops(*json_str, *item5, "add", *size) );
   writeLine("stdout", "Check 2: adding 'null' value to JSON array. Outcome:");
   writeLine("stdout", "    *json_str size: *size");

   # adding those will append additional boolean values to array
   *ec = errorcode( msi_json_arrayops(*json_str, *item6, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item7, "add", *size) );
   writeLine("stdout", "Check 3: adding boolean values to JSON array. Outcome:");
   writeLine("stdout", "    *json_str size: *size");

   # adding those will not change array because they are already present in it
   *ec = errorcode( msi_json_arrayops(*json_str, *item1, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item2, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item3, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item4, "add", *size) );
   writeLine("stdout", "Check 4: adding pre-existing values to JSON array. Outcome:");
   writeLine("stdout", "    *json_str size: *size");

   # remove JSON object (rm/find)
   *ec = errorcode( msi_json_arrayops(*json_str, *item3, "rm", *size) );
   writeLine("stdout", "Check 5: removing 1 object from JSON array. Outcome:");
   writeLine("stdout", "    *json_str size: *size");

   # find operations
   *idx = 0;
   *ec = errorcode( msi_json_arrayops(*json_str, *item4, "find", *idx) );
   writeLine("stdout", "Check 6: Finding index of object in JSON array. Outcome:");
   writeLine("stdout", "    *json_str value '*item4' at idx: *idx");

   *ec = errorcode( msi_json_arrayops(*json_str, *item5, "find", *idx) );
   writeLine("stdout", "Check 7: Finding index of object in JSON array. Outcome:");
   writeLine("stdout", "    *json_str value '*item5' at idx: *idx");

   *ec = errorcode( msi_json_arrayops(*json_str, *item1, "find", *idx) );
   writeLine("stdout", "Check 8: Finding index of object in JSON array. Outcome:");
   writeLine("stdout", "    *json_str value '*item1' at idx: *idx");

   # size operation
   *ec = errorcode( msi_json_arrayops(*json_str, "", "size", *idx) );
   writeLine("stdout", "Check 9: Determine size of JSON array. Outcome:");
   writeLine("stdout", "    *json_str max idx: *idx");

   # get operation
   *idx = 0;
   *item11 = "";
   *ec = errorcode( msi_json_arrayops(*json_str, *item11, "get", *idx) );
   writeLine("stdout", "Check 10: Get an object from the JSON array. Outcome:");
   writeLine("stdout", "    *json_str value '*item11' retrieved from idx: *idx");

}

OUTPUT ruleExecOut

