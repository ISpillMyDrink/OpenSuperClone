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

if ! command -v dkms >/dev/null; then
    echo "WARNING: DKMS is not installed, skipping driver removal."
    echo "If the driver was installed manually, it will not be removed."
    exit 0
fi

echo "Uninstalling OSCDriver..."

if lsmod | grep -q oscdriver; then
    sudo rmmod oscdriver
fi

VERSION=$(grep -oP '(?<=set\(OSC_DRIVER_VERSION ).*(?=\))' CMakeLists.txt)
VERSION=$(echo $VERSION | cut -d' ' -f1)
echo "Assuming OSCDriver version $VERSION."

sudo dkms remove -m oscdriver/$VERSION --all

if [ -d /usr/src/oscdriver-$VERSION ]; then
    sudo rm -rf /usr/src/oscdriver-$VERSION
fi

echo "Done uninstalling OSCDriver."

echo "Uninstalling MiniNVMe..."

if lsmod | grep -q mininvme; then
    sudo rmmod mininvme
fi

VERSION=$(grep -oP '(?<=set\(MIMINVME_VERSION ).*(?=\))' CMakeLists.txt)
VERSION=$(echo $VERSION | cut -d' ' -f1)
echo "Assuming MiniNVMe version $VERSION."

sudo dkms remove -m mininvme/$VERSION --all

if [ -d /usr/src/mininvme-$VERSION ]; then
    sudo rm -rf /usr/src/mininvme-$VERSION
fi

echo "Done uninstalling MiniNVMe."

echo "Done."
