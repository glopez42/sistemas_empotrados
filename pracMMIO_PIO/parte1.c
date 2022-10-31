#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <grp.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "bits.h"

// registros Generic Host Control
struct ahci_info
{
	uint32_t hostCapabilites;
	uint32_t globalHostControl;
	uint32_t interruptStatus;
	uint32_t portsImplemented;
	uint32_t version;
	uint32_t commandCompletionCoalescing[2];
	uint32_t enclosureManagement[2];
	uint32_t hostCapabilitesExtended;
	uint32_t biosHandoff;
};

int main(int argc, char *argv[])
{
	int fd, tam = 4096;
	unsigned long dir;

	struct ahci_info volatile *ahci; // volatile elimina optimizaciones de compilador

	if (argc != 2) {
		printf("ERROR: Indica como parámetro la dirección física inicial en hexadecimal.\n");
		return 1;
	}

	if ((fd = open("/dev/mem", O_RDONLY | __O_DSYNC)) < 0)
	{ // O_DSYNC: accesos no usan cache
		perror("open");
		return 1;
	}

	// Obtenemos direccion fisica inicial en hexadecimal
	dir = strtoul(argv[1], NULL, 16);

	// obtiene rango dir. lógicas usuario asociadas a dir. físicas dispositivo
	if ((ahci = mmap(NULL, tam, PROT_READ, MAP_SHARED, fd, dir)) == MAP_FAILED)
	{
		perror("mmap");
		return 1;
	}

	printf("Versión %x.%x\n", bits_extrae(ahci->version, 16, 8), bits_extrae(ahci->version, 8, 8));
	printf("Versión %x\n", ahci->version);

	close(fd);
	munmap((void *)ahci, tam);
	return 0;
}