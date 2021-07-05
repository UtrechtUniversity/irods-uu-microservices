# Microservice API

- __`msiCreateEpicPID(*uuid, *url, *handle, *http_code)`__

  `*uuid`: UUID to base the PID on

  `*url`: URL for the PID

  `*handle`: output, the PID handle (prefix + uuid)

  `*http_code`: output, the HTTP code for the request, normally 201

- __`msiGetEpicPID(*handle, *url, *http_code)`__

  `*handle`: the PID handle

  `*url`: output, the URL of the PID

  `*http_code`: output, the HTTP code for the request, normally 200

- __`msiDeleteEpicPID(*handle, *http_code)`__

  `*handle`: the PID handle to delete

  `*http_code`: output, the HTTP code for the request, normally 200

- __`msiUpdateEpicPID(*handle, *key, *value, *http_code)`__

  `*handle`: the PID handle

  `*key`: the metadata key, which can be the URL

  `*value`: the metadata value, always a string

  `*http_code`: output, the HTTP code for the request, normally 200
