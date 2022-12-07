#!/bin/sh

echo "Installing dependencies for OpenSuperClone..."

sudo yum install libgtk-3 -y
sudo yum install libusb-0.1 -y
sudo yum install make -y
sudo yum install gcc -y

echo "Done installing dependencies for OpenSuperClone."
