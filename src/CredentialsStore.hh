#include "jansson.h"
#include "irods_includes.hh"

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
	if (store == NULL) {
	    rodsLog(LOG_ERROR, "Failed to load credentials store");
	}
	// store is returned with refcount 1
    }
    // free stored values
    ~CredentialsStore() {
	if (store != NULL) {
	    json_decref(store);
	}
    }

    // check that the store is properly initialized
    bool isLoaded() {
	return (store != NULL);
    }

    // check that the store has a credential
    bool has(const char *key) {
	return (store != NULL && json_object_get(store, key) != NULL);
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

	rodsLog(LOG_ERROR, "Failed to retrieve credential \"%s\"", key);
	return NULL;
    }

private:
    json_t *store;	// in-memory copy of credentials store
};
