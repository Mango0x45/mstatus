#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <linux/limits.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <X11/Xlib.h>

#define CTOI(x) ((x) ^ 48)

struct Block {
	int pos;
	char *text;
	bool remove;
};

bool rflag;
const char *argv0;
struct {
	char *str;
	size_t len;
} seperator = {.str = " | ", .len = 3};

static noreturn void usage(void);
static noreturn void die(const char *);
static void xfork(void);
static void *xrealloc(void *, size_t);
static void xfree(char **);
static void write_status(struct Block);
static bool process(char *, struct Block *);
static void create_fifo(char *);
static void daemonize(void);

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-r] [-s seperator]\n", argv0);
	exit(EXIT_FAILURE);
}

void
die(const char *s)
{
	char *err = strerror(errno);
	syslog(LOG_ERR, "%s: %s", s, err);
	fprintf(stderr, "%s: %s: %s\n", argv0, s, err);
	exit(EXIT_FAILURE);
}

void
xfork(void)
{
	pid_t pid = fork();
	if (pid == -1)
		die("fork");
	if (pid != 0)
		exit(EXIT_SUCCESS);
}

void *
xrealloc(void *ptr, size_t size)
{
	void *ret;
	if (!(ret = realloc(ptr, size)))
		die("realloc");
	return ret;
}

void
xfree(char **ptr)
{
	free(*ptr);
	*ptr = NULL;
}

void
write_status(struct Block b)
{
	static struct {
		char **blocks;
		int count;
		size_t length;
	} sb;

	if (b.remove) {
		if (b.pos > sb.count)
			return;

		/* b.remove && !b.pos is a special case to remove everything from the bar */
		if (!b.pos) {
			for (int i = 0; i < sb.count; i++)
				free(sb.blocks[i]);
			sb.count = 0;
			sb.blocks = xrealloc(sb.blocks, sizeof(char *));
			goto update_bar;
		}

		/* If the block is not NULL we free it */
		if (sb.blocks[--b.pos]) {
			sb.length -= strlen(sb.blocks[b.pos]);
			xfree(&sb.blocks[b.pos]);
		}

		/* If the block is the last one, we resize the bar to remove trailing NULL blocks */
		if (b.pos + 1 == sb.count) {
			for (; !sb.blocks[b.pos] && b.pos; b.pos--)
				sb.count--;
			sb.blocks = xrealloc(sb.blocks, sizeof(char *) * ++b.pos);
		}
		goto update_bar;
	}

	/* If the position exceeds the space allocated, allocate more blocks */
	if (b.pos > sb.count) {
		sb.blocks = xrealloc(sb.blocks, sizeof(char *) * b.pos);
		/* Make sure to set all the newly allocated blocks to NULL */
		for (int i = sb.count; i < b.pos; i++)
			sb.blocks[i] = NULL;
		sb.count = b.pos;
	}

	/* If the block is NULL we dont need to bother with strlen and free */
	if (sb.blocks[--b.pos]) {
		sb.length -= strlen(sb.blocks[b.pos]);
		xfree(&sb.blocks[b.pos]);
	}
	if (!(sb.blocks[b.pos] = strdup(b.text)))
		die("strdup");
	sb.length += strlen(b.text);

	/* The buffer to store the text that will be displayed in. It needs space for the text, the
	 * seperators between the different blocks, the NUL byte at the end, and the right padding
	 * space.
	 */
update_bar:;
	char buf[sb.length + (sb.count - 1) * seperator.len + 2];
	memset(buf, '\0', sizeof(buf));

	/* Double for loops so that the seperator isnt printed to the left of the first block */
	int i;
	for (i = 0; i < sb.count; i++) {
		if (sb.blocks[i]) {
			strcpy(buf, sb.blocks[i]);
			break;
		}
	}
	for (i++; i < sb.count; i++)
		if (sb.blocks[i])
			sprintf(buf, "%s%s%s", buf, seperator.str, sb.blocks[i]);
	if (rflag)
		strcat(buf, " ");

	/* Xlib magic to set the DWM status */
	Display *dpy = XOpenDisplay(NULL);
	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);
	(void) XStoreName(dpy, root, buf);
	(void) XCloseDisplay(dpy);
}

bool
process(char *line, struct Block *b)
{
	if (*line == '-') {
		b->remove = true;
		line++;
	} else
		b->remove = false;

	if (!isdigit(*line))
		b->pos = 1; /* Default position */
	else {
		b->pos = 0;
		while (isdigit(*line))
			b->pos = b->pos * 10 + CTOI(*line++);
		if (!b->pos && !b->remove)
			return false;
	}

	b->text = line;
	return true;
}

void
create_fifo(char *fifo_path)
{
	char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (runtime_dir) {
		size_t end = strlen(runtime_dir) - 1;
		if (runtime_dir[end] == '/')
			runtime_dir[end] = '\0';
		sprintf(fifo_path, "%s/%s.pipe", runtime_dir, argv0);
	} else
		sprintf(fifo_path, _PATH_VARRUN "user/%d/%s.pipe", getuid(), argv0);

	umask(0);

create_fifo:
	if (mkfifo(fifo_path, DEFFILEMODE) == -1) {
		if (errno == EEXIST) {
			if (unlink(fifo_path) == -1)
				die("unlink");
			goto create_fifo;
		} else
			die("mkfifo");
	}

	syslog(LOG_INFO, "Created input FIFO '%s'", fifo_path);
}

void
daemonize(void)
{
	xfork();
	if (setsid() == -1)
		die("setsid");

	(void) signal(SIGCHLD, SIG_IGN);
	xfork();

	(void) chdir("/");
	(void) close(STDIN_FILENO);
	(void) close(STDOUT_FILENO);
	(void) close(STDERR_FILENO);

	stdin = fopen(_PATH_DEVNULL, "r");
	stdout = fopen(_PATH_DEVNULL, "w+");
	stderr = fopen(_PATH_DEVNULL, "w+");

	syslog(LOG_INFO, "Daemonized '%s'", argv0);
}

int
main(int argc, char **argv)
{
	argv0 = argv[0];

	int opt;
	while ((opt = getopt(argc, argv, ":rs:")) != -1) {
		switch (opt) {
		case 'r':
			rflag = true;
			break;
		case 's':
			seperator.str = optarg;
			seperator.len = strlen(optarg);
			break;
		default:
			usage();
		}
	}

	openlog(argv0, LOG_PID | LOG_CONS, LOG_DAEMON);
	char fifo_path[PATH_MAX];
	create_fifo(fifo_path);
	daemonize();

	char *line = NULL;
	size_t len = 0;
	while (true) {
		FILE *fp;
		if (!(fp = fopen(fifo_path, "r")))
			die("fopen");

		ssize_t nr;
		while ((nr = getline(&line, &len, fp)) != -1) {
			/* For some reason output with newlines can cause performance issues */
			if (line[--nr] == '\n')
				line[nr] = '\0';

			syslog(LOG_INFO, "Recieved command '%s'", line);

			struct Block b;
			if (!process(line, &b))
				continue;
			write_status(b);
		}
		if (ferror(fp))
			die("getline");

		(void) fclose(fp);
	}
	/* NOTREACHED */
}
