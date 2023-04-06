#!/bin/sh

if [ -z "$SUDO_USER" ]; then
    # if the user didn't call us with sudo, re-execute
    exec sudo $0 "$@"
fi

### update sources and get add-apt-repository
apt-get update
apt-get -y install software-properties-common

### add the extended source repos
add-apt-repository multiverse
add-apt-repository universe
add-apt-repository restricted

### make sure we're totally up-to-date now
apt-get update
apt-get -y dist-upgrade

### install the software we need for the VM and build env
apt-get -y install build-essential gcc g++ cmake libpcap-dev htop jnettop screen   \
                   emacs-nox vim-nox automake pkg-config libtool libtool-bin git tig links     \
                   parallel iptables mahimahi mininet net-tools tcpdump wireshark telnet socat \
                   clang clang-format clang-tidy clang-tools coreutils bash doxygen graphviz   \
                   virtualbox-guest-utils netcat-openbsd
