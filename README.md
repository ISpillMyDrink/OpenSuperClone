<h1 align="center">
  <br>
  OpenSuperClone
  <br>
</h1>

<div align="center">

  [![Commits](https://badgen.net/github/commits/ispillmydrink/opensuperclone/main)](https://github.com/ISpillMyDrink/OpenSuperClone/commits/main)
  [![Last Commit](https://badgen.net/github/last-commit/ispillmydrink/opensuperclone/main)](https://github.com/ISpillMyDrink/OpenSuperClone/commits/main)
  [![Release](https://badgen.net/github/release/ispillmydrink/opensuperclone)](https://github.com/ISpillMyDrink/OpenSuperClone/releases)
  [![License](https://badgen.net/github/license/ispillmydrink/opensuperclone)](https://github.com/ISpillMyDrink/OpenSuperClone/blob/main/LICENSE)
  
</div>

<h4 align="center">
  A powerful disk cloning utility for Linux with many advanced features based on <a href="https://www.hddsuperclone.com/">Scott Dwyer's HDDSuperClone</a>.
</h4>

<p align="center">
  <a href="#-features">✨ Features</a> •
  <a href="#-installation">💾 Installation</a> •
  <a href="#%EF%B8%8F-changelog">✏️ Changelog</a> •
  <a href="#-credits">💵 Credits</a> •
  <a href="#-license">📄 License</a>
</p>

<p align="center">
  <img src="./doc/opensuperclone.gif" alt="Size Limit CLI" width="738">
</p>

## ✨ Features

* Advanced drive cloning capability akin to that of ddrescue
* Virtual drive mode for targeted data extraction
* Direct AHCI and IDE capability to circumvent OS drivers completely
* USB relay control for automatic power cycling of drives

#### 📋 Planned Changes

* Refactoring of codebase (get rid of -fcommon flag)
* Remove requirement for running as root
* Add configuration files and presets for settings

## 💾 Installation

Pre-built DEB and RPM packages for OpenSuperClone can be found on the <a href=https://github.com/ISpillMyDrink/OpenSuperClone/releases>Release</a> page.

Note that OpenSuperClone requires a kernel module to be installed for the virtual drive functionality and direct AHCI/IDE access. If `DKMS` is present,
the module will be automatically built and installed. Otherwise, the module can be built and installed manually.

```Bash

# Build and install the kernel module manually (substitute x.x.x with the version number)
$ cd /usr/src/OSCDriver-x.x.x
$ make
$ sudo make install

# Alternatively, build and install the kernel module using DKMS
# after the package has been installed (requires DKMS to be installed)
$ sudo dkms install OSCDriver/x.x.x

```

### Compiling from Source

💡 OpenSuperClone requires the following packages to be installed for a successful project build:

- `cmake`
- `gcc`
- `gettext`
- `libconfig-dev`
- `libgtk-3-dev`
- `libusb-dev`
- `pkg-config`

#### Building and running locally

```Bash
# Build OpenSuperClone and OSCViewer for Release and install to ./Release
$ ./build.sh

# If you want to install the kernel module with DKMS, copy the source to the DKMS directory
# and install the module (substitute x.x.x with the version number)
sudo cp -r ./Release/src/OSCDriver-x.x.x /usr/src/OSCDriver-x.x.x
sudo dkms install OSCDriver/x.x.x

# Run OpenSuperClone
$ sudo ./Release/bin/opensuperclone

# Run OSCViewer
$ ./Release/bin/oscviewer
```

#### Building and installing to /usr/

```Bash
# Build OpenSuperClone and OSCViewer for Release and install to /usr/
# This will also attempt to install the kernel module if DKMS is installed
$ ./install.sh

# Run OpenSuperClone
$ sudo opensuperclone

# Run OSCViewer
$ oscviewer
```

## ✏️ Changelog

#### OpenSuperClone 2.5 [Work-In-Progress]

* Implemented DKMS for driver installation
* OSCDriver 2.6.2: add support for kernel 5.19.12 to 6.1.1 (thanks to <a href="https://github.com/ISpillMyDrink/OpenSuperClone/commit/f59b24ae116fa284ab4f6c8d6b288f01370f88dc">piernov</a>)

#### OpenSuperClone 2.4.1

* Fixed a bug where the UI labels would show bogus strings

#### OpenSuperClone 2.4

* Removal of licensing related code
* Implemented CMake as build system
* Switched from GTK2 to GTK3
* Replaced localization system with gettext
* OSCDriver 2.6.1: add suport for kernel 5.15 to 5.19.11 (thanks to <a href="https://github.com/piernov/">piernov</a>)
* Minor improvements to OSCViewer (color picker, minor graphical changes)

## 💵 Credits

All credit belongs to Scott Dwyer with his work on <a href="https://www.hddsuperclone.com/">HDDSuperClone</a>.

This project makes use of the following libraries/programs:
* GLADE (https://glade.gnome.org/)
* GTK (https://www.gtk.org/)
* libconfig (https://hyperrealm.github.io/libconfig/)
* libintl (https://www.gnu.org/software/gettext/)
* libusb (https://libusb.info/)

## 📄 License

This software is licensed under the [GPLv2](LICENSE).
