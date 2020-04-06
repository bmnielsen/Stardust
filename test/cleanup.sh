#!/bin/bash
killall tests || true
ipcs | grep 0x00000000 | tr -s ' ' | cut -d' ' -f2 | xargs -n1 ipcrm -m
