#!/bin/sh

echo "Installing OpenSuperClone..."

echo "Installing binaries..."
if [ ! -f /usr/bin/opensuperclone ]; then
    sudo cp ./bin/opensuperclone /usr/bin/opensuperclone
else
    echo "Opensuperclone is already installed. Exiting..."
    exit 1
fi

if [ ! -f /usr/bin/oscviewer ]; then
    sudo cp ./bin/oscviewer /usr/bin/oscviewer
else
    echo "OSCViewer is already installed. Exiting..."
    exit 1
fi

echo "Done installing binaries."

echo "Installing scripts..."
if [ ! -d /usr/bin/oscscripts ]; then
    sudo cp -r ./bin/oscscripts /usr/bin/oscscripts
else
    echo "OSCScripts are already installed. Exiting..."
    exit 1
fi

echo "Done installing scripts."

echo "Installing locales..."

if [ ! -d /usr/share/locale ]; then
    sudo mkdir /usr/share/locale
fi

sudo cp -r ./share/locale/* /usr/share/locale

echo "Done installing locales."

echo "Installiong desktop files..."

if [ ! -d /usr/share/applications ]; then
    sudo mkdir /usr/share/applications
fi

sudo cp ./share/applications/opensuperclone.desktop /usr/share/applications/opensuperclone.desktop
sudo cp ./share/applications/opensupertool.desktop /usr/share/applications/opensupertool.desktop
sudo cp ./share/applications/oscviewer.desktop /usr/share/applications/oscviewer.desktop

echo "Done installing desktop files."

echo "Done installing OpenSuperClone."
