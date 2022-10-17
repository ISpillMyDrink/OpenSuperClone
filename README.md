# OpenSuperClone

This is a fork of the HDDSuperClone source code (<http://www.hddsuperclone.com/>) released under the GPL2 by Scott Dwyer.

OpenSuperClone/HDDSuperClone is a powerful disk cloning utility for Linux akin to ddrescue with many advanced features.

## Changes (since version 2.3.3_20220430 of HDDSuperClone)

- Cleanup of project files
- Implemented CMake as build system
- Removal of licensing related functions
- Switched from GTK2 to GTK3
- Updated kernel driver to work on 5.15

## Planned Changes

- Refactoring of codebase (get rid of -fcommon flag)
- Implement DKMS for driver (un-)installation
- Remove requirement for running as root
- Get completely rid of GTK2 code

## Known Issues

- OSCViewer is extremely buggy in GTK3 and missing functionality from its GTK2 counterpart, it is recommended to build it with GTK2 for the time being

## Compiling and Running Locally

```Bash
# Install required packages for building
./install-build-requirements.sh

# Build OpenSuperClone and OSCViewer for Release and install to ./release
./build.sh

# Install dependencies for running
./install-dependencies-deb.sh

# Run OpenSuperClone
sudo ./release/bin/opensuperclone

# Run OSCViewer
./release/bin/osscviewer
```

## Installing Globally

Installation will build OpenSuperClone for Release and install binaries, scripts,
and translation files to /usr/local. Installation of the build requirements is necessary.

```Bash
# Build OpenSuperClone and OSCViewer for Release and install to /usr/local
# (installation prefix can be changed by editing install.sh)
./install.sh

# Run OpenSuperClone
sudo opensuperclone

# Run OSCViewer
oscviewer
```