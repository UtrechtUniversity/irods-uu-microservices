# iRODS UU Microservices
Miscellaneous iRODS microservices developed or modified by Utrecht University.

## Included microservices
Developed at Utrecht University:
  * msiGetDataCiteDOI: API call to retrieve record of a DOI from DataCite
  * msiRegisterDataCiteDOI: API call to register a DOI with metadata or to mint a DOI
  * msiStrToUpper: Returns an uppercase string
  * msiSetUpperCaseWhereQuery: Set the UPPERCASE flag on a irods query.
  * msiGenerateRandomID: Generate a Random ID to use as basis for a new DOI

Developed at Donders Institute:
  * msi\_json\_objops: get, add and set values in a json object
  * msi\_json\_arrayops: get, add and set values in a json array. modified to handle arrays of arrays

  The msi\_json\_arrayops and msi\_json\_objops microservices are derived from
  ork from the Donders Institute. The license in DONDERS-LICENSE applies

Developed at Maastricht University:
  * msi\_add\_avu: Microservice to add AVU
  * msi\_rmw\_avu: Microservice to remove AVU

## Installation
iRODS UU microservices can be installed using the packages provided on the
[releases page](https://github.com/UtrechtUniversity/irods-uu-microservices/releases).

You can also build the microservices yourself, see [Building from source](#building-from-source).

## Building from source
To build from source, the following build-time dependencies must be installed:

- `make`
- `gcc-c++`
- `irods-devel`
- `irods-externals-cmake3.5.2-0`
- `irods-externals-clang6.0-0`
- `boost-devel`
- `boost-locale`
- `openssl-devel`
- `libcurl-devel`
- `libuuid-devel`
- `jansson-devel`
- `rpmdevtools` (if you are creating an RPM)

```
sudo yum install make gcc-c++ irods-devel irods-externals-cmake3.5.2-0 irods-externals-clang6.0-0 boost-devel boost-locale openssl-devel libcurl-devel jansson-devel libuuid-devel rpmdevtools
```

Follow these instructions to build from source:

- First, browse to the directory where you have unpacked the source
  distribution.

- Check whether your umask is set to a sane value. If the output of
  `umask` is not `0022`, run `umask 0022` to fix it. This is important
  for avoiding conflicts in created packages later on.

- Compile the project
```bash
export PATH=/opt/irods-externals/cmake3.5.2-0/bin:$PATH
cmake .
make
```

Now you can either build an RPM or install the project without a package manager.

**To create a package:**
```bash
make package
```

That's it, you should now have an RPM in your build directory which you can install using yum.

**To install without creating a package**
```bash
make install
```

This will install the `.so` files into the microservice plugin directory.
