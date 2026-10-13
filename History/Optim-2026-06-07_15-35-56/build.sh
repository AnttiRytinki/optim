#!/bin/bash

printf '\e[8;48;160t'

gcc -Wall -Wextra -O2 *.c -lm -o opt_bench || exit 1

./opt_bench

TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
DESTINATION="History/Optim-$TIMESTAMP"

mkdir -p "$DESTINATION"

find . -maxdepth 1 -type f -exec cp {} "$DESTINATION" \;
