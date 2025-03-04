#!/bin/bash

ADAPTIVED=0
ADAPTIVEMM=0

ADAPTIVEMM_FILES=(
	"adaptivemmd.8"
	"adaptivemmd.c"
	"adaptivemmd.cfg"
	"adaptivemmd.service"
	"adaptivemm.spec"
	"build_spec.yaml"
	"CONTRIBUTING.md"
	"LICENSE.txt"
	"Makefile"
	"predict.c"
	"predict.h"
	"README.md"
	"SECURITY.md"
)

RETURN_ADAPTIVED=false

START_HASH="origin/master"
END_HASH="HEAD"

VERBOSE=false
PRINTED_FILES=false

while getopts demsv option
do
case "${option}"
in
d) RETURN_ADAPTIVED=true;;
e)
	eval nextopt=\${$OPTIND}
	if [[ -n $nextopt && $nextopt != -* ]] ; then
		OPTIND=$((OPTIND + 1))
		END_HASH=$nextopt
	fi
;;
m) RETURN_ADAPTIVED=false;;
s)
	eval nextopt=\${$OPTIND}
	if [[ -n $nextopt && $nextopt != -* ]] ; then
		OPTIND=$((OPTIND + 1))
		START_HASH=$nextopt
	fi
;;
v) VERBOSE=true;;
esac
done

if [[ $RETURN_ADAPTIVED == true ]];
then
	DIR=adaptived/
else
	DIR=adaptivemm/
fi
CMD="git diff --name-only $START_HASH..$END_HASH $DIR"
FILES=$($CMD)

if [[ $VERBOSE == true ]];
then
	echo "$CMD"
	echo "-------------------------------"
fi

for FILE in $FILES;
do
	if [[ $FILE == adaptived/* ]];
	then
		ADAPTIVED=1

		if [[ $VERBOSE == true ]];
		then
			echo "$FILE"
			PRINTED_FILES=true
		fi
	fi

	for AMM_FILE in ${ADAPTIVEMM_FILES[@]};
	do
		if [[ $FILE == $AMM_FILE ]];
		then
			ADAPTIVEMM=1

			if [[ $VERBOSE == true ]];
			then
				echo "$FILE"
				PRINTED_FILES=true
			fi
		fi
	done
done


if [[ $VERBOSE == true ]];
then
	if [[ $PRINTED_FILES == false ]];
	then
		echo "No files were modified"
	fi
fi

if [[ $RETURN_ADAPTIVED == true ]];
then
	if [[ $VERBOSE == true ]];
	then
		echo "-------------------------------"
	else
		echo $ADAPTIVED
	fi
else
	if [[ $VERBOSE == true ]];
	then
		echo "-------------------------------"
	else
		echo $ADAPTIVEMM
	fi
fi

exit 0
