#!/bin/sh

# build it
# Autoconf 2.69
# Automake 1.15

aclocal
autoconf
automake --add-missing

./configure

make # emmcdl linux binary in .
