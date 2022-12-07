#!/bin/sh

echo "Uninstalling OpenSuperClone..."

echo "Uninstalling binaries..."

if [ -f /usr/bin/opensuperclone ]; then
    sudo rm -f /usr/bin/opensuperclone
fi

if [ -f /usr/bin/oscviewer ]; then
    sudo rm -f /usr/bin/oscviewer
fi

echo "Done uninstalling binaries."

echo "Uninstalling scripts..."

if [ -d /usr/bin/oscscripts ]; then
    sudo rm -f -r /usr/bin/oscscripts
fi

echo "Done uninstalling scripts."

echo "Uninstalling locales..."

sudo find /usr/share/locale -name "opensuperclone.mo" -exec rm -f {} \;
sudo find /usr/share/locale -name "oscviewer.mo" -exec rm -f {} \;

echo "Done uninstalling locales."

echo "Uninstalling desktop files..."

if [ -f /usr/share/applications/opensuperclone.desktop ]; then
    sudo rm -f /usr/share/applications/opensuperclone.desktop
fi

if [ -f /usr/share/applications/opensupertool.desktop ]; then
    sudo rm -f /usr/share/applications/opensupertool.desktop
fi

if [ -f /usr/share/applications/oscviewer.desktop ]; then
    sudo rm -f /usr/share/applications/oscviewer.desktop
fi
