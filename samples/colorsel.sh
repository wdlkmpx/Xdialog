#!/bin/sh
# --colorsel is not compatible with (c)dialog [ncurses]

NO_NCURSES_COMPAT=1
. ./0common.sh || exit 1

COLORX=$(
$DIALOG --stdout \
        --title "Select color" \
        --colorsel "Choose a color..." 0 0
)

case $? in
	0)   echo "Selected color: $COLORX." ;;
	1)   echo "Cancel pressed." ;;
	255) echo "Box closed." ;;
esac
