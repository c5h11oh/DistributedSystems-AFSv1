#!/bin/bash
PREFIX="/p2/repo/g1n"
/p2/pssh-2.3.1/bin/pscp -h ${PREFIX}/c15.txt -o ${PREFIX}/g1n-client-stdout -e ${PREFIX}/g1n-server-stderr /p2/repo/build/O3/afs_client ~
/p2/pssh-2.3.1/bin/pscp -h ${PREFIX}/c15.txt -o ${PREFIX}/g1n-client-stdout -e ${PREFIX}/g1n-server-stderr -r /p2/repo/measurement ~
/p2/pssh-2.3.1/bin/pscp -h ${PREFIX}/g1n-hosts-server.txt -o ${PREFIX}/g1n-client-stdout -e ${PREFIX}/g1n-server-stderr /p2/repo/build/O3/afs_server ~