# iRODS Microservices
Miscellaneous iRODS microservices originally developed by Utrecht University.

## Included microservices
  * msiCreateEpicPID: API call to register an Epic PID
  * msiDeleteEpicPID: API call to delete an Epic PID
  * msiGetEpicPID: API call to get an Epic PID
  * msiUpdateEpicPID: API call to update metadata for an Epic PID

## Installation
iRODS microservices can be installed using the packages provided on the
[releases page](https://git.wur.nl/rdm-infrastructure/irods-microservices/-/releases)

You can also build the microservices yourself, see [Building from source](#building-from-source).

## Building from source
To build from source, the following build-time dependencies must be installed:

- `make`
- `irods-dev`
- `irods-externals-cmake3.11.4`
- `irods-externals-clang6.0`
- `libboost-dev`
- `libcurl-openssl-dev`
- `libjansson-dev`
- `g++`

```
apt-get install g++ irods-dev irods-externals-cmake3.11.4 irods-externals-clang6.0 make libcurl4-openssl-dev libboost-dev libjansson-dev
```

Follow these instructions to build from source:

- First, browse to the directory where you have unpacked the source
  distribution.

- Check whether your umask is set to a sane value. If the output of
  `umask` is not `0022`, run `umask 0022` to fix it. This is important
  for avoiding conflicts in created packages later on.

- Access rights for iRODS group
  ```
  sudo chgrp irods /usr/lib/irods/plugins/microservices
  sudo chmod g+w /usr/lib/irods/plugins/microservices
  ```

- Add your admin account ot the iRODS group, e.g.:
  ```
  sudo vigr
  sudo vigr -s
  ```
  Logout and login, check with `id` if you are part of the iRODS group.
  
- Compile the project
```bash
export PATH=/opt/irods-externals/cmake3.11.4-0/bin:$PATH
cmake .
make
```

Now you can either build a DEB or install the project without a package manager.

**To create a package:**
```bash
make package
```

That's it, you should now have a DEB in your build directory which you can install using apt.

**To install without creating a package**
```bash
make install
```

This will install the `.so` files into the microservice plugin directory.
