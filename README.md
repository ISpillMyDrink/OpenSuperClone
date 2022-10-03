# OpenSuperClone

This is a fork of the HDDSuperClone source code (http://www.hddsuperclone.com/) released under the GPL2 by Scott Dwyer.

Only changes so far include removal of unnecessary license check code and cleanup of the project files.
The standalone CLI program HDDSuperTool has been completely removed.


### Compiling

```Bash
# Install required packages for building
./install-build-requirements.sh

# Build OpenSuperClone and OSCViewer
./build.sh
```

Compilation has been successfully tested on Ubuntu 22.04.1.

### Running

```Bash
# Install dependencies for running
./install-dependencies-deb.sh

# Run OpenSuperClone
sudo ./bin/opensuperclone

# Run OSCViewer
./bin/osscviewer
```

### Installing

```Bash
# Install OpenSuperClone
./install.sh
```