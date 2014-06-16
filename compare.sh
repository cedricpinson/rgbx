

list="../tests/Alexs_Apt_2k.hdr
../tests/Arches_E_PineTree_3k.hdr
../tests/GCanyon_C_YumaPoint_3k.hdr
../tests/Mans_Outside_2k.hdr
../tests/Milkyway_small.hdr"

#list="../tests/Alexs_Apt_2k.hdr"

methods="rgbm rgbe rgbd rgbd2"
for method in ${methods}
do
    for file in ${list}
    do

        filename=$(basename "$file")
        extension="${filename##*.}"
        filename="${filename%.*}"

        range="$(iinfo --stats ${file} | grep Max | sed 's/ *Stats Max://' | sed 's/(float)//g' | tr ' ' '\n' | sort -r | head -1 )"

        ./rgbx -m ${method} -r ${range} ${file} "${filename}_${method}_${range}.png"
        ./rgbx -d -m ${method} -r ${range} "${filename}_${method}_${range}.png" "${filename}_${method}_${range}_decode.tif"

    done
done
