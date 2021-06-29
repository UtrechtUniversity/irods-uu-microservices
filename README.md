# iRODS UU Microservices
Miscellaneous iRODS microservices developed or modified by Utrecht University.

## Included microservices
Developed at Utrecht University:
  * msiRegisterEpicPID: API call to register an Epic PID

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
