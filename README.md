# OpenSuperClone

This is a fork of the HDDSuperClone source code (<http://www.hddsuperclone.com/>) released under the GPL2 by Scott Dwyer.

## Changes (since version 2.3.3_20220430 of HDDSuperClone)

- Cleanup of project files
- Implemented CMake as build system
- Removal of licensing related functions
- Switched from GTK2 to GTK3
- Updated kernel driver to work on 5.15

## Known Issues

- OSCViewer is extremely buggy in GTK3 and missing functionality from its GTK2 counterpart, it is recommended to build it with GTK2 instead

## Compiling

```Bash
# Install required packages for building
./install-build-requirements.sh

# Build OpenSuperClone and OSCViewer
./build.sh
```

Compilation has been successfully tested on Ubuntu 22.04.1.

## Running

```Bash
# Install dependencies for running
./install-dependencies-deb.sh

# Run OpenSuperClone
sudo ./release/bin/opensuperclone

# Run OSCViewer
./release/bin/osscviewer
```
