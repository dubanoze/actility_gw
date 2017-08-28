#!/bin/sh

kmstool position | grep -v "SEND COMMAND" | grep -v POSITION | grep -v "Application"
exit 0
