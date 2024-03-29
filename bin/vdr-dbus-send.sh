#!/bin/bash

declare -i VDR_ID=${VDR_ID:-0}

if [ $# -lt 2 ]
then
  echo "usage: $0 objectpath interface.method [arguments]"
  exit 1
fi

DEST=de.tvdr.vdr
INTERFACE_DEST="$DEST"
(( VDR_ID > 0 )) && DEST+="${VDR_ID}"
OBJECT=$1
shift
INTERFACE=$1
shift

dbus-send --system --type=method_call --dest=$DEST --print-reply $OBJECT $INTERFACE_DEST.$INTERFACE "$@"
