#include "jansson.h"

# define CREDS_STORE	"/var/lib/irods/.credentials_store/store_config.json"


/*
 * keep it simple
 */
class CredentialsStore {
public:
    // load stored values
    CredentialsStore() {
	json_error_t error;

	store = json_load_file(CREDS_STORE, 0, &error);
	// store is returned with refcount 1
    }
    // free stored values
    ~CredentialsStore() {
	if (store != NULL) {
	    json_decref(store);
	}
    }

    // get a credential from the store
    const char *get(const char *key) {
	if (store != NULL) {
	    json_t *value;

	    value = json_object_get(store, key);
	    if (value != NULL && json_is_string(value)) {
		return json_string_value(value);
	    }
	}

	return NULL;
    }

private:
    json_t *store;	// in-memory copy of credentials store
};
