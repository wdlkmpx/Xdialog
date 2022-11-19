#!/bin/sh
# --colorsel is not compatible with (c)dialog [ncurses]

DIALOG='Xdialog'
if [ -x ./Xdialog ] ; then
    DIALOG='./Xdialog'
elif [ -x ../Xdialog ] ; then
    DIALOG='../Xdialog'
fi

case $1 in
    cli|-cli|--cli)
        DIALOG='dialog'
        echo "this is not compatible with (c)dialog [ncurses]"
        exit 0
        ;;
    *dialog*)
        if [ -x $1 ] ; then
            DIALOG=$1
            shift
        fi
        ;;
esac

echo "$DIALOG"

#================================

COLORX=`$DIALOG --stdout --title "Select color" --colorsel "Choose a color..." 0 0`

case $? in
	0)   echo "Selected color: $COLORX." ;;
	1)   echo "Cancel pressed." ;;
	255) echo "Box closed." ;;
esac
