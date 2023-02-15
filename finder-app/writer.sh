#!/bin/sh

if [ "$#" -ne 2 ]
then
    echo "Error. You must pass two arguments"
    exit 1
fi

writefile=$1
writestr=$2

DIR="$(dirname "${writefile}")" ; FILE="$(basename "${writefile}")"

echo ${DIR}
echo ${FILE}
echo ${writefile}

if [ ! -d ${DIR} ]
then
    mkdir -p ${DIR}
fi

if [ ! -e ${writefile} ]
then
    touch ${writefile}
fi

if [ ! -f ${writefile} ]
then
    echo "Cannot write to ${writefile}"
    exit 1
fi

echo ${writestr} > ${writefile}
exit 0

