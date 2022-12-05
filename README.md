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
* Virtual driver mode for targeted data extraction
* Direct AHCI and IDE capability to circumvent OS drivers completely
* USB Relay control for automatic drive power-cycling

#### 📋 Planned Changes

* Refactoring of codebase (get rid of -fcommon flag)
* Implement DKMS for driver (un-)installation
* Remove requirement for running as root
* Add configuration files and presets for settings

## 💾 Installation

Pre-built binaries aswell as Debian packages for OpenSuperClone can be found on the <a href=https://github.com/ISpillMyDrink/OpenSuperClone/releases>Release</a> page.

#### Compiling from Source

💡 OpenSuperClone requires the following packages to be installed for a successful project build:

- `cmake`
- `gcc`
- `gettext`
- `libgtk-3-dev`
- `libconfig-dev`
- `libusb-dev`
- `pkg-config`

```Bash
# Build OpenSuperClone and OSCViewer for Release and install to ./Release
$ ./build.sh

# Run OpenSuperClone
$ sudo ./Release/bin/opensuperclone

# Run OSCViewer
$ ./Release/bin/oscviewer
```

## ✏️ Changelog

#### OpenSuperClone 2.4 [Pre-Release]

* Removal of licensing related code
* Implemented CMake as build system
* Switched from GTK2 to GTK3
* Replaced localization system with gettext
* Updated kernel driver to work on 5.15
* Minor improvements to OSCViewer (color picker, minor graphical changes)

## 💵 Credits

All credit belongs to Scott Dwyer with his work on <a href="https://www.hddsuperclone.com/">HDDSuperClone</a>.

This project makes use of the following libraries/programs:
* GTK (https://www.gtk.org/)
* GLADE (https://glade.gnome.org/)
* libusb (https://libusb.info/)
* libintl (https://www.gnu.org/software/gettext/)
* libconfig (https://hyperrealm.github.io/libconfig/)

## 📄 License

This software is licensed under the [GPLv2](LICENSE).
