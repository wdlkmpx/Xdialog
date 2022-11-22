#!/bin/sh

DIALOG='Xdialog'
if [ -x ./Xdialog ] ; then
    DIALOG='./Xdialog'
elif [ -x ../Xdialog ] ; then
    DIALOG='../Xdialog'
elif [ -x ../src/Xdialog ] ; then
    DIALOG='../src/Xdialog'
fi

case $1 in
    cli|-cli|--cli)
        DIALOG='dialog'
        if [ -n "$NO_NCURSES_COMPAT" ] ; then
            echo "this is not compatible with (c)dialog [ncurses]"
            exit 0
        fi
        Xdialog_opts=''
        shift
        ;;
    *dialog*)
        if [ -x $1 ] ; then
            DIALOG=$1
            shift
        fi
        ;;
esac

echo "$DIALOG"


