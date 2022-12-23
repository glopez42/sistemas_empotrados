#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	uint16_t v1, v2;
	while ((read(0, &v1, sizeof(v1))>0) && (read(0, &v2, sizeof(v2))>0))
		printf("%d %d\n", v1, v2);
	return 0;
}
