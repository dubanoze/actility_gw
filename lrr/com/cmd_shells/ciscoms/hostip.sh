#!/usr/bin/python
import re
import pexpect
import sys
import sched, time

host_ip_tmp="0"
ROOTACT="/tmp/mdm/pktfwd/firmware"

while True:
        fname=ROOTACT+"/usr/etc/lrr/credentials.txt"
        with open(fname) as f:
                content = f.readlines()
        content = [x.strip() for x in content]
        if len(content) < 3:
                print("Problem with credentials in file!")
                exit(1)
        adminsecpassword=content[0]
        username=content[1]
        password=content[2]

        cmd ='ssh -o StrictHostKeyChecking=no '+username+'@10.0.3.1'
        p=pexpect.spawn(cmd)
        p.expect('password:')
        p.sendline(password)
        p.sendline("\n")
        p.sendline("\n")
        p.sendline("\n")
        p.expect('>')
        p.sendline("enable\n")
        p.expect('Password:')
        p.sendline(adminsecpassword)
        p.expect('#')
        p.sendline("show running-config")
        p.expect('#')
        run_config=p.before
        vlan_ip = re.search(r'.*Vlan (\d{1,4}).*', run_config)
        try:
                # try to get number of vlan
                vlan = vlan_ip.group(1)
                # vlan number is present, parse host's ip from vlan configuration
                p.sendline("show ip interface Vlan "+str(vlan))
                p.expect('#')
                vlan_config=p.before
                host_ip = re.search(r'.*Internet address is (\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}).*', vlan_config)
                try:
                        ip = host_ip.group(1)
                except Exception, e:
                        ip = 0;
        except Exception, e:
                # number of vlan does not exists
                # now, try with FastEthernet 0/1

                p.sendline("show ip interface FastEthernet 0/1")
                p.expect('#')
                if_config=p.before
                host_ip = re.search(r'.*Internet address is (\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}).*', if_config)
                try:
                        ip = host_ip.group(1)
                except Exception, e:
                        ip = 0
        p.sendline("request shell exit")

        if ip != host_ip_tmp:
                with open('/var/run/hosts_ip_status', 'w+') as f:
                        f.write(str(ip)+"\n")
                        f.close()
                host_ip_tmp = ip
        time.sleep(60)
