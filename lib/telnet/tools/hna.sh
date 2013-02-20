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
  empty -s "hna add $1\n" || return 1
  empty -t 1 -w "^added" "" "^FAILED:" "" 
  if [ $? -ne 1 ]; then
    return 1
  fi
  return 0
}

function olsrd_hna_del()
{
  empty -s "hna del $1\n" || return 1
  empty -t 1 -w "^removed" "" "^FAILED:" "" 
  if [ $? -ne 1 ]; then
    return 1
  fi
  return 0
}

olsrd_connect
