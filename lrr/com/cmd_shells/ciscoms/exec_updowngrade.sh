#!/usr/bin/python
import pexpect
import sys
import os

ROOTACT="/tmp/mdm/pktfwd/firmware"

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
p.sendline("configure terminal")
p.expect('#')
p.sendline("container copy "+sys.argv[1]+" flash:")
p.expect('#')
p.sendline("exit")
p.expect('#')
p.sendline("request shell exit")

os.remove(sys.argv[1])
filename = sys.argv[1].split("/")[-1]

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
p.sendline("archive download-sw firmware /normal /save-reload flash:"+filename)
p.expect('#')
p.sendline("request shell exit")
