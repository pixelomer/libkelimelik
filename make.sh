#!/bin/bash

set -e

rm -f *.o
clang -Iheaders -c src/*.c
clang -Iheaders examples/account-info/*.c *.o -o account-info
clang -Iheaders examples/tests/*.c *.o -o tests
clang -Iheaders examples/proxy/*.c *.o -o proxy