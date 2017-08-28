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
from datetime import datetime
from subprocess import call

ROOTACT="/tmp/mdm/pktfwd/firmware"

try:
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
        p.sendline('copy running-config flash:backup_config\n')
        p.expect('#')

        #call(["touch usr/etc/lrr/saverff_done"])
        micro = datetime.utcnow().strftime("%f")[:-5]
        datestring = datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%S."+micro+"+00:00")

        f = open('usr/etc/lrr/saverff_done', 'w+')
        version = open('lrr/Version', 'r+')
        version_line = version.readline()

        f.write("RFFVERSION="+version_line+"\n")
        #call(["cat lrr/Version > usr/etc/lrr/saverff_done"])

        f.write("RFFDATE="+datestring+"\n")

        version.close()
        f.close()
        exit(0)

except Exception, e:
        print("Backup failed, exception: "+str(e))
        exit(1)
