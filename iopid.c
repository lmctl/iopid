/* iopid: print io stats for pid
 *
 * Copyright (C) 2016  Karol Lewandowski <lmctl@lmctl.net>
 *
 * License: GNU General Public License v2.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <signal.h>

#define STATIC_LEN(str) (sizeof(str) - 1)

char *progname;

int update_winch; // flag only
int redraw_row;   // redraw header every this row

enum {
     INDEX_RCHAR = 0,
     INDEX_WCHAR = 1,
     INDEX_SYSCR = 2,
     INDEX_SYSCW = 3,
     INDEX_RBYTES = 4,
     INDEX_WBYTES = 5,
     INDEX_CANCELLED = 6,
     _INDEX_MAX_SIZE,
};

static char *index_names[] = {
     [INDEX_RCHAR] = "rchar",
     [INDEX_WCHAR] = "wchar",
     [INDEX_SYSCR] = "syscr",
     [INDEX_SYSCW] = "syscw",
     [INDEX_RBYTES] = "rbytes",
     [INDEX_WBYTES] = "wbytes",
     [INDEX_CANCELLED] ="cancelled"
};

void usage(void)
{
     fprintf(stderr, "usage: %s PID INTERVAL\n", progname);
}

void io_parse_line(char *line, int len, uint64_t *io)
{
     char *p = line;
     int index;

     if (p[0] == 'c') {
	  p += STATIC_LEN("cancelled_write_bytes: ");
	  index = INDEX_CANCELLED;
     } else if (p[0] == 'r') {
	  if (p[1] == 'c') {
	       p += STATIC_LEN("rchar: ");
	       index = INDEX_RCHAR;
	  } else {
	       p += STATIC_LEN("read_bytes: ");
	       index = INDEX_RBYTES;
	  }
     } else if (p[0] == 'w') {
	  if (p[1] == 'c') {
	       p += STATIC_LEN("wchar: ");
	       index = INDEX_WCHAR;
	  } else {
	       p += STATIC_LEN("write_bytes: ");
	       index = INDEX_WBYTES;
	  }
     } else if (p[0] == 's') {
	  if (p[4] == 'r')
	       index = INDEX_SYSCR;
	  else
	       index = INDEX_SYSCW;

	  p += STATIC_LEN("syscX: ");
     } else
	  abort();

     sscanf(p, "%" PRIu64, &io[index]);
}

void annotate_num(uint64_t n, char str[])
{
     char *p = NULL;
     double num;

#define SZ_1GB (1014 * 1024 * 1024)
#define SZ_1MB (1014 * 1024)
#define SZ_1KB (1024)

     if (n >= SZ_1GB) {
	  p = "G";
	  num = n / SZ_1GB;
     } else if (n > SZ_1MB) {
	  p = "M";
	  num = n / SZ_1MB;
     } else if (n > SZ_1KB) {
	  p = "k";
	  num = n / SZ_1KB;
     }

     if (p)
	  sprintf(str, "%-4.1f%s", num, p);
     else
	  sprintf(str, "%-8" PRIu64, n);
}

void print_line(uint64_t *ioc, uint64_t *iop)
{
     int i;

     for (i = 0; i < _INDEX_MAX_SIZE; i++) {
	  static char str[16];
	  uint64_t n = iop ? ioc[i] - iop[i] : ioc[i];

	  annotate_num(n, str);
	  printf("%-8s ", str);
     }
     printf("\n");
}

static void __draw_header(void)
{
     int i;

     for (i = 0; i < _INDEX_MAX_SIZE; i++)
	  printf("%-8s ", index_names[i]);
     printf("\n");
}

static void draw_header(void)
{
     static int cur_row = -1;

     ++ cur_row;

     if (0 == cur_row)
	  goto out;

     if (!redraw_row || cur_row < redraw_row)
	  return;

out:
     cur_row = 2; // 2 accounts header and data line
     __draw_header();
}

void setup_winch(void)
{
     struct winsize ws;
     int r;

     r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
     if (r == -1)
	  return;

     redraw_row = ws.ws_row;

     update_winch = 0;
}

void handle_winch(int signo)
{
     (void)signo;

     ++ update_winch;
}

int main(int argc, char *argv[])
{
     int pid;
     int interval;
     uint64_t io[2][_INDEX_MAX_SIZE] = { [0] = {0, 0, 0, 0, 0, 0, 0} };
     int io_pos = 0;
     char path[PATH_MAX];
     char line[LINE_MAX];
     uint64_t *iop = NULL; // previous entry
     uint64_t *ioc;        // current
     int n;

     progname = basename(argv[0]);

     setvbuf(stdout, NULL, _IOLBF, 0);

     if (argc != 3)
	  goto err;

     pid = atoi(argv[1]);
     interval = atoi(argv[2]);
     if (!pid || !interval)
	  goto err;

     n = snprintf(path, sizeof path, "/proc/%d/io", pid);
     if (sizeof path == n)
	  goto err; // no space for \0

     if (isatty(STDOUT_FILENO)) {
	  ++ update_winch;
	  signal(SIGWINCH, handle_winch);
     }

     while (1) {
	  char *p;

	  FILE *fp = fopen(path, "r");
	  if (!fp) {
	       fprintf(stderr, "%s: unable to open %s: %m\n", progname, path);
	       exit(EXIT_FAILURE);
	  }

	  ioc = &io[io_pos][0];
	  memset(ioc, 0, _INDEX_MAX_SIZE*sizeof(io[0][0]));

	  while (p = fgets(line, sizeof line, fp))
	       io_parse_line(line, sizeof line, ioc);

	  fclose(fp);

	  if (update_winch)
	       setup_winch();

	  draw_header();
	  print_line(ioc, iop);

	  iop = ioc;
	  io_pos = (io_pos + 1) % 2;

	  sleep(interval);
     }

     return 0;

err:
     usage();
     exit(EXIT_FAILURE);
}
