#!/bin/bash
rm /users/c5h11oh/journal/cache/*
for i in {1..4}; do
    /users/c5h11oh/measurement/loop_write_client &
done