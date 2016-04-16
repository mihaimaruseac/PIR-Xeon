#!/bin/bash

moduli="1024 2048"
dbsizes="65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432"
ops="8 16 32 64 128 256 512 1024 2048 4096"

for m in ${moduli}; do
    for n in ${dbsizes}; do
        for k in ${ops}; do
            timeout 10s ./ko -m ${m} -n ${n} -k ${k}
        done
    done
done
