#!/bin/bash
sudo umount -l ~/mount
rm ~/journal/cache/*
~/afs_client -s server:53706 -m ~/mount -c ~/journal/cache -f