#!/bin/sh

echo "Uninstalling OpenSuperClone from /usr/local/..."

if [ -f /usr/local/bin/opensuperclone ]; then
    sudo rm -f /usr/local/bin/opensuperclone
fi

if [ -f /usr/local/bin/oscviewer ]; then
    sudo rm -f /usr/local/bin/oscviewer
fi

if [ -d /usr/local/bin/oscscripts ]; then
    sudo rm -rf /usr/local/bin/oscscripts
fi

if [ -f /usr/local/share/applications/opensuperclone.desktop ]; then
    sudo rm -f /usr/local/share/applications/opensuperclone.desktop
fi

if [ -f /usr/local/share/applications/opensupertool.desktop ]; then
    sudo rm -f /usr/local/share/applications/opensupertool.desktop
fi

if [ -f /usr/local/share/applications/oscviewer.desktop ]; then
    sudo rm -f /usr/local/share/applications/oscviewer.desktop
fi

if [ -d /usr/local/share/locale ]; then
    for locale in $(ls /usr/local/share/locale); do
        if [ -f /usr/local/share/locale/$locale/LC_MESSAGES/opensuperclone.mo ]; then
            sudo rm -f /usr/local/share/locale/$locale/LC_MESSAGES/opensuperclone.mo
        fi
        if [ -f /usr/local/share/locale/$locale/LC_MESSAGES/oscviewer.mo ]; then
            sudo rm -f /usr/local/share/locale/$locale/LC_MESSAGES/oscviewer.mo
        fi
    done
fi

echo "Done."