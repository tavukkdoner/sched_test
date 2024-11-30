#!/system/bin/sh
for BLOCK in mmcblk0 #mtdblock5
do	
	echo "Testing: read dev/block/$BLOCK"
	for SCHED in mq-deadline kyber bfq none
	#for SCHED in noop anticipatory deadline cfq bfq
	do
		if [ $SCHED = "anticipatory" -o $SCHED = "deadline" ] ; then
			echo -en "$SCHED\t"
		else
			echo -en "$SCHED\t\t"
		fi
		echo $SCHED > /sys/block/$BLOCK/queue/scheduler
		./read_test /dev/block/$BLOCK
	done
done

BLOCK="mmcblk0"
for FILE in /sdcard/test.bin  /data/test.bin
do	
	#if [ $FILE = "/sdcard/test.bin" ] ; then
	#	BLOCK="mmcblk0"
	#else
	#	BLOCK="mtdblock5"
	#fi
	echo "Testing: write dev/block/$BLOCK"
	for SCHED in mq-deadline kyber bfq none
	#for SCHED in noop anticipatory deadline cfq bfq
	do
		if [ $SCHED = "anticipatory" -o $SCHED = "deadline" ] ; then
			echo -en "$SCHED\t"
		else
			echo -en "$SCHED\t\t"
		fi
		echo $SCHED > /sys/block/$BLOCK/queue/scheduler
		./write_test $FILE
		rm $FILE
	done
done