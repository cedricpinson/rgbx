

list="../tests/Alexs_Apt_2k.hdr
../tests/Arches_E_PineTree_3k.hdr
../tests/GCanyon_C_YumaPoint_3k.hdr
../tests/Mans_Outside_2k.hdr
../tests/Milkyway_small.hdr"

#list="../tests/Alexs_Apt_2k.hdr"

#rgbm
range_list="4 8 16 32"
methods="rgbm"
for method in ${methods}
do
    for file in ${list}
    do

        filename=$(basename "$file")
        extension="${filename##*.}"
        filename="${filename%.*}"

        for range in ${range_list}
        do
            ./rgbx -m ${method} -r ${range} ${file} "${filename}_${method}_${range}.png"
            ./rgbx -d -m ${method} -r ${range} "${filename}_${method}_${range}.png" "${filename}_${method}_${range}_decode.tif"
        done
    done
done

method="rgbe"
for file in ${list}
do

    filename=$(basename "$file")
    extension="${filename##*.}"
    filename="${filename%.*}"

    ./rgbx -m ${method} ${file} "${filename}_${method}.png"
    ./rgbx -d -m ${method} "${filename}_${method}.png" "${filename}_${method}_decode.tif"

done
