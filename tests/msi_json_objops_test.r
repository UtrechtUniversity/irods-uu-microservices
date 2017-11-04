myTestRule {

   *json_str = "";

   ## build JSON object from key-value pairs
   msiString2KeyValPair("", *kvp);
   msiAddKeyVal(*kvp, 'type', 'DATA_SHARING');
   msiAddKeyVal(*kvp, 'collId', '12345');
   msiAddKeyVal(*kvp, 'title', 'DICOM 原生資料備份');
   msiAddKeyVal(*kvp, 'creatorList', '["U505173-ru.nl","hurngchunlee-icloud.com"]');
   msiAddKeyVal(*kvp, 'associatedPublication', '[{"type":"PMID", "id":"654321"}]');

   *ec = errorcode( msi_json_objops(*json_str, *kvp, "add") );
   writeLine("stdout", *json_str);

   ## modify JSON object (add/set/get/rm)
   msiString2KeyValPair("", *kvp);
   msiAddKeyVal(*kvp, 'associatedPublication', '{"type":"arXiv", "id":"xyz/123"}');
   *ec = errorcode( msi_json_objops(*json_str, *kvp, "add") );
   writeLine("stdout", *json_str);
  
   msiString2KeyValPair("", *kvp);
   msiAddKeyVal(*kvp, 'associatedPublication', '{"type":"PMID", "id":"654321"}');
   *ec = errorcode( msi_json_objops(*json_str, *kvp, "rm") );
   writeLine("stdout", *json_str);

   msiString2KeyValPair("", *kvp);
   msiAddKeyVal(*kvp, 'creatorList', '["hurngchunlee-icloud.com","U505173-ru.nl"]');
   *ec = errorcode( msi_json_objops(*json_str, *kvp, "set") );
   writeLine("stdout", *json_str);

   msiString2KeyValPair("", *kvp);
   msiAddKeyVal(*kvp, 'creatorList', '');
   msiAddKeyVal(*kvp, 'associatedPublication', '');
   msiAddKeyVal(*kvp, 'collId', '');
   msiAddKeyVal(*kvp, 'title', '');
   #msiAddKeyVal(*kvp, 'associatedPublication', '');
   *ec = errorcode( msi_json_objops(*json_str, *kvp, "get") );
   writeLine("stdout", str(*kvp));
}
OUTPUT ruleExecOut

