/* 
 *  read_test (c) farmatito@tiscali.it 2009
 * 
 *  This program performs average calculation of block device reads.
 *  It contains code from busybox and hdparm.
 *
 * Licensed under GPLV2.  NO WARRANTY. USE AT YOUR OWN RISK.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
//#include <inttypes.h>
//#include <mtd/mtd-abi.h>


#define TIMING_BUF_MB           10
#define TIMING_BUF_BYTES        (TIMING_BUF_MB * 1024 * 1024)


unsigned long long monotonic_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

void print_timing(unsigned m, unsigned elapsed_us)
{
	unsigned sec = elapsed_us / 1000000;
	unsigned hs = (elapsed_us % 1000000) / 10000;

	printf("%5u MB in %u.%02u seconds = %u kB/s\n",
		m, sec, hs,
		/* "| 1" prevents div-by-0 */
		(unsigned) ((unsigned long long)m * (1024 * 1000000) / (elapsed_us | 1))
		// ~= (m * 1024) / (elapsed_us / 1000000)
		// = kb / elapsed_sec
	);
}

ssize_t safe_write(int fd, const void *buf, size_t count)
{
	ssize_t n;

	do {
		n = write(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

ssize_t full_write(int fd, const void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len) {
		cc = safe_write(fd, buf, len);

		if (cc < 0) {
			if (total) {
				/* we already wrote some! */
				/* user can do another write to know the error code */
				return total;
			}
			return cc;	/* write() returns -1 on failure. */
		}

		total += cc;
		buf = ((const char *)buf) + cc;
		len -= cc;
	}

	return total;
}

// Die with an error message if we can't write the entire buffer.
void xwrite(int fd, const void *buf, size_t count)
{
	if (count) {
		ssize_t size = full_write(fd, buf, count);
		if ((size_t)size != count) {
			fprintf(stderr, "short write\n");
			exit(1);
		}
	}
}

int main(int argc, char **argv)
{
	FILE *test_file;
	unsigned iterations;
	unsigned start; /* doesn't need to be long long */
	unsigned elapsed, elapsed2;
	unsigned total_MB;
	char *buf = malloc(TIMING_BUF_BYTES);
	if (!buf) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	if (mlock(buf, TIMING_BUF_BYTES) == -1) {
		fprintf(stderr, "mlock failed\n");
		exit(1);
	}
	if (argc != 2) {
		fprintf(stderr, "missing arg\n");
		exit(1);
	}
	
	//printf("\n%s:\n", argv[1]);
	
	sync();
	sleep(1);
	fflush(stdout);

	/* Now do the timing */
	iterations = 0;
	elapsed2 = 3000000;
	/* Don't want to read past the end! */
	start = monotonic_us();
	do {
		test_file = fopen(argv[1], "w+");
		if (!test_file) {
			fprintf(stderr, "fopen '%s' failed\n", argv[1]);
		}
		xwrite(fileno(test_file), buf, TIMING_BUF_BYTES);
		fclose(test_file);
		elapsed = (unsigned)monotonic_us() - start;
		++iterations;
	} while (elapsed < elapsed2);
	total_MB = iterations * TIMING_BUF_MB;
	print_timing(total_MB, elapsed);
	munlock(buf, TIMING_BUF_BYTES);
	free(buf);
	return 0;
}
