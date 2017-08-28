#!/usr/bin/python
###########################################################################
#Name of script : reboot_pending.sh
#usage : to reboot the gw from lxc container
#reads from credentials.txt:privileg-password, ssh-username , ssh-password
#usge example : ./reboot_pending.sh -> it is called from reboot.sh script
#warnings : May loose the connectivity as the gw is reloaded
###########################################################################
import re
import pexpect
import sys
import time

ROOTACT="/tmp/mdm/pktfwd/firmware"

try:
        sleep_time = sys.argv[1]
        sleep_time = float(sleep_time)
        time.sleep(15)

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
        p.sendline('copy running-config startup-config\n')
        p.expect('#')
        p.sendline('reload')
        p.expect('Proceed with reload')
        p.sendline('y')
        p.sendline('y')
        print("Reload started")

except Exception, e:
        print("Reload failed, exception: "+str(e))
