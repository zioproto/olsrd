####################
# initialization   #
####################
trap olsrd_disconnect INT TERM EXIT

function olsrd_connect()
{
  empty -f $OLSRD_CONNECT_CMD
}

function olsrd_disconnect()
{
  empty -k
}

####################
# HNA              #
####################
function olsrd_hna_add()
{
  empty -s "hna add $1\n" 2>&1 > /dev/null || return 1
  empty -t $OLSRD_TIMEOUT -w "^added" "" "^FAILED:" "" 2>&1 > /dev/null
  if [ $? -ne 1 ]; then
    return 1
  fi
  return 0
}

function olsrd_hna_del()
{
  empty -s "hna del $1\n" 2>&1 > /dev/null || return 1
  empty -t $OLSRD_TIMEOUT -w "^removed" "" "^FAILED:" "" 2>&1 > /dev/null
  if [ $? -ne 1 ]; then
    return 1
  fi
  return 0
}

olsrd_connect
