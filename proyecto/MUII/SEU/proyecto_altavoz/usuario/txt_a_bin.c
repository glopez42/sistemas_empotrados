#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	uint16_t v1, v2;
	while (scanf("%hd%hd", &v1, &v2)==2) {
		write(1, &v1, sizeof(v1));
		write(1, &v2, sizeof(v2));
	}
	return 0;
}
