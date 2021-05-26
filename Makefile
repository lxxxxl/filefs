all:
	$(CC) -Wall -g filefs.c n_tree.c `pkg-config fuse3 --cflags --libs` -o filefs

clean:
	rm filefs

