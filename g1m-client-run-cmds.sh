#!/bin/bash
# while IFS= read -r cmd; do
# cmd="sudo mkfs.ext4 -F /dev/xvda4"
while IFS= read -r cmd; do
  /p2/pssh-2.3.1/bin/pssh -h /p2/repo/g1m-hosts-client.txt -o g1m-client-stdout -e g1m-client-stderr $cmd
done </p2/repo/g1m-client-run-cmds.txt