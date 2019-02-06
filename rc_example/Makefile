all:
	gcc -D_GNU_SOURCE -O3 -Wall -L/usr/lib rdma_rc_example.c util.c -libverbs -o rdma_rc_example
clean:
	rm -f *.o rdma_rc_example