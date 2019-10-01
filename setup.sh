# Android Firmware Developer | Zenlty <zenlty@outlook.com>
sudo apt-get update
sudo apt-get upgrade
cd /usr/local/src
sudo wget http://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz 
sudo tar xf autoconf*
cd autoconf-2.69
sudo sh configure --prefix /usr/local
sudo make install
cd ..
sudo wget http://ftp.gnu.org/gnu/automake/automake-1.15.tar.gz
sudo tar xf automake*
sudo cd automake-1.15
sh configure --prefix /usr/local
sudo make install
cd ..
sudo wget http://mirror.jre655.com/GNU/libtool/libtool-2.4.6.tar.gz
sudo tar xf libtool*
cd libtool-2.4.6
sudo sh configure --prefix /usr/local
sudo make install 
# Alternative sudo apt-get install autoconf
# Alternative  sudo apt-get install autotools-dev
