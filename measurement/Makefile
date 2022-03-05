all: driver_read driver_write

driver_read: driver_read.cc timespec_lib.cc
	g++ driver_read.cc timespec_lib.cc -o driver_read

driver_write: driver_write.cc timespec_lib.cc
	g++ driver_write.cc timespec_lib.cc -o driver_write

.PHONY: clean
clean:
	rm -f driver_write driver_read