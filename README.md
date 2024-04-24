# iRODS UU Microservices
Miscellaneous iRODS microservices developed or modified by Utrecht University.

## Included microservices
Developed at Utrecht University:
  * msiRegisterEpicPID: Register an EPIC PID

Developed at Donders Institute:
  * msi\_json\_objops: get, add and set values in a json object
  * msi\_json\_arrayops: get, add and set values in a json array. modified to handle arrays of arrays

  The msi\_json\_arrayops and msi\_json\_objops microservices are derived from
  ork from the Donders Institute. The license in DONDERS-LICENSE applies

Developed at Maastricht University:
  * msi\_add\_avu: Microservice to add AVU
  * msi\_rmw\_avu: Microservice to remove AVU

Developed at Wageningen University & Research:
  * msiArchiveCreate: create an archive
  * msiArchiveExtract: extract from an archive
  * msiArchiveIndex: index an archive

## Installation
iRODS UU microservices can be installed using the packages provided on the
[releases page](https://github.com/UtrechtUniversity/irods-uu-microservices/releases).

You can also build the microservices yourself, see [Building from source](#building-from-source).

## Building from source
This repository includes a Vagrant configuration for building irods-uu-microservices from source on either CentOS 7 (for the RPM package) or Ubuntu 20.04 LTS (for the DEB package). It can be found in vagrant/build. In order to build a package using Vagrant, edit the .env file in the Vagrant build directory. Adjust the BOXNAME and IRODS_VERSION vars as needed. Then run vagrant up to provision the VM. The VM has all dependencies pre-installed, as well as a clone of the irods-uu-microservices repository. Log in on the VM using vagrant ssh and create the package (see below).

To build from source without using the Vagrant configuration, the following build-time dependencies must be installed (package names may differ on your platform):

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
- `libarchive-devel`
- `rpmdevtools` (if you are creating an RPM)

```
sudo yum install make gcc-c++ irods-devel irods-externals-cmake3.5.2-0 irods-externals-clang6.0-0 boost-devel boost-locale openssl-devel libcurl-devel jansson-devel libuuid-devel libarchive-devel rpmdevtools
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
