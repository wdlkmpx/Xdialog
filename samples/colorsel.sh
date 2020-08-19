#!/bin/sh
# --colorsel is not compatible with (c)dialog [ncurses]

COLORX=`Xdialog --stdout --title "Select color" --colorsel "Choose a color..." 0 0`

case $? in
	0)   echo "Selected color: $COLORX." ;;
	1)   echo "Cancel pressed." ;;
	255) echo "Box closed." ;;
esac
