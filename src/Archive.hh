#include <archive.h>
#include <archive_entry.h>
#include <jansson.h>

#include "rsDataObjCreate.hpp"
#include "rsDataObjOpen.hpp"
#include "rsDataObjRead.hpp"
#include "rsDataObjWrite.hpp"
#include "rsDataObjClose.hpp"
#include "rsCollCreate.hpp"
#include "rcMisc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define A_BUFSIZE       (1024 * 1024)
#define A_BLOCKSIZE     ((size_t) 8192)

/*
 * libarchive for iRODS
 */
class Archive {
    class Data {
    public:
        Data(rsComm_t *rsComm, const char *name) :
             rsComm(rsComm),
             name(name) {
            resource = NULL;
            index = 0;

            /*
             * initialize _creat() input
             */
            memset(&create, '\0', sizeof(dataObjInp_t));
            create.openFlags = O_CREAT | O_WRONLY | O_TRUNC;
            if (resource != NULL) {
                addKeyVal(&create.condInput, DEST_RESC_NAME_KW, resource);
            }
            addKeyVal(&create.condInput, FORCE_FLAG_KW, "");
            addKeyVal(&create.condInput, TRANSLATED_PATH_KW, "");

            /*
             * initialize _open() input
             */
            memset(&open, '\0', sizeof(dataObjInp_t));
            open.openFlags = O_RDONLY;
        }

        rsComm_t *rsComm;       /* iRODS context */
        const char *name;       /* name of file to open */
        const char *resource;   /* resource to create the file on */
        int index;              /* file index */
        dataObjInp_t create;    /* cached create input */
        dataObjInp_t open;      /* cached open input */
        char buf[A_BUFSIZE];    /* buffer for reading */
    };

    /*
     * archive constructor
     */
    Archive(struct archive *archive, Data *data, bool creating, json_t *list,
            size_t dataSize, std::string &path, std::string &collection,
            const char *resc, std::string indexString) :
        archive(archive),
        data(data),
        creating(creating),
        list(list),
        dataSize(dataSize),
        path(path),
        origin(collection),
        indexString(indexString)
    {
        data->resource = resc;
        index = 0;
    }

public:
    /*
     * create archive
     */
    static Archive *create(rsComm_t *rsComm, std::string path,
                           std::string collection, const char *resc) {
        struct archive *a;
        Data *data;

        /*
         * Create archive, determine format and compression mode based on
         * the name: archive.tar, archive.zip, archive.tar.gz
         */
        a = archive_write_new();
        if (a == NULL) {
            return NULL;
        }
        if (path.length() >= 4 && !path.compare(path.length() - 4, 4, ".zip")) {
            archive_write_set_format_zip(a);
        } else {
            archive_write_set_format_ustar(a);
        }
        data = new Data(rsComm, path.c_str());
        data->resource = resc;
        if (archive_write_open(a, data, &a_creat, &a_write, &a_close) !=
                                                                ARCHIVE_OK) {
            delete data;
            archive_write_free(a);
            return NULL;
        }

        /*
         * archive was created, call the constructor
         */
        return new Archive(a, data, true, json_array(), 0, path, collection,
                           resc, "");
    }

    /*
     * open existing archive
     */
    static Archive *open(rsComm_t *rsComm, std::string path, const char *resc) {
        struct archive *a;
        Data *data;
        struct archive_entry *entry;
        size_t size;
        char *buf;
        json_t *json, *list;
        json_error_t error;
        std::string origin;
        Archive *archive;

        /*
         * open any archive
         */
        a = archive_read_new();
        if (a == NULL) {
            return NULL;
        }
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);
        data = new Data(rsComm, path.c_str());
        if (archive_read_open(a, data, &a_open, &a_read, &a_close) !=
                                                                ARCHIVE_OK ||
            archive_read_next_header(a, &entry) != ARCHIVE_OK) {
            delete data;
            archive_read_free(a);
            return NULL;
        }

        /*
         * the archive must have INDEX.json as its first entry
         */
        if (strcmp(archive_entry_pathname(entry), "INDEX.json") != 0) {
            delete data;
            archive_read_free(a);
            return NULL;
        }

        /*
         * retrieve and load INDEX.json
         */
        size = (size_t) archive_entry_size(entry);
        buf = new char[size + 1];
        buf[size] = '\0';
        if (archive_read_data(a, buf, size) != (__LA_SSIZE_T) size ||
            (json=json_loads(buf, 0, &error)) == NULL) {
            delete buf;
            delete data;
            archive_read_free(a);
            return NULL;
        }

        /*
         * obtain list of items from INDEX.json
         */
        origin = json_string_value(json_object_get(json, "collection"));
        size = (size_t) json_integer_value(json_object_get(json, "size"));
        list = json_object_get(json, "items");
        json_incref(list);
        json_decref(json);

        /*
         * safe to call the constructor
         */
        archive = new Archive(a, data, false, list, size, path, origin, resc,
                              buf);
        delete buf;
        return archive;
    }

    /*
     * destruct archive, cleaning up if needed
     */
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

    /*
     * Add a DataObj to an archive.  It will be added to the index at first,
     * the actual archive will be created when construct() is called.
     */
    void addDataObj(std::string name, size_t size, time_t created,
                    time_t modified, std::string owner, std::string zone,
                    std::string checksum, json_t *attributes, json_t *acl) {
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
            if (acl != NULL) {
                json_object_set(json, "ACL", acl);
            }
            json_array_append_new(list, json);

            dataSize += (size + A_BLOCKSIZE - 1) & ~(A_BLOCKSIZE - 1);
        }
    }

    /*
     * Add a collection to an archive.  It will be added to the index at first,
     * the actual archive will be created when construct() is called.
     */
    void addColl(std::string name, time_t created, time_t modified,
                 std::string owner, std::string zone, json_t *attributes,
                 json_t *acl) {
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
        if (acl != NULL) {
            json_object_set(json, "ACL", acl);
        }
        json_array_append_new(list, json);
    }

    /*
     * construct an archive from the index, return status
     */
    int construct() {
        if (creating) {
            json_t *json;
            char *str;
            __LA_SSIZE_T len;

            /*
             * first entry, INDEX.json
             */
            json = json_object();
            json_object_set_new(json, "collection",
                                json_string(origin.c_str()));
            json_object_set_new(json, "size",
                                json_integer((json_int_t) dataSize));
            json_object_set(json, "items", list);
            str = json_dumps(json, JSON_INDENT(2));
            json_decref(json);

            len = (__LA_SSIZE_T) strlen(str);
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

            /*
             * now add the DataObjs and collections
             */
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
                    /*
                     * collection
                     */
                    archive_entry_set_filetype(entry, AE_IFDIR);
                    archive_entry_set_perm(entry, 0750);
                    if (archive_write_header(archive, entry) < 0) {
                        return SYS_TAR_APPEND_ERR;
                    }
                } else {
                    /*
                     * DataObj
                     */
                    archive_entry_set_filetype(entry, AE_IFREG);
                    archive_entry_set_perm(entry, 0600);
                    size = (size_t) json_integer_value(json_object_get(json,
                                                                       "size"));
                    archive_entry_set_size(entry, (__LA_INT64_T) size);
                    if (archive_write_header(archive, entry) < 0) {
                        return SYS_TAR_APPEND_ERR;
                    }
                    fd = _open(data, (origin + "/" + filename).c_str());
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

    /*
     * return INDEX.json as a string
     */
    std::string indexItems() {
        return indexString;
    }

    /*
     * return size in blocks of items once extracted
     */
    size_t size() {
        return dataSize;
    }

    /*
     * get metadata of next item (potentially skipping current) from archive
     */
    json_t *nextItem() {
        if (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
            return json_array_get(list, index++);
        } else {
            return NULL;
        }
    }

    /*
     * extract current item under the given filename
     */
    int extractItem(std::string filename) {
        if (archive_entry_filetype(entry) == AE_IFDIR) {
            collInp_t collCreateInp;

            /*
             * collection
             */
            memset(&collCreateInp, '\0', sizeof(collInp_t));
            rstrcpy(collCreateInp.collName, filename.c_str(), MAX_NAME_LEN);
            return rsCollCreate(data->rsComm, &collCreateInp);
        } else {
            char buf[A_BUFSIZE];
            int fd, status;
            __LA_SSIZE_T len;

            /*
             * DataObj
             */
            fd = _creat(data, filename.c_str());
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

private:
    /*
     * create an iRODS DataObj
     */
    static int _creat(Data *data, const char *name) {
        rstrcpy(data->create.objPath, name, MAX_NAME_LEN);
        return rsDataObjCreate(data->rsComm, &data->create);
    }

    /*
     * open an iRODS DataObj
     */
    static int _open(Data *data, const char *name) {
        char tmp[MAX_NAME_LEN];

        rstrcpy(data->open.objPath, name, MAX_NAME_LEN);
        rstrcpy(tmp, TRANSLATED_PATH_KW, MAX_NAME_LEN);
        rmKeyVal(&data->open.condInput, tmp);
        return rsDataObjOpen(data->rsComm, &data->open);
    }

    /*
     * read an iRODS DataObj
     */
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

    /*
     * write to an iRODS DataObj
     */
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

    /*
     * close an iRODS DatObj
     */
    static int _close(rsComm_t *rsComm, int index) {
        openedDataObjInp_t input;

        memset(&input, '\0', sizeof(openedDataObjInp_t));
        input.l1descInx = index;
        return rsDataObjClose(rsComm, &input);
    }

    /*
     * libarchive wrapper for _creat()
     */
    static int a_creat(struct archive *a, void *data) {
        Data *d;

        d = (Data *) data;
        d->index = _creat(d, d->name);
        return (d->index >= 0) ? ARCHIVE_OK : ARCHIVE_FATAL;
    }

    /*
     * libarchive wrapper for _open()
     */
    static int a_open(struct archive *a, void *data) {
        Data *d;

        d = (Data *) data;
        d->index = _open(d, d->name);
        return (d->index >= 0) ? ARCHIVE_OK : ARCHIVE_FATAL;
    }

    /*
     * libarchive wrapper for _read()
     */
    static __LA_SSIZE_T a_read(struct archive *a, void *data, const void **buf) {
        Data *d;
        __LA_SSIZE_T status;

        d = (Data *) data;
        if (d->index < 0 ||
            (status=_read(d->rsComm, d->index, d->buf, sizeof(d->buf))) < 0) {
            return -1;
        } else {
            *buf = d->buf;
            return status;
        }
    }

    /*
     * libarchive wrapper for _write()
     */
    static __LA_SSIZE_T a_write(struct archive *a, void *data, const void *buf,
                                size_t size) {
        Data *d;
        __LA_SSIZE_T status;

        d = (Data *) data;
        if (d->index < 0 ||
            (status=_write(d->rsComm, d->index, buf, size)) < 0) {
            return -1;
        } else {
            return status;
        }
    }

    /*
     * libarchive wrapper for _close()
     */
    static int a_close(struct archive *a, void *data) {
        Data *d;

        d = (Data *) data;
        return (d->index < 0 || _close(d->rsComm, d->index) < 0) ? -1 : 0;
    }

    struct archive *archive;            /* libarchive reference */
    struct archive_entry *entry;        /* archive entry */
    Data *data;                         /* context data */
    bool creating;                      /* new archive? */
    json_t *list;                       /* list of items */
    size_t index;                       /* index of current item */
    size_t dataSize;                    /* total size of archived DataObjs */
    std::string path;                   /* path of archive */
    std::string origin;                 /* original collection */
    std::string indexString;            /* index as a string */
};
