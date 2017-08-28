#!/bin/sh

  #DROP everything in INPUT (Let everything going out)
  #iptables -P INPUT DROP
  #ip6tables -P INPUT DROP

  #Allow everything on localhost interface
  #iptables -A INPUT -i lo -j ACCEPT

  #Allow DHCP protocol on all interfaces
  #iptables -A INPUT -p udp --dport 67:68 --sport 67:68 -j ACCEPT

  #Allow ICMP output (ping requests) on all interfaces
  #iptables -A INPUT -p icmp -j ACCEPT

  #allow DNS requests
  #iptables -A INPUT -p udp --sport 53 -j ACCEPT
  #iptables -A INPUT -p tcp --sport 53 -j ACCEPT

  #allow NTP
  #iptables -A INPUT -p udp --dport 123 --sport 123 -j ACCEPT

  #iptables -A INPUT -p tcp --dport 22 --source 192.168.0.0/16 -j ACCEPT
  #iptables -A INPUT -p tcp --dport 22 --source 10.10.0.0/16 -j ACCEPT

  #iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT
