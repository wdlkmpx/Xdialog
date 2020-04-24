#!/bin/sh

if [ "$1" ] ; then
	DIALOG=$1
else
	DIALOG=Xdialog
fi

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
	--progress "Processing 200 entries..." \
	8 60 $ENTRIES

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
	--progress "Hi, this is a progress widget" \
	8 60 6

if [ "$?" = 255 ] ; then
	echo ""
	echo "Box closed !"
fi
