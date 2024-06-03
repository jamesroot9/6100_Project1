#!/bin/bash
set -e

make FAST=1


for c in $(seq 12 15); do
    for b in $(seq 4 7); do
        for p in $(seq 12 $(( 14 < $c ? 14 : $c ))); do
            for s in $(seq $((c-p)) $((c-b-5))); do
                for t in $(seq 5 $((c-b-s))); do
#                    for m in $(seq $p $(( 20 < $((32-p)) ? 20 : $((32-p))))); do
                            echo "$c,$b,$p,$s,$t,$(( 20 < $((32-p)) ? 20 : $((32-p))))," >> results_gcc.txt;
                            ./cachesim -c "$c" -b "$b" -s "$s" -p "$p" -t "$t" -m "$(( 20 < $((32-p)) ? 20 : $((32-p))))" -v 1 < traces/gcc.trace >> results_gcc.txt
 #                           echo "\n" >> results_gcc.txt;
 #                       done
                    done
                done
            done
        done
    done

