#!/bin/sh

if [ "$#" -ne 2 ]
then
    echo "Error. You must pass two arguments"
    exit 1
fi

filesdir=$1
searchstr=$2

if [ -d "$1" ]
then
    echo "The number of files are ";
    (find ${filesdir} -type f | wc -l);
    echo "and the number of matching lines are";
    (grep -rn ${searchstr} ${filesdir} | wc -l)
    exit 0
else
    echo "Directory ${1} not found"
    exit 1
fi