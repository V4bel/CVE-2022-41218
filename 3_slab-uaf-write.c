#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <malloc.h>
#include <poll.h>
#include <pty.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <linux/userfaultfd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <linux/netlink.h>
#include <stddef.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <linux/bpf.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#include <linux/dvb/dmx.h>


#define CPU_0 1
#define CPU_1 2
#define CPU_2 3
#define CPU_3 4

#define die() do { \
	fprintf(stderr, "died in %s: %u\n", __func__, __LINE__); \
	exit(EXIT_FAILURE); \
} while (0)


int fd;
int set1 = 0;
int set2 = 0;
int set3 = 0;

void set_affinity(unsigned long mask) {
	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), (cpu_set_t *)&mask) < 0) {
		perror("pthread_setaffinity_np");
	}
	return;
}

void *dvb_wait_queue(void) {
	int fd;
	int ret;

	//set_affinity(CPU_2);

	fd = open("/dev/dvb/adapter0/dvr0", O_RDONLY);
	if (fd > 0) {
		printf("[step 1] dvr0 open() : %d  pid : %ld\n", fd, syscall(SYS_gettid));
	} else {
		perror("/dev/dvb/adapter0/dvr0 open() failed");
		die();
	}
	set1 = 1;


	while(!set2);
	close(fd);
	set3 = 1;
	printf("[step 4] dvr0 close()  pid : %ld\n", syscall(SYS_gettid));

	sleep(5);
}

void *demux_close(void) {
	int ret;
	unsigned char tmp;
	char input[2];
	int fd;

	//set_affinity(CPU_1);

	while(!set1);
	printf("Disconnect now (After disconnecting, type enter) : \n");
	read(0, input, 1);
	printf("[step 2] disconnect dvb usb\n");


	fd = open("/dev/dvb/adapter0/demux0", O_RDWR);
	if (fd > 0) {
		printf("[step 3] demux0 open() : %d  pid : %ld\n", fd, syscall(SYS_gettid));

	} else {
		perror("/dev/dvb/adapter0/demux0 open() failed");
		die();
	}
	set2 = 1;


	while(!set3);
	usleep(65);
	close(fd);
	printf("[step 5] demux0 close()  pid : %ld\n", syscall(SYS_gettid));

	sleep(5);
}

int main() {
	pthread_t pf_hdr;
	int p1, p2;
	int status1, status2;
	pthread_t hdr1, hdr2;
	int ret;

	//set_affinity(CPU_0);

	p1 = pthread_create(&hdr1, NULL, dvb_wait_queue, (void *)NULL);
	if (p1 != 0) {
		perror("pthread_create 1");
		die();
	}

	p2 = pthread_create(&hdr2, NULL, demux_close, (void *)NULL);
	if (p2 != 0) {
		perror("pthread_create 2");
		die();
	}

	pthread_join(hdr1, (void **)&status1);
	pthread_join(hdr2, (void **)&status2);

	return 0;
}
