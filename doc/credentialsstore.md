# Provisioning the iRODS microservices with credentials

The iRODS microservices must be provisioned with the credentials used to communicate with the Epic PID server.  These credentials are stored in `/var/lib/irods/.credentials_store`.  To first create the credentials store, issue the following commands as the user `irods`:

```bash
mkdir /var/lib/irods/.credentials_store
chmod 700 /var/lib/irods/.credentials_store
```

Then create the file `/var/lib/irods/.credentials_store/store_config.json`:

```json
{
    "epic_url": "EPIC_SERVER_URL",
    "epic_handle_prefix": "HANDLE_PREFIX",
    "epic_key" : "/var/lib/irods/.credentials_store/epic_key.pem",
    "epic_certificate" : "/var/lib/irods/.credentials_store/epic_cert.pem"
}
```

Fill in `epic_url` and `epic_handle_prefix` with the Epic PID server URL and the handle prefix, respectively.

Then copy the `.pem` files of the communication certificate and the communication key to `/var/lib/irods/.credentials_store/epic_key.pem` and `/var/lib/irods/.credentials_store/epic_cert.pem`, respectively.
