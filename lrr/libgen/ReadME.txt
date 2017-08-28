
# Set arm toolchain
wget ftp://wiki-ftp-wirgrid:CmhKQtm1IqxOTgQ0RTE8@ftp.kerlinkm2mtechnologies.fr/_kerlink/arm-2011.03-wirma2-r59.tar.xz
mkdir /opt/toolchains
cd /opt/toolchains
tar xJf arm-2011.03-wirma2-r59.tar.xz
export PATH=$PATH:/opt/toolchains/arm-2011.03-wirma2/bin

# for 32bits compilation,
yum install glibc-devel.i686

# compilation
make clean && make all

# usage
usage: keygen create a hash for LRR configuration
-p                              set if running in production environment
-r                              generate a root password
-s                              generate a support password
-k                              generate a usb installation key
-c <string>                     key if -p is not set
-h                              print this page

