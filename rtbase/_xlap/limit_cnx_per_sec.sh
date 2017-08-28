
iptables -A INPUT -p tcp --syn --dport 1234 -m limit --limit 100/s --limit-burst 100 -j ACCEPT 
iptables -A INPUT -p tcp --syn --dport 1234 -j DROP

