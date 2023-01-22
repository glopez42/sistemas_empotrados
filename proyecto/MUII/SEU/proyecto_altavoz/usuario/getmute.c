#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define SPKR_SET_MUTE_STATE _IOW('9', 1, int)
#define SPKR_GET_MUTE_STATE _IOR('9', 1, int)

int main(int argc, char *argv[]) {
	int sd;
	
	if ((sd = open("/dev/int_spkr", O_RDONLY)) <0) {
		perror("open");
		return 1;
	}
#ifndef SPKR_GET_MUTE_STATE
#error Debe definir el ioctl para la operación get mute
#else
	int param;
	if (ioctl(sd, SPKR_GET_MUTE_STATE, &param) <0) {
		perror("ioctl");
		return 1;
	}
	printf("%s\n", param ? "mute on" : "mute off");
#endif
	return 0;
}

