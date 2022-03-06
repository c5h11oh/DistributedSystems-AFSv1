#!/bin/bash
PREFIX="/p2/repo/g1n"

while IFS= read -r cmd; do
  /p2/pssh-2.3.1/bin/pssh -h ${PREFIX}/g1n-hosts-client.txt -o ${PREFIX}/g1n-client-stdout -e ${PREFIX}/g1n-client-stderr $cmd
done </p2/repo/g1n/g1n-client-run-cmds.txt