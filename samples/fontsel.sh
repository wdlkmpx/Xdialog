#!/bin/sh
# --fontsel is not compatible with (c)dialog [ncurses]

NO_NCURSES_COMPAT=1
. ./0common.sh || exit 1

FONTX=$(
$DIALOG --stdout \
        --title "Select font" \
        --fontsel "Choose a font..." 0 0
)

case $? in
	0)   echo "Selected font: $FONTX." ;;
	1)   echo "Cancel pressed." ;;
	255) echo "Box closed." ;;
esac
