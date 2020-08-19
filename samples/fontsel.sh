#!/bin/sh
# --fontsel is not compatible with (c)dialog [ncurses]

FONTX=`Xdialog --stdout --title "Select color" --fontsel "Choose a font..." 0 0`

case $? in
	0)   echo "Selected color: $FONTX." ;;
	1)   echo "Cancel pressed." ;;
	255) echo "Box closed." ;;
esac
