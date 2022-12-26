#!/bin/sh

echo "Uninstalling OpenSuperClone from /usr/..."

if [ -f /usr/bin/opensuperclone ]; then
    sudo rm -f /usr/bin/opensuperclone
fi

if [ -f /usr/bin/oscviewer ]; then
    sudo rm -f /usr/bin/oscviewer
fi

if [ -d /usr/bin/oscscripts ]; then
    sudo rm -rf /usr/bin/oscscripts
fi

if [ -f /usr/share/applications/opensuperclone.desktop ]; then
    sudo rm -f /usr/share/applications/opensuperclone.desktop
fi

if [ -f /usr/share/applications/opensupertool.desktop ]; then
    sudo rm -f /usr/share/applications/opensupertool.desktop
fi

if [ -f /usr/share/applications/oscviewer.desktop ]; then
    sudo rm -f /usr/share/applications/oscviewer.desktop
fi

if [ -d /usr/share/locale ]; then
    for locale in $(ls /usr/share/locale); do
        if [ -f /usr/share/locale/$locale/LC_MESSAGES/opensuperclone.mo ]; then
            sudo rm -f /usr/share/locale/$locale/LC_MESSAGES/opensuperclone.mo
        fi
        if [ -f /usr/share/locale/$locale/LC_MESSAGES/oscviewer.mo ]; then
            sudo rm -f /usr/share/locale/$locale/LC_MESSAGES/oscviewer.mo
        fi
    done
fi

echo "Done uninstalling OpenSuperClone from /usr/..."

echo "Uninstalling OSCDriver..."

sudo rmmod oscdriver

VERSION=$(grep -oP '(?<=set\(OSC_DRIVER_VERSION ).*(?=\))' CMakeLists.txt)
VERSION=$(echo $VERSION | cut -d' ' -f1)
echo "Assuming OSCDriver version $VERSION."

sudo dkms remove -m oscdriver/$VERSION --all

if [ -d /usr/src/oscdriver-$VERSION ]; then
    sudo rm -rf /usr/src/oscdriver-$VERSION
fi

echo "Done uninstalling OSCDriver."

echo "Done."
