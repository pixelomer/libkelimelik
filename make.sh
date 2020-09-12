#!/bin/bash

set -e

rm -f *.o
clang -Iheaders -c src/*.c
clang -Wall -O2 -Iheaders examples/account-info/*.c *.o -o account-info
clang -Wall -O2 -Iheaders examples/tests/*.c *.o -o tests
clang -Wall -O2 -Iheaders examples/proxy/*.c *.o -o proxy
