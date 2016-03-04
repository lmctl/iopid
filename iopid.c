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
#include <libgen.h>

#define STATIC_LEN(str) (sizeof(str) - 1)

char *progname;

typedef struct proc_io {
     uint64_t rchar;
     uint64_t wchar;
     uint64_t syscr;
     uint64_t syscw;
     uint64_t read_bytes;
     uint64_t write_bytes;
     uint64_t cancelled_write_bytes;
} proc_io_t;

void usage(void)
{
     fprintf(stderr, "usage: %s PID INTERVAL\n", progname);
}

void io_parse_line(char *line, int len, proc_io_t *io)
{
     char *p = line;
     uint64_t *pnum = 0;

     if (p[0] == 'c') {
	  p += STATIC_LEN("cancelled_write_bytes: ");
	  pnum = &io->cancelled_write_bytes;
     } else if (p[0] == 'r') {
	  if (p[1] == 'c') {
	       p += STATIC_LEN("rchar: ");
	       pnum = &io->rchar;
	  } else {
	       p += STATIC_LEN("read_bytes: ");
	       pnum = &io->read_bytes;
	  }
     } else if (p[0] == 'w') {
	  if (p[1] == 'c') {
	       p += STATIC_LEN("wchar: ");
	       pnum = &io->wchar;
	  } else {
	       p += STATIC_LEN("write_bytes: ");
	       pnum = &io->write_bytes;
	  }
     } else if (p[0] == 's') {
	  if (p[4] == 'r')
	       pnum = &io->syscr;
	  else
	       pnum = &io->syscw;

	  p += STATIC_LEN("syscX: ");
     } else
	  abort();

     sscanf(p, "%llu", pnum);
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
	  sprintf(str, "%-8lld", n);
}

int main(int argc, char *argv[])
{
     int pid;
     int interval;
     proc_io_t io[2];
     int io_pos = 0;
     char path[PATH_MAX];
     char line[LINE_MAX];
     int have_prev_io = 0;
     int n;

     progname = basename(argv[0]);

     if (argc != 3)
	  goto err;

     pid = atoi(argv[1]);
     interval = atoi(argv[2]);
     if (!pid || !interval)
	  goto err;

     n = snprintf(path, sizeof path, "/proc/%d/io", pid);
     if (sizeof path == n)
	  goto err; // no space for \0

     while (1) {
	  char *p;
	  proc_io_t *iop, *ioc;

	  FILE *fp = fopen(path, "r");
	  if (!fp) {
	       fprintf(stderr, "%s: unable to open %s: %m\n", progname, path);
	       exit(EXIT_FAILURE);
	  }

	  ioc = &io[io_pos];
	  while (p = fgets(line, sizeof line, fp)) {
	       io_parse_line(line, sizeof line, ioc);
	  }
	  io_pos = (io_pos + 1) % 2;
	  iop = &io[io_pos];

	  if (have_prev_io) {
	       uint64_t rbytes = ioc->read_bytes - iop->read_bytes;
	       uint64_t wbytes = ioc->write_bytes - iop->write_bytes;
	       uint64_t rchar = ioc->rchar - iop->rchar;
	       uint64_t wchar = ioc->wchar - iop->wchar;
	       uint64_t syscr = ioc->syscr - iop->syscr;
	       uint64_t syscw = ioc->syscw - iop->syscw;
	       uint64_t cancelled = ioc->cancelled_write_bytes - iop->cancelled_write_bytes;
	       static char str_rbytes[16];
	       static char str_wbytes[16];
	       static char str_rchar[16];
	       static char str_wchar[16];
	       static char str_syscr[16];
	       static char str_syscw[16];
	       static char str_cancelled[16];

	       annotate_num(rbytes, str_rbytes);
	       annotate_num(wbytes, str_wbytes);
	       annotate_num(rchar, str_rchar);
	       annotate_num(wchar, str_wchar);
	       annotate_num(syscr, str_syscr);
	       annotate_num(syscw, str_syscw);
	       annotate_num(cancelled, str_cancelled);

	       printf("%-8s %-8s %-8s %-8s %-8s %-8s %-8s\n",
		      str_rbytes,
		      str_wbytes,
		      str_rchar,
		      str_wchar,
		      str_syscr,
		      str_syscw,
		      str_cancelled);

	  } else {
	       printf("rbytes   wbytes   rchar    wchar    syscr    syscw    cancelled-wbytes\n");
	       have_prev_io = 1;
	  }

	  sleep(interval);
     }

     return 0;

err:
     usage();
     exit(EXIT_FAILURE);
}
