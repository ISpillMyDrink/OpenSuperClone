# install stuff needed for making and building
sudo apt-get install gcc -y
sudo apt-get install g++ -y
sudo apt-get install make -y
sudo apt-get install libusb-dev -y
sudo apt-get install libcurl4-gnutls-dev -y
sudo apt-get install libreoffice-base -y
sudo apt-get install libreoffice-common -y
sudo apt-get install libssl-dev -y
sudo apt-get install p7zip-full -y
sudo apt-get install dh-make -y
sudo apt-get install devscripts -y
sudo apt-get install help2man -y
sudo apt-get install texinfo -y
sudo apt-get install ruby-dev build-essential -y
sudo gem install fpm
sudo apt-get install rpm -y
sudo apt-get install gtk+-2.0 -y
sudo apt-get install gtk+2.0 -y
sudo apt install libgtk-3-dev -y
sudo apt-get install squashfs-tools -y
sudo apt-get install genisoimage -y


# install things for glade
sudo apt-get install intltool -y
sudo apt-get install libxml2-dev -y
sudo apt-get install libgladeui-1-dev -y
sudo apt-get install libgladeui-dev -y
sudo apt-get install -y itstool -y
sudo apt-get install libpango1.0-0 -y

wget http://archive.ubuntu.com/ubuntu/pool/universe/g/glade-3/libgladeui-1-11_3.8.0-0ubuntu6_amd64.deb
sudo dpkg -i libgladeui-1-11_3.8.0-0ubuntu6_amd64.deb
rm libgladeui-1-11_3.8.0-0ubuntu6_amd64.deb

wget https://download.gnome.org/sources/glade3/3.8/glade3-3.8.5.tar.xz
tar xf glade3-3.8.5.tar.xz
cd glade3-3.8.5
./configure
make
sudo make install
cd ..
rm glade3-3.8.5.tar.xz
sudo rm -r glade3-3.8.5

wget https://download.gnome.org/sources/glade3/3.8/glade3-3.8.6.tar.xz
tar xf glade3-3.8.6.tar.xz
cd glade3-3.8.6
./configure
make
sudo make install
cd ..
rm glade3-3.8.6.tar.xz
sudo rm -r glade3-3.8.6

# now install regular up to date glade
sudo apt install glade -y