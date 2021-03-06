#!/bin/sh
#
#       this file has been changed when installing LRR software
#

export iptables=/usr/sbin/iptables
ROOTACT=/tmp/mdm/pktfwd/firmware
export ROOTACT
set_rules()
{
  if [ ! -z "$ROOTACT" -a -d "$ROOTACT" ]
  then
        LOG="$ROOTACT/var/log/lrr/iptablesfrom.log"
        [ ! -d "$ROOTACT/var/log/lrr" ] && mkdir -p "$ROOTACT/var/log/lrr"
        if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
        then
                . ${ROOTACT}/lrr/com/_parameters.sh
        fi
        if [ -f $ROOTACT/usr/etc/lrr/iptables.sh ]
        then
                echo $ROOTACT/usr/etc/lrr/iptables.sh > $LOG
                sh $ROOTACT/usr/etc/lrr/iptables.sh
                return
        fi
        if [ -f $ROOTACT/lrr/com/shells/${SYSTEM}/iptables.sh ]
        then
                echo $ROOTACT/lrr/com/shells/${SYSTEM}/iptables.sh > $LOG
                sh $ROOTACT/lrr/com/shells/${SYSTEM}/iptables.sh
                return
        fi
        if [ -f $ROOTACT/lrr/com/shells/iptables.sh ]
        then
                echo $ROOTACT/lrr/com/shells/iptables.sh > $LOG
                sh $ROOTACT/lrr/com/shells/iptables.sh
                return
        fi
        echo /etc/init.d/S57iptables > $LOG
  fi
  #DROP everything in INPUT (Let everything going out)
  iptables -P INPUT DROP
  ip6tables -P INPUT DROP
  #Allow everything on localhost interface
  iptables -A INPUT -i lo -j ACCEPT
  #Allow DHCP protocol on all interfaces
  #iptables -A INPUT -p udp --dport 67:68 --sport 67:68 -j ACCEPT
  #Allow ICMP output (ping requests) on all interfaces
  iptables -A INPUT -p icmp --source 192.168.0.0/16 -j ACCEPT
  iptables -A INPUT -p icmp --source 10.10.0.0/16 -j ACCEPT
  #allow DNS requests
  #iptables -A INPUT -p udp --sport 53 -j ACCEPT
  #iptables -A INPUT -p tcp --sport 53 -j ACCEPT
  #allow NTP
  #iptables -A INPUT -p udp --dport 123 --sport 123 -j ACCEPT
  iptables -A INPUT -p tcp --dport 22 --source 192.168.0.0/16 -j ACCEPT
  iptables -A INPUT -p tcp --dport 22 --source 10.10.0.0/16 -j ACCEPT
  iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

  iptables -I INPUT 1 -p tcp --dport 22 --source 10.0.0.0/16 -j ACCEPT
  iptables -I OUTPUT 1 -p tcp --dport 22 --source 10.0.0.0/16 -j ACCEPT
}
remove_rules()
{
  # Flush Rules
  iptables -F INPUT
  iptables -F OUTPUT
  # Change default Policy
  iptables -P INPUT ACCEPT
  iptables -P OUTPUT ACCEPT
}
# Main script
case "$1" in
        start|restart)
                remove_rules
                set_rules
                ;;
        stop)
                remove_rules
                ;;
        *)
                echo "Usage: $0 {start|stop|restart}"
                exit 1
                ;;
esac
exit 0

