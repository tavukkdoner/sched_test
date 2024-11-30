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


#define TIMING_BUF_MB           1
#define TIMING_BUF_BYTES        (TIMING_BUF_MB * 1024 * 1024)

unsigned dev_size_mb(int fd)
{
	//struct region_info_user mtdInfo;
	union {
		unsigned long long blksize64;
		unsigned blksize32;
	} u;

	//if(ioctl(fd, MEMGETREGIONINFO, &mtdInfo) == 0) {
	//	/* It is flash memory */
	//	return mtdInfo.erasesize / (2 * 1024);
	//}
	if (0 == ioctl(fd, BLKGETSIZE64, &u.blksize64)) { // bytes
		u.blksize64 /= (1024 * 1024);
	} else {
		if (ioctl(fd, BLKGETSIZE, &u.blksize32) != 0) { // sectors
			fprintf(stderr, "ioctl BLKGETSIZE failed\n");
		}
		u.blksize64 = u.blksize32 / (2 * 1024);
	}
	if (u.blksize64 > UINT_MAX)
		return UINT_MAX;
	return u.blksize64;
}

ssize_t safe_read(int fd, void *buf, size_t count)
{
	ssize_t n;

	do {
		n = read(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

ssize_t full_read(int fd, void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len) {
		cc = safe_read(fd, buf, len);

		if (cc < 0) {
			if (total) {
				/* we already have some! */
				/* user can do another read to know the error code */
				return total;
			}
			return cc; /* read() returns -1 on failure. */
		}
		if (cc == 0)
			break;
		buf = ((char *)buf) + cc;
		total += cc;
		len -= cc;
	}

	return total;
}

/* Die with an error message if we can't read the entire buffer. */
void xread(int fd, void *buf, size_t count)
{
	if (count) {
		ssize_t size = full_read(fd, buf, count);
		if ((size_t)size != count) {
			fprintf(stderr, "short read\n");
			exit(1);
		}
	}
}

void read_big_block(int fd, char *buf)
{
	int i;

	xread(fd, buf, TIMING_BUF_BYTES);
	/* access all sectors of buf to ensure the read fully completed */
	for (i = 0; i < TIMING_BUF_BYTES; i += 512)
		buf[i] &= 1;
}

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

// Die with an error message if we can't lseek to the right spot.
off_t xlseek(int fd, off_t offset, int whence)
{
	off_t off = lseek(fd, offset, whence);
	if (off == (off_t)-1) {
		if (whence == SEEK_SET) {
			fprintf(stderr, "lseek(%llu)", offset);
		}
		fprintf(stderr, "lseek failed");
		exit(1);
	}
	return off;
}

void seek_to_zero(int fd)
{
	xlseek(fd, (off_t) 0, SEEK_SET);
}

int main(int argc, char **argv)
{
	int fd;
	unsigned max_iterations, iterations;
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

	fd = open(argv[1], O_RDONLY | O_NONBLOCK);
	if (fd == -1) {
		fprintf(stderr, "open failed\n");
	}
	
	//printf("\n%s:\n", argv[1]);
	
	sync();
	sleep(1);
	fflush(stdout);

	/* Now do the timing */
	iterations = 0;
	elapsed2 = 3000000;
	/* Don't want to read past the end! */
	max_iterations = dev_size_mb(fd) / TIMING_BUF_MB;
	start = monotonic_us();
	do {
		read_big_block(fd, buf);
		elapsed = (unsigned)monotonic_us() - start;
		//printf("elapsed %lu\n", elapsed);
		++iterations;
	} while (elapsed < elapsed2 && iterations < max_iterations);
	total_MB = iterations * TIMING_BUF_MB;
	print_timing(total_MB, elapsed);
	munlock(buf, TIMING_BUF_BYTES);
	free(buf);
	return 0;
}
