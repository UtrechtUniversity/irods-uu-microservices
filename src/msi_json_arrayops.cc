// =-=-=-=-=-=-=-
#include "irods_error.hpp"
#include "irods_ms_plugin.hpp"

#include "jansson.h"

// =-=-=-=-=-=-=-
// STL/boost Includes
#include <string>
#include <iostream>
#include <vector>
//#include <boost/algorithm/string.hpp>

extern "C" {
    // =-=-=-=-=-=-=-
    int msi_json_arrayops(msParam_t* json_str, msParam_t* val, msParam_t* ops, msParam_t* sizeOrIndex, ruleExecInfo_t* rei) {
        using std::cout;
        using std::endl;
        using std::string;

        // input type check
        const char *inJsonStr = parseMspForStr( json_str );
        const char *inVal = parseMspForStr( val );
        const char *inOps = parseMspForStr( ops );
        const int inIdx = parseMspForPosInt( sizeOrIndex );

        if( ! inJsonStr ) {
            cout << "msi_json_arrayops - invalid inJsonStr" << endl;
            return SYS_INVALID_INPUT_PARAM;
        }
        if( ! inOps ) {
            cout << "msi_json_arrayops - invalid inOps" << endl;
            return SYS_INVALID_INPUT_PARAM;
        }
        if( ! inVal ) {
            cout << "msi_json_arrayops - invalid inVal" << endl;
            return SYS_INVALID_INPUT_PARAM;
        }

        string strOps(inOps); 

        json_error_t error;
        json_t *root;

        // try to make initial inJsonStr if it's an empty string 
        if ( strcmp(inJsonStr, "") == 0 ) {
            inJsonStr = "[]";
        }

        // try to load JSON object
        root = json_loads(inJsonStr, 0, &error);
        if ( ! root || ! json_is_array(root) ) {
            cout << "msi_json_arrayops - invalid json document" << endl;
            json_decref(root);
            return SYS_INVALID_INPUT_PARAM;
        }

        int outSizeOrIndex = (int) json_array_size(root);
        json_t *jval;
        if (strcmp(inVal, "null") == 0) {
            jval = json_null();
        } else if (strcmp(inVal, "true") == 0 ) {
            jval = json_true();
        } else if (strcmp(inVal, "false") == 0 ) {
            jval = json_false();
        } else {
            jval = json_loads(inVal, 0, &error);
            if ( ! jval ) jval = json_string(inVal);
        }

        // find if jval is already presented in array
        size_t i_match = outSizeOrIndex;
        for( int i=0; i < outSizeOrIndex; i++ ) {
            json_t *ov = json_array_get(root, i);
            if ( json_equal(ov, jval) ) { i_match = i; break; }
        }

        if ( strOps == "add" ) {
            // append value only if it's a boolean, or it's not presented in the array
            if ( json_is_boolean(jval) || i_match == outSizeOrIndex ) {
                json_array_append_new(root, jval);
                outSizeOrIndex = (int) json_array_size(root);
            }
        } else if ( strOps == "find" || strOps == "rm" ) {

            if ( i_match < outSizeOrIndex ) {
                if ( strOps == "rm" ) {
                    json_array_remove(root, i_match);
                    outSizeOrIndex = (int) json_array_size(root);
                } else {
                    outSizeOrIndex = i_match;
                }
            } else if ( strOps == "find" ) {
                outSizeOrIndex = -1;
            }
        } else if ( strOps == "size" ) {
            outSizeOrIndex = (int) json_array_size(root);
        } else if ( strOps == "get" ) {
            json_t *elem = json_array_get(root, inIdx);
            if (elem == NULL) {
                json_decref(root);
                return SYS_INVALID_INPUT_PARAM;
            }

	    /* output a string directly, but encode other json types using json_dumps with JSON_ENCODE_ANY set */
            if ( json_is_string(elem)) { 
                fillStrInMsParam(val, json_string_value(elem));
            } else {
                fillStrInMsParam(val, json_dumps(elem, JSON_ENCODE_ANY));
            }


            outSizeOrIndex = inIdx;
        }

        fillStrInMsParam(json_str, json_dumps(root, 0));
        fillIntInMsParam(sizeOrIndex, outSizeOrIndex);

        json_decref(root);

        // Done
        return 0;
    }

  irods::ms_table_entry* plugin_factory() {
    irods::ms_table_entry* msvc = new irods::ms_table_entry(4);
        
    msvc->add_operation<
      msParam_t*,
      msParam_t*,
      msParam_t*,
      msParam_t*,		  
      ruleExecInfo_t*>("msi_json_arrayops",
		       std::function<int(
					 msParam_t*,
					 msParam_t*,
					 msParam_t*,
					 msParam_t*,
					 ruleExecInfo_t*)>(msi_json_arrayops));

    return msvc;
  }

} // extern "C"
