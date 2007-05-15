#!/bin/bash
#
# Usage:   ./makeRecexpt.sh snapfile.snp experiment stationID
#          ./makeRecexpt.sh demo.snp EURO85 On
#
# Input:  SNAP file created with DRUDG from the schedule for your station
#
# Output: The 'recexpt_[stationID]' file for recording the experiment
#         You may need to edit 'recpass' for correct VSIB and data rate settings.
#
# ---code could be improved a LOT.....----

if [ "$1" == "" ] || [ "$2" == "" ] || [ "$3" == "" ]; then
   echo "Syntax: ./makeRecexpt.sh snapfile.snp experiment stationID"
   echo "        ./makeRecexpt.sh euro85on.snp EURO85 On"
   exit
fi

SNP=$1
EXPT=$2
STATION=$3

# get scan names, convert "," to space
sed -n -e '/scan_name=/s/scan_name=//p' $SNP | sed -n -e 's/,/ /gp' > scans

# get start times
sed -n -e '/preob/,/!/p' $SNP | grep ! | sed -n -e 's/[!.]/ /gp' > starttimes

# merge the two files
I=1
LC=`wc -l scans | awk '{print $1}'`
rm -f merged
while [ $I -le $LC ]
do
   SCAN_CURR=`tail -n +$I scans | head -1`
   TIME_CURR=`tail -n +$I starttimes | head -1`
   #echo " sc=${SCAN_CURR} tc=${TIME_CURR} "
   echo "${SCAN_CURR} ${TIME_CURR}" >> merged
   I=$(($I+1))
done

# create recexpt
cat recexpt.head > "recexpt_${EXPT}_$STATION.sh"
cat merged | while read scan expt dur1 dur2 year day clock; do 

   if [ "${dur1}" -ne "${dur2}" ]; then
      clock=$day; day=$year; year=$dur2; dur2=$dur1;
   fi

   # remove leading 0's from day, convert from day-of-year into date
   day=`echo ${day} | sed 's/^[0\t]*//'`;
   day=$(($day - 1))
   datestr=`date -d "01/01/${year} + ${day} days" +"%Y-%m-%d"`
   
   # output line format:
   #   ./dstart dateTtime; ./recpass euro85_mh_No0113_date 220
   # debug:
   #   echo "scan:${scan} expt:${expt} duration:${dur1}s/${dur2}s year:${year} day:${day} time:${clock}   -- ${datestr}T${clock}"

   echo " ./dstart ${datestr}T${clock}; ./recpass ${EXPT}_${STATION}_${scan}_${datestr} ${dur1}"   
   echo " ./dstart ${datestr}T${clock}; ./recpass ${EXPT}_${STATION}_${scan}_${datestr} ${dur1}" >> "recexpt_${EXPT}_$STATION.sh"
done
cat recexpt.tail >> "recexpt_${EXPT}_$STATION.sh"

chmod ug+x "recexpt_${EXPT}_$STATION.sh"
# rm merged

echo
echo "Script written into file:  recexpt_${EXPT}_$STATION.sh"
echo

