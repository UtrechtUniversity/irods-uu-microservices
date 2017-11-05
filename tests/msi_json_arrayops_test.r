myTestRule {

   *json_str = '[]';

   *item1 = 'test & and %';
   *item2 = 'U123456-ru.nl';
   *item3 = '{"type":"PMID","id":"12345"}';
   *item4 = '{"type":"arXiv","id":"xyz/abc"}';
   *item5 = '1';
   *item6 = 'true';
   *item7 = 'false';
   *item8 = '["DICOM","test"]';

   ## build JSON array
   *size = 0;
   *ec = errorcode( msi_json_arrayops(*json_str, *item1, "add", *size) );
   #*ec = errorcode( msi_json_arrayops(*json_str, *item2, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item3, "add", *size) );
   #*ec = errorcode( msi_json_arrayops(*json_str, *item4, "add", *size) );
   #*ec = errorcode( msi_json_arrayops(*json_str, *item6, "add", *size) );
   #*ec = errorcode( msi_json_arrayops(*json_str, *item7, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item8, "add", *size) );

    # adding null does not supported? 
   *ec = errorcode( msi_json_arrayops(*json_str, *item5, "add", *size) );

    # adding those will append additional boolean values to array 
   *ec = errorcode( msi_json_arrayops(*json_str, *item6, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item7, "add", *size) );

    # adding those will not change array because they have been presented in it
   *ec = errorcode( msi_json_arrayops(*json_str, *item1, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item2, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item3, "add", *size) );
   *ec = errorcode( msi_json_arrayops(*json_str, *item4, "add", *size) );

   writeLine("stdout", *json_str ++ " size: " ++ *size);

   ## remove JSON object (rm/find)
   *ec = errorcode( msi_json_arrayops(*json_str, *item3, "rm", *size) );
   writeLine("stdout", *json_str ++ " size: " ++ *size);

   #*idx = 0;
   #*ec = errorcode( msi_json_arrayops(*json_str, *item5, "find", *idx) );
   #writeLine("stdout", *json_str ++ " " ++ *item5 ++ " at idx: " ++ str(*idx));

   #*ec = errorcode( msi_json_arrayops(*json_str, *item4, "find", *idx) );
   #writeLine("stdout", *json_str ++ " " ++ *item4 ++ " at idx: " ++ str(*idx));

   #*ec = errorcode( msi_json_arrayops(*json_str, *item1, "find", *idx) );
   #writeLine("stdout", *json_str ++ " " ++ *item1 ++ " at idx: " ++ str(*idx));

   #*ec = errorcode( msi_json_arrayops(*json_str, "", "size", *idx) );
   #writeLine("stdout", *json_str ++ " size: " ++ str(*idx));

   *mysize = 0;
   *ec = errorcode( msi_json_arrayops(*json_str, "", "size", *mysize) );
   writeLine("stdout", *json_str ++ " size: " ++ str(*mysize));

   *idx = 1;
   *item1 = "";
   *ec = errorcode( msi_json_arrayops(*json_str, *item1, "get", *idx) );
   writeLine("stdout", *json_str ++ " " ++ *item1 ++ " at idx: " ++ str(*idx));

}
OUTPUT ruleExecOut
