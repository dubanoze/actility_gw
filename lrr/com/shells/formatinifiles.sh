#!/bin/bash          
lrriniFile="$1"       
sed 's/^[ \t]*//' -i "$lrriniFile"
sed '/^\(\[\|;\)/! s/^/    /' -i "$lrriniFile"
