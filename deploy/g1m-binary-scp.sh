#!/bin/bash
PREFIX="/p2/repo/deploy"
/p2/pssh-2.3.1/bin/pscp -h ${PREFIX}/g1m-hosts-client.txt -o ${PREFIX}/g1m-client-stdout -e ${PREFIX}/g1m-server-stderr /p2/repo/build/O3/afs_client ~
/p2/pssh-2.3.1/bin/pscp -h ${PREFIX}/g1m-hosts-server.txt -o ${PREFIX}/g1m-client-stdout -e ${PREFIX}/g1m-server-stderr /p2/repo/build/O3/afs_server ~