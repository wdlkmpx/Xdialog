#!/bin/sh
# --combobox is not compatible with (c)dialog [ncurses]

NO_NCURSES_COMPAT=1
. ./0common.sh || exit 1

ITEMX=$(
$DIALOG --stdout \
        --title "Select item" \
        --combobox "This is a combo..." 0 0 \
            item1 item2 item3 item4 item5 \
            item6 item7 item8 item9 item10
)

case $? in
	0)   echo "Selected item: $ITEMX." ;;
	1)   echo "Cancel pressed." ;;
	255) echo "Box closed." ;;
esac
