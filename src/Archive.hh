# include <archive.h>
# include <archive_entry.h>
# include <jansson.h>

# include <string>

# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <string.h>

class Archive {
    struct Data {
	const char *name;
	int fd;
	char buf[8192];
    };

public:
    Archive(struct archive *archive, struct Data *data, bool creating,
	    json_t *list, std::string &path, std::string &collection) :
	archive(archive),
	data(data),
	creating(creating),
	list(list),
	path(path),
	origin(collection)
    {
	index = 0;
    }

    /*
     * create archive
     */
    static Archive *create(std::string path, std::string collection) {
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
	data->name = path.c_str();
	if (archive_write_open(a, data, &a_creat, &a_write, &a_close) !=
								ARCHIVE_OK) {
	    delete data;
	    archive_write_free(a);
	    return NULL;
	}

	return new Archive(a, data, true, json_array(), path, collection);
    }

    /*
     * open existing archive
     */
    static Archive *open(std::string path) {
	struct archive *a;
	Data *data;
	struct archive_entry *entry;
	size_t len;
	char *buf;
	json_t *json, *list;
	json_error_t error;
	std::string origin;

	a = archive_read_new();
	if (a == NULL) {
	    return NULL;
	}
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);
	data = new Data;
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

	len = archive_entry_size(entry);
	buf = new char[len + 1];
	archive_read_data(a, buf, len);
	buf[len] = '\0';
	json = json_loads(buf, 0, &error);
	delete buf;

	// get list of items from json
	origin = json_string_value(json_object_get(json, "collection"));
	list = json_object_get(json, "items");
	json_incref(list);
	json_decref(json);

	return new Archive(a, data, false, list, path, origin);
    }

    ~Archive() {
	if (creating) {
	    // if writing, create INDEX.json and actually add items
	    build();
	    archive_write_free(archive);
	} else {
	    archive_read_free(archive);
	}

	delete data;
    }

    void addItem(std::string path, json_t *metadata) {
	json_t *json;

	json = json_object();
	json_object_set_new(json, "path", json_string(path.c_str()));
	json_object_set(json, "metadata", metadata);
	json_array_append_new(list, json);
    }

    std::string nextItem() {
	// get next item (potentially skipping current) from archive
	index++;
	if (archive_read_next_header(archive, &entry) != ARCHIVE_OK) {
	    return "";
	}
	return archive_entry_pathname(entry);
    }

    void extractItem(std::string filename) {
	char buf[8192];
	mode_t filetype, mode;
	int fd;
	la_ssize_t len;

	// extract current object
	filetype = archive_entry_filetype(entry);
	mode = archive_entry_perm(entry);
	fd = ::open(filename.c_str(), O_CREAT | O_TRUNC | O_WRONLY, mode);
	while ((len=archive_read_data(archive, buf, sizeof(buf))) > 0) {
	    write(fd, buf, len);
	}
	close(fd);
    }

    json_t *metadata() {
	return json_array_get(list, index - 1);
    }

private:
    void build() {
	json_t *json;
	char *str;
	size_t len;

	json = json_object();
	json_object_set_new(json, "collection", json_string(origin.c_str()));
	json_object_set(json, "items", list);
	str = json_dumps(json, JSON_INDENT(2));
	json_decref(json);

	len = strlen(str);
	entry = archive_entry_new();
	archive_entry_set_pathname(entry, "INDEX.json");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, 0444);
	archive_entry_set_size(entry, len);
	archive_write_header(archive, entry);
	archive_write_data(archive, str, len);
	free(str);

	for (index = 0; index < json_array_size(list); index++) {
	    const char *filename;
	    struct stat st;
	    int fd;

	    entry = archive_entry_new();
	    json = json_array_get(list, index);
	    filename = json_string_value(json_object_get(json, "path"));
	    archive_entry_set_pathname(entry, filename);
	    stat(filename, &st);
	    archive_entry_set_mode(entry, st.st_mode);
	    archive_entry_set_size(entry, st.st_size);
	    archive_entry_set_mtime(entry, st.st_mtim.tv_sec,
				    st.st_mtim.tv_nsec);
	    archive_write_header(archive, entry);
	    fd = ::open(filename, O_RDONLY);
	    while ((len=read(fd, data->buf, sizeof(data->buf))) > 0) {
		archive_write_data(archive, data->buf, len);
	    }
	    close(fd);
	}

	json_decref(list);
    }

    static int a_creat(struct archive *a, void *data) {
	Data *d;

	d = (Data *) data;
	d->fd = ::open(d->name, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	return (d->fd >= 0) ? ARCHIVE_OK : ARCHIVE_FATAL;
    }

    static int a_open(struct archive *a, void *data) {
	Data *d;

	d = (Data *) data;
	d->fd = ::open(d->name, O_RDONLY, 0);
	return (d->fd >= 0) ? ARCHIVE_OK : ARCHIVE_FATAL;
    }

    static la_ssize_t a_read(struct archive *a, void *data, const void **buf) {
	Data *d;

	d = (Data *) data;
	*buf = d->buf;
	return read(d->fd, d->buf, sizeof(d->buf));
    }

    static la_ssize_t a_write(struct archive *a, void *data, const void *buf,
			      size_t size) {
	Data *d;

	d = (Data *) data;
	return write(d->fd, buf, size);
    }

    static int a_close(struct archive *a, void *data) {
	Data *d;

	d = (Data *) data;
	if (d->fd >= 0) {
	    close(d->fd);
	}
	return 0;
    }

    struct archive *archive;	// libarchive reference
    struct archive_entry *entry;// archive entry
    Data *data;
    bool creating;		// new archive?
    json_t *list;		// list of items
    size_t index;		// item index
    std::string path;		// path of archive
    std::string origin;		// original collection
};
