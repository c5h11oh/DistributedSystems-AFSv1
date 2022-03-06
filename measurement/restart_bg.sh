sudo umount -l ~/mount
nohup ~/afs_client -s server:53706 -m ~/mount -c ~/journal/cache -f >/dev/null &