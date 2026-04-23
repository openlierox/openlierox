#!/bin/sh

grep '<string name=' values/strings.xml | while read str; do

	if echo "$str" | grep 'translatable="false"' ; then continue; fi
	
	var=`echo $str | sed 's/<string name=["]\([^"]*\).*/\1/'`
	text=`echo $str | sed 's/<string name=["][^"]*["]>\([^<]*\).*/\1/'`
	if [ "$var" = "app_name" ]; then
		continue
	fi
	PRINTEN=true
	echo 
	for dir in values-*; do
		lang=`echo $dir | sed 's/[^-]*-\(..\).*/\1/'`
		trans=`grep "<string name=\"$var\">" $dir/strings.xml`
		transtext=`echo $trans | sed 's/<string name=["][^"]*["]>\([^<]*\).*/\1/'`
		if [ -z "$transtext" ] ; then
			#transtext=`./translate.py en $lang "$text"`
			#echo "$transtext" | grep 'Suspected Terms of Service Abuse' > /dev/null && transtext="$text"
			transtext="$text"
			grep -v "^[<]/resources[>]\$" $dir/strings.xml > $dir/strings.1.xml
			echo "<string name=\"$var\">$transtext</string>" >> $dir/strings.1.xml
			echo "</resources>" >> $dir/strings.1.xml
			mv -f $dir/strings.1.xml $dir/strings.xml
			if $PRINTEN ; then
				echo en: $var: $text
				PRINTEN=false
			fi
			echo $lang: $var: $transtext
		fi
	done
done
