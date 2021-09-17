# include <archive.h>
# include <archive_entry.h>
# include <jansson.h>

# include "rsDataObjCreate.hpp"
# include "rsDataObjOpen.hpp"
# include "rsDataObjRead.hpp"
# include "rsDataObjWrite.hpp"
# include "rsDataObjClose.hpp"
# include "rsCollCreate.hpp"
# include "rcMisc.h"

# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <string.h>

# define A_BUFSIZE	(1024 * 1024)
# define A_BLOCKSIZE	((size_t) 8192)

class Archive {
    struct Data {
	rsComm_t *rsComm;
	const char *name;
	const char *resource;
	int index;
	char buf[A_BUFSIZE];
    };

    Archive(struct archive *archive, Data *data, bool creating, json_t *list,
	    size_t dataSize, std::string &path, std::string &collection,
	    std::string &resc, std::string indexString) :
	archive(archive),
	data(data),
	creating(creating),
	list(list),
	dataSize(dataSize),
	path(path),
	origin(collection),
	resc(resc),
	indexString(indexString)
    {
	data->resource = (this->resc.length() != 0) ? this->resc.c_str() : NULL;
	index = 0;
    }

public:
    /*
     * create archive
     */
    static Archive *create(rsComm_t *rsComm, std::string path,
			   std::string collection, std::string resc) {
	struct archive *a;
	Data *data;

	a = archive_write_new();
	if (a == NULL) {
	    return NULL;
	}
	if (archive_write_set_format_filter_by_ext(a, path.c_str()) !=
								ARCHIVE_OK) {
	    archive_write_add_filter_gzip(a);
	    archive_write_set_format_ustar(a);
	}
	data = new Data;
	data->rsComm = rsComm;
	data->name = path.c_str();
	data->resource = (resc.length() != 0) ? resc.c_str() : NULL;
	if (archive_write_open(a, data, &a_creat, &a_write, &a_close) !=
								ARCHIVE_OK) {
	    delete data;
	    archive_write_free(a);
	    return NULL;
	}

	return new Archive(a, data, true, json_array(), 0, path, collection,
			   resc, "");
    }

    /*
     * open existing archive
     */
    static Archive *open(rsComm_t *rsComm, std::string path, std::string resc) {
	struct archive *a;
	Data *data;
	struct archive_entry *entry;
	size_t size;
	char *buf;
	json_t *json, *list;
	json_error_t error;
	std::string origin;
	Archive *archive;

	a = archive_read_new();
	if (a == NULL) {
	    return NULL;
	}
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);
	data = new Data;
	data->rsComm = rsComm;
	data->name = path.c_str();
	if (archive_read_open(a, data, &a_open, &a_read, &a_close) !=
								ARCHIVE_OK ||
	    archive_read_next_header(a, &entry) != ARCHIVE_OK) {
	    delete data;
	    archive_read_free(a);
	    return NULL;
	}

	// read INDEX.json
	if (strcmp(archive_entry_pathname(entry), "INDEX.json") != 0) {
	    delete data;
	    archive_read_free(a);
	    return NULL;
	}

	size = (size_t) archive_entry_size(entry);
	buf = new char[size + 1];
	buf[size] = '\0';
	if (archive_read_data(a, buf, size) != (la_ssize_t) size ||
	    (json=json_loads(buf, 0, &error)) == NULL) {
	    delete buf;
	    delete data;
	    archive_read_free(a);
	    return NULL;
	}

	// get list of items from json
	origin = json_string_value(json_object_get(json, "collection"));
	size = (size_t) json_integer_value(json_object_get(json, "size"));
	list = json_object_get(json, "items");
	json_incref(list);
	json_decref(json);

	archive = new Archive(a, data, false, list, size, path, origin, resc,
			      buf);
	delete buf;
	return archive;
    }

    ~Archive() {
	if (archive != NULL) {
	    if (creating) {
		archive_write_free(archive);
	    } else {
		archive_read_free(archive);
	    }
	}

	json_decref(list);
	delete data;
    }

    void addDataObj(std::string name, size_t size, time_t created,
		    time_t modified, std::string owner, std::string zone,
		    std::string checksum, json_t *attributes) {
	if (path.compare(origin + "/" + name) != 0) {
	    json_t *json;

	    json = json_object();
	    json_object_set_new(json, "name", json_string(name.c_str()));
	    json_object_set_new(json, "type", json_string("dataObj"));
	    json_object_set_new(json, "size", json_integer((json_int_t) size));
	    json_object_set_new(json, "created", json_integer(created));
	    json_object_set_new(json, "modified", json_integer(modified));
	    json_object_set_new(json, "owner",
				json_string((owner + "#" + zone).c_str()));
	    if (checksum.length() != 0) {
		json_object_set_new(json, "checksum",
				    json_string(checksum.c_str()));
	    }
	    if (attributes != NULL) {
		json_object_set(json, "attributes", attributes);
	    }
	    json_array_append_new(list, json);

	    dataSize += (size + A_BLOCKSIZE - 1) & ~(A_BLOCKSIZE - 1);
	}
    }

    void addColl(std::string name, time_t created, time_t modified,
		 std::string owner, std::string zone, json_t *attributes) {
	json_t *json;

	json = json_object();
	json_object_set_new(json, "name", json_string(name.c_str()));
	json_object_set_new(json, "type", json_string("coll"));
	json_object_set_new(json, "created", json_integer(created));
	json_object_set_new(json, "modified", json_integer(modified));
	json_object_set_new(json, "owner",
			    json_string((owner + "#" + zone).c_str()));
	if (attributes != NULL) {
	    json_object_set(json, "attributes", attributes);
	}
	json_array_append_new(list, json);
    }

    std::string indexItems() {
	return indexString;
    }

    size_t size() {
	return dataSize;
    }

    json_t *nextItem() {
	// get next item (potentially skipping current) from archive
	if (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
	    return json_array_get(list, index++);
	} else {
	    return NULL;
	}
    }

    int extractItem(std::string filename) {
	// extract current object
	if (archive_entry_filetype(entry) == AE_IFDIR) {
	    collInp_t collCreateInp;

	    memset(&collCreateInp, '\0', sizeof(collInp_t));
	    rstrcpy(collCreateInp.collName, filename.c_str(), MAX_NAME_LEN);
	    return rsCollCreate(data->rsComm, &collCreateInp);
	} else {
	    char buf[A_BUFSIZE];
	    int fd, status;
	    la_ssize_t len;

	    fd = _creat(data->rsComm, filename.c_str(), data->resource);
	    if (fd < 0) {
		return fd;
	    }
	    while ((len=archive_read_data(archive, buf, sizeof(buf))) > 0) {
		status = _write(data->rsComm, fd, buf, (size_t) len);
		if (status < 0) {
		    _close(data->rsComm, fd);
		    return status;
		}
	    }
	    if (len < 0) {
		_close(data->rsComm, fd);
		return SYS_TAR_EXTRACT_ALL_ERR;
	    }
	    return _close(data->rsComm, fd);
	}

	return 0;
    }

    int construct() {
	if (creating) {
	    json_t *json;
	    char *str;
	    la_ssize_t len;

	    json = json_object();
	    json_object_set_new(json, "collection",
				json_string(origin.c_str()));
	    json_object_set_new(json, "size",
				json_integer((json_int_t) dataSize));
	    json_object_set(json, "items", list);
	    str = json_dumps(json, JSON_INDENT(2));
	    json_decref(json);

	    len = (la_ssize_t) strlen(str);
	    entry = archive_entry_new();
	    archive_entry_set_pathname(entry, "INDEX.json");
	    archive_entry_set_filetype(entry, AE_IFREG);
	    archive_entry_set_perm(entry, 0444);
	    archive_entry_set_size(entry, len);
	    if (archive_write_header(archive, entry) < 0 ||
		archive_write_data(archive, str, (size_t) len) < 0) {
		free(str);
		return SYS_TAR_APPEND_ERR;
	    }
	    free(str);

	    for (index = 0; index < json_array_size(list); index++) {
		const char *filename;
		int fd;
		size_t size;
		time_t mtime;

		entry = archive_entry_new();
		json = json_array_get(list, index);
		filename = json_string_value(json_object_get(json, "name"));
		mtime = json_integer_value(json_object_get(json, "modified"));
		archive_entry_set_pathname(entry, filename);
		archive_entry_set_mtime(entry, mtime, 0);
		if (strcmp(json_string_value(json_object_get(json, "type")),
			   "coll") == 0) {
		    archive_entry_set_filetype(entry, AE_IFDIR);
		    archive_entry_set_perm(entry, 0750);
		    if (archive_write_header(archive, entry) < 0) {
			return SYS_TAR_APPEND_ERR;
		    }
		} else {
		    archive_entry_set_filetype(entry, AE_IFREG);
		    archive_entry_set_perm(entry, 0600);
		    size = (size_t) json_integer_value(json_object_get(json,
								       "size"));
		    archive_entry_set_size(entry, (la_int64_t) size);
		    if (archive_write_header(archive, entry) < 0) {
			return SYS_TAR_APPEND_ERR;
		    }
		    fd = _open(data->rsComm, (origin + "/" + filename).c_str());
		    if (fd < 0) {
			return fd;
		    }
		    while ((len=_read(data->rsComm, fd, data->buf,
				      sizeof(data->buf))) > 0) {
			if (archive_write_data(archive, data->buf,
					       (size_t) len) < 0) {
			    _close(data->rsComm, fd);
			    return SYS_TAR_APPEND_ERR;
			}
		    }
		    if (len < 0) {
			_close(data->rsComm, fd);
			return SYS_TAR_APPEND_ERR;
		    }
		    _close(data->rsComm, fd);
		}
	    }

	    archive_write_free(archive);
	    archive = NULL;
	}

	return 0;
    }

private:
    static int _creat(rsComm_t *rsComm, const char *name,
		      const char *resource) {
	dataObjInp_t input;

	memset(&input, '\0', sizeof(dataObjInp_t));
	rstrcpy(input.objPath, name, MAX_NAME_LEN);
	input.openFlags = O_CREAT | O_WRONLY | O_TRUNC;
	if (resource != NULL) {
	    addKeyVal(&input.condInput, DEST_RESC_NAME_KW, resource);
	}
	addKeyVal(&input.condInput, FORCE_FLAG_KW, "");
	return rsDataObjCreate(rsComm, &input);
    }

    static int _open(rsComm_t *rsComm, const char *name) {
	dataObjInp_t input;

	memset(&input, '\0', sizeof(dataObjInp_t));
	rstrcpy(input.objPath, name, MAX_NAME_LEN);
	input.openFlags = O_RDONLY;
	return rsDataObjOpen(rsComm, &input);
    }

    static int _read(rsComm_t *rsComm, int index, void *buf, size_t len) {
	openedDataObjInp_t input;
	bytesBuf_t rbuf;

	memset(&input, '\0', sizeof(openedDataObjInp_t));
	input.l1descInx = index;
	input.len = (int) len;
	rbuf.buf = buf;
	rbuf.len = (int) len;
	return rsDataObjRead(rsComm, &input, &rbuf);
    }

    static int _write(rsComm_t *rsComm, int index, const void *buf, size_t len)
    {
	openedDataObjInp_t input;
	bytesBuf_t wbuf;

	memset(&input, '\0', sizeof(openedDataObjInp_t));
	input.l1descInx = index;
	input.len = (int) len;
	wbuf.buf = (void *) buf;
	wbuf.len = (int) len;
	return rsDataObjWrite(rsComm, &input, &wbuf);
    }

    static int _close(rsComm_t *rsComm, int index) {
	openedDataObjInp_t input;

	memset(&input, '\0', sizeof(openedDataObjInp_t));
	input.l1descInx = index;
	return rsDataObjClose(rsComm, &input);
    }

    static int a_creat(struct archive *a, void *data) {
	Data *d;

	d = (Data *) data;
	d->index = _creat(d->rsComm, d->name, d->resource);
	return (d->index >= 0) ? ARCHIVE_OK : ARCHIVE_FATAL;
    }

    static int a_open(struct archive *a, void *data) {
	Data *d;

	d = (Data *) data;
	d->index = _open(d->rsComm, d->name);
	return (d->index >= 0) ? ARCHIVE_OK : ARCHIVE_FATAL;
    }

    static la_ssize_t a_read(struct archive *a, void *data, const void **buf) {
	Data *d;
	la_ssize_t status;

	d = (Data *) data;
	if (d->index < 0 ||
	    (status=_read(d->rsComm, d->index, d->buf, sizeof(d->buf))) < 0) {
	    return -1;
	} else {
	    *buf = d->buf;
	    return status;
	}
    }

    static la_ssize_t a_write(struct archive *a, void *data, const void *buf,
			      size_t size) {
	Data *d;
	la_ssize_t status;

	d = (Data *) data;
	if (d->index < 0 ||
	    (status=_write(d->rsComm, d->index, buf, size)) < 0) {
	    return -1;
	} else {
	    return status;
	}
    }

    static int a_close(struct archive *a, void *data) {
	Data *d;

	d = (Data *) data;
	return (d->index < 0 || _close(d->rsComm, d->index) < 0) ? -1 : 0;
    }

    struct archive *archive;	// libarchive reference
    struct archive_entry *entry;// archive entry
    Data *data;
    bool creating;		// new archive?
    json_t *list;		// list of items
    size_t index;		// item index
    size_t dataSize;		// total size of archived data objects
    std::string path;		// path of archive
    std::string origin;		// original collection
    std::string resc;		// resource
    std::string indexString;
};
