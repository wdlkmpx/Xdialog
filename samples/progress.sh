#!/bin/sh
# --progress is not compatible with (c)dialog [ncurses]

NO_NCURSES_COMPAT=1
. ./0common.sh || exit 1

#====================================================

ENTRIES=200
(
i=0
while [ $i -le $ENTRIES ]
do
	echo "X"
	sleep 0.1
done
) |
$DIALOG --title "PROGRESS" \
        --progress "Processing 200 entries..." 8 60 $ENTRIES

#====================================================

# 6 "dots"
(
echo "X" ; sleep 1
echo "X" ; sleep 1
echo "X" ; sleep 1
echo "X" ; sleep 1
echo "X" ; sleep 1
echo "X" ; sleep 1
) |
$DIALOG --title "PROGRESS" \
        --progress "Hi, this is a progress widget" 8 60 6

if [ "$?" = 255 ] ; then
	echo ""
	echo "Box closed !"
fi

#====================================================

# Specify %
(
echo "10" ; sleep 1
echo "20" ; sleep 1
echo "30" ; sleep 1
echo "40" ; sleep 1
echo "10" ; sleep 1
echo "50" ; sleep 1
echo "70" ; sleep 1
echo "80" ; sleep 1
echo "90" ; sleep 1
echo "95" ; sleep 1
echo "100" ; sleep 1
) |
$DIALOG --title "PROGRESS %" \
	--progress "" \
	8 60

if [ "$?" = 255 ] ; then
	echo ""
	echo "Box closed !"
fi
