# Microservice API
## PID microservices

- __`msiCreateEpicPID(*uuid, *url, *handle, *http_code)`__

  `*uuid`: UUID to base the PID on

  `*url`: URL for the PID

  `*handle`: output, the PID handle (prefix + uuid)

  `*http_code`: output, the HTTP code for the request, normally 201

- __`msiGetEpicPID(*handle, *url, *http_code)`__

  `*handle`: the PID handle

  `*url`: output, the URL of the PID

  `*http_code`: output, the HTTP code for the request, normally 200

- __`msiGetEpicPIDMetadtaa(*handle, *metaName, *metaValue, *http_code)`__

  `*handle`: the PID handle

  `*metaName`: Metadata key in the handle system

  `*metaValue`: output, Metadata value from the handle system

  `*http_code`: output, the HTTP code for the request, normally 200

- __`msiDeleteEpicPID(*handle, *http_code)`__

  `*handle`: the PID handle to delete

  `*http_code`: output, the HTTP code for the request, normally 200

- __`msiUpdateEpicPID(*handle, *key, *value, *http_code)`__

  `*handle`: the PID handle

  `*key`: the metadata key, which can be the URL

  `*value`: the metadata value, always a string

  `*http_code`: output, the HTTP code for the request, normally 200

## Archival package microservices
- __`msiArchiveCreate(*archive_name, *collection, *resource, *out)`__

  `*archive_name`: name of the archive, suffix determines type of archive (tar, zip, bzip, ...)

  `*collection`: iRODS collection to be archived

  `*resource`: iRODS resource the archive should be placed on

  `*out`: status

- __`msiArchiveExtract(*archive_name, *destination_collection, *extract, *resource, *out)`__

  `*archive_name`: name of the archive to be extracted

  `*destination_collection`: iRODS collection where data shall be extracted to

  `*extract`: Partial path as stored in the index of the archive if only one element should be extracted, null otherwise

  `*resource`: iRODS resource the data should be placed on

  `*out`: status

- __`msiArchiveIndex(*archive_name, *out)`__

  `*archive_name`: name of the archive

  `*out`: status
