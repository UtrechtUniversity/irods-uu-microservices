// =-=-=-=-=-=-=-
#include "irods_error.hpp"
#include "irods_ms_plugin.hpp"

#include "jansson.h"

// =-=-=-=-=-=-=-
// STL/boost Includes
#include <string>
#include <iostream>
#include <vector>
#include <boost/algorithm/string.hpp>

extern "C" {
// =-=-=-=-=-=-=-
int msi_json_objops(msParam_t* json_str, msParam_t* kvp, msParam_t* ops, ruleExecInfo_t* rei)
{
    using std::cout;
    using std::endl;
    using std::string;

    char* null_cstr = new char[5];
    char* true_cstr = new char[5];
    char* false_cstr = new char[6];

    strcpy(null_cstr, "null");
    strcpy(true_cstr, "true");
    strcpy(false_cstr, "false");

    // input type check
    const char* inJsonStr = parseMspForStr(json_str);
    const char* inOps = parseMspForStr(ops);
    if (!inJsonStr || !inOps) {
        cout << "msi_json_objops - invalid input for string" << endl;
        return SYS_INVALID_INPUT_PARAM;
    }

    if (kvp == NULL || kvp->inOutStruct == NULL || kvp->type == NULL || strcmp(kvp->type, KeyValPair_MS_T) != 0) {
        cout << "msi_json_objops - invalid input for key-value pairs" << endl;
        return SYS_INVALID_INPUT_PARAM;
    }

    keyValPair_t* inKVP;
    inKVP = (keyValPair_t*) kvp->inOutStruct;

    string strOps(inOps);

    json_error_t error;
    json_t* root;

    // try to make initial inJsonStr if it's an empty string
    if (strcmp(inJsonStr, "") == 0) {
        inJsonStr = "{}";
    }

    // try to load JSON object
    root = json_loads(inJsonStr, 0, &error);
    if (!root) {
        cout << "msi_json_objops - invalid json document" << endl;
        json_decref(root);
        return SYS_INVALID_INPUT_PARAM;
    }

    for (int ik = 0; ik < inKVP->len; ik++) {
        char* inKey = inKVP->keyWord[ik];
        char* inVal = inKVP->value[ik];

        json_t* jval;
        if (strcmp(inVal, null_cstr) == 0) {
            jval = json_null();
        }
        else {
            jval = json_loads(inVal, 0, &error);
            if (!jval)
                jval = json_string(inVal);
        }

        // try to find objects in the key
        json_t* data = json_object_get(root, inKey);

        if (strOps == "get") {
            if (json_is_null(data)) {
                inKVP->value[ik] = null_cstr;
            }
            else if (json_is_true(data)) {
                inKVP->value[ik] = true_cstr;
            }
            else if (json_is_false(data)) {
                inKVP->value[ik] = false_cstr;
            }
            else if (json_is_string(data)) {
                const char* val_str = json_string_value(data);
                char* val_cstr = new char[strlen(val_str) + 1];
                strcpy(val_cstr, val_str);
                inKVP->value[ik] = val_cstr;
            }
            else {
                inKVP->value[ik] = json_dumps(data, 0);
            }
        }
        else if (strOps == "add") {
            if (json_is_array(data)) {
                json_array_append_new(data, jval);
            }
            else {
                json_object_set_new(root, inKey, jval);
            }
        }
        else if (strOps == "set") {
            json_object_set_new(root, inKey, jval);
        }
        else if (strOps == "rm") {
            if (data) {
                if (json_is_array(data)) {
                    size_t i_match = json_array_size(data);
                    for (int i = 0; i < json_array_size(data); i++) {
                        json_t* ov = json_array_get(data, i);
                        if (json_equal(ov, jval)) {
                            i_match = i;
                            break;
                        }
                    }
                    if (i_match < json_array_size(data))
                        json_array_remove(data, i_match);
                }
                else if (json_equal(data, jval)) {
                    json_object_del(root, inKey);
                }
            }
        }
    }

    kvp->inOutStruct = (void*) inKVP;
    kvp->type = (char*) strdup(KeyValPair_MS_T);

    fillStrInMsParam(json_str, json_dumps(root, 0));

    json_decref(root);

    // Done
    return 0;
}

irods::ms_table_entry* plugin_factory()
{
    irods::ms_table_entry* msvc = new irods::ms_table_entry(3);

    msvc->add_operation<msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*>(
        "msi_json_objops", std::function<int(msParam_t*, msParam_t*, msParam_t*, ruleExecInfo_t*)>(msi_json_objops));

    return msvc;
}

} // extern "C"
