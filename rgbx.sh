#!/usr/local/bin/bash
filename=$1
range=$2
methods="rgbe rgbm rgbd rgbd2"

if [ "$#" == 3 ]
then
    methods="$3"
fi

for method in ${methods}
do
    ./rgbx -m ${method} -r ${range} ${filename} "${filename}_${method}_${range}.tga"
    ./rgbx -d -m ${method} -r ${range} "${filename}_${method}_${range}.tga" "${filename}_${method}_${range}_decode.tif"
    iinfo --stats "${filename}_${method}_${range}_decode.tif"
done
