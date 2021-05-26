/** @file
 *
 * This file implements a file system from listing file.
 * Listing file format example:
 * 	/a/b/c.txt
 *	/a/c/e.dat
 *	/a/c/f.mp4
 *
 * ## Source code ##
 * \include filefs.c
 * \include n_tree.c
 * \include n_tree.h
 */

#define FUSE_USE_VERSION 34

#include <fuse.h>
#include <fuse_lowlevel.h>  /* for fuse_cmdline_opts */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <limits.h>

#include "n_tree.h"

/* We can't actually tell the kernel that there is no
   timeout, so we just send a big value */
#define NO_TIMEOUT 500000


/*
 * Filesystem entity structure
 *
 * Implemented with n-ary tree structure
 * Contains filename and pointers to next sibling and child.
 * if entity contains children, than it wil be processed as directory. Otherwise as file.
 */
node *fs_root = NULL;


/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct options {
	char* listing_file;
} options;


#define OPTION(t, p) { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
		OPTION("--listing-file=%s", listing_file),
		FUSE_OPT_END
};

static void *ffs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
	(void) conn;
	cfg->entry_timeout = NO_TIMEOUT;
	cfg->attr_timeout = NO_TIMEOUT;
	cfg->negative_timeout = 0;

	return NULL;
}

static int ffs_getattr(const char *path,
		struct stat *stbuf, struct fuse_file_info* fi) {

	size_t path_len = strlen(path);	// do it here because strdup in get_node damages string
	node *n = get_node(fs_root, path);

	if (!n)
		return -ENOENT;
	
	// it is directory
	if (n->child) {
		stbuf->st_ino = 1;
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 1;
	}
	// it is file
	else {
		stbuf->st_ino = 2;
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		// default file contents is:
		// FILENAME is in LISING_FILE
		stbuf->st_size = path_len + strlen(options.listing_file) + strlen(" is in ");
	}

	return 0;
}

static int ffs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi,
		enum fuse_readdir_flags flags) {
	(void) fi;
	(void) offset;
	(void) flags;{
		(void) filler;
		(void) buf;
		struct stat file_stat;
		char filepath[2048];

		node *n = get_node(fs_root, path);
		if (!n || !n->child)
			return -ENOTDIR;

		n = n->child;
		while (n) {
			sprintf(filepath, "%s/%s", path, n->data);
			ffs_getattr(filepath, &file_stat, NULL);
			filler(buf, n->data, &file_stat, 0, 0);
			n = n->next;
		}

		return 0;
	}
}

static int ffs_open(const char *path, struct fuse_file_info *fi) {
	(void) path;
	if (!file_exists(path))
		return -ENOENT;

	return 0;
}

static int ffs_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	size_t len;
	char data[2048];
	(void) fi;
	
	// default file contents is:
	// FILENAME is in LISING_FILE
	sprintf(data, "%s is in %s", path, options.listing_file);
	len = strlen(data);

	if (!file_exists(path))
		return -ENOENT;

	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, data + offset, size);
	} else
		size = 0;

	return size;

}

static const struct fuse_operations xmp_oper = {
		.init     = ffs_init,
		.getattr  = ffs_getattr,
		.readdir  = ffs_readdir,
		.open     = ffs_open,
		.read     = ffs_read,
};

int file_exists(const char *path) {
	return get_node(fs_root, path);
}

int dir_exists(const char *path) {
	node *n = get_node(fs_root, path);
	//if node contains children, then it is directory
	if (!n)
		return 0;

	return n->child;
}

void create_entities(char* path) {
	char *pch, *saveptr;
	node* current_node = fs_root;
	pch = strtok_r(path,"/", &saveptr);
	while (pch != NULL) {

		// check if this node already exists
		node* n =  get_child(current_node, pch);
		if (n)
			current_node = n;
		else
			current_node = add_child(current_node, pch);
			
		pch = strtok_r(NULL, "/", &saveptr);
	}
}

void parse_listing_file(char* listing_file_path) {
	FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(listing_file_path, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
    		printf("%d %d %s\n",read, len, line);
    		// remove trailing \n char
    		if (line[read - 1] == '\n')
    			line[read - 1] = '\0';
		create_entities(line);
    }

    fclose(fp);
    if (line)
        free(line);
}

static void show_help(const char *progname)
{
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
			"    --listing-file=<path>  text file with filelist\n"
			"Listing file format should be as follows:\n"
			"/a/b/c.txt\n"
			"/a/c/e.dat\n"
			"/a/c/f.mp4\n"
			"\n");
}

int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse *fuse;
	struct fuse_cmdline_opts opts;
	struct fuse_loop_config config;
	int res;


	// default value for listing file
	options.listing_file = strdup("/dev/null");

	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	if (fuse_parse_cmdline(&args, &opts) != 0)
		return 1;

	// FUSE default command line options
	if (opts.show_version) {
		printf("FUSE library version %s\n", fuse_pkgversion());
		fuse_lowlevel_version();
		res = 0;
		goto out1;
	} else if (opts.show_help) {
		show_help(argv[0]);
		fuse_cmdline_help();
		fuse_lib_help(&args);
		res = 0;
		goto out1;
	} else if (!opts.mountpoint) {
		fprintf(stderr, "error: no mountpoint specified\n");
		res = 1;
		goto out1;
	}

	// initialize root FS node
	fs_root = new_node("/");
	// parse file
	parse_listing_file(options.listing_file);


	fuse = fuse_new(&args, &xmp_oper, sizeof(xmp_oper), NULL);
	if (fuse == NULL) {
		res = 1;
		goto out1;
	}

	if (fuse_mount(fuse,opts.mountpoint) != 0) {
		res = 1;
		goto out2;
	}

	if (fuse_daemonize(opts.foreground) != 0) {
		res = 1;
		goto out3;
	}

	struct fuse_session *se = fuse_get_session(fuse);
	if (fuse_set_signal_handlers(se) != 0) {
		res = 1;
		goto out3;
	}

	if (opts.singlethread)
		res = fuse_loop(fuse);
	else {
		config.clone_fd = opts.clone_fd;
		config.max_idle_threads = opts.max_idle_threads;
		res = fuse_loop_mt(fuse, &config);
	}
	if (res)
		res = 1;

	fuse_remove_signal_handlers(se);
out3:
	fuse_unmount(fuse);
out2:
	fuse_destroy(fuse);
out1:
	free(opts.mountpoint);
	fuse_opt_free_args(&args);
	return res;
}
