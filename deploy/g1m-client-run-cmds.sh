#!/bin/bash
PREFIX="/p2/repo/deploy"

while IFS= read -r cmd; do
  /p2/pssh-2.3.1/bin/pssh -h ${PREFIX}/g1m-hosts-client.txt -o ${PREFIX}/g1m-client-stdout -e ${PREFIX}/g1m-client-stderr $cmd
done </p2/repo/g1m-client-run-cmds.txt