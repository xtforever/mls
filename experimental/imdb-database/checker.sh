#!/bin/bash
awk -F'\t' '!seen[$2]++ { print $2 "\t" NR "\t" $0 }' movies-guess.tsv |
sort -t$'\t' -k1,1 |
head |
cut -f2-

