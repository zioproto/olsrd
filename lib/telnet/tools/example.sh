#!/bin/bash

HOST="localhost"
PORT=2023

## this has to contain a valid command to connect to olsrd telnet
OLSRD_CONNECT_CMD="nc $HOST $PORT"
#OLSRD_CONNECT_CMD="telnet $HOST $PORT"

# wait OLSRD_TIMEOUT seconds for command response
OLSRD_TIMEOUT=1

# this connects to olsrd and creates the communication buffers
# the connection stays online until the calling script (example.sh)
# terminiates
. ./hna.sh

echo -n "adding a HNA entry .. "
olsrd_hna_add 1.2.3.4/24
if [ $? -ne 0 ]; then
  echo "failed."
else
  echo "ok."
fi 

echo -n "removing a HNA entry .. "
olsrd_hna_del 5.6.7.8/24
if [ $? -ne 0 ]; then
  echo "failed."
else
  echo "ok."
fi 
