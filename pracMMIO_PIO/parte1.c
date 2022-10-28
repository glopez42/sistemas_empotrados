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

#define LOCAL_APIC_BASE_ADDRESS 0xfee00000

struct ahci_info
{
	uint32_t reservado[8];
	uint32_t ID;
	uint32_t relleno[3];
	uint32_t version; // nº versión: 8 bits de menor peso
					  // ... no usados por el programa
};

int main(int argc, char *argv[])
{
	int fd, tam = 4096;
	unsigned long dir;

	struct ahci_info volatile *ahci; // volatile elimina optimizaciones de compilador

	if ((fd = open("/dev/mem", O_RDONLY | __O_DSYNC)) < 0)
	{ // O_DSYNC: accesos no usan cache
		perror("open");
		return 1;
	}

	// Obtenemos direccion fisica inicial en hexadecimal
	dir = strtoul(argv[1], NULL, 16);
	printf("Acceso a dirección física: %lu\n", dir);

	// obtiene rango dir. lógicas usuario asociadas a dir. físicas dispositivo
	if ((ahci = mmap(NULL, tam, PROT_READ, MAP_SHARED, fd, LOCAL_APIC_BASE_ADDRESS)) == MAP_FAILED)
	{
		perror("mmap");
		return 1;
	}

	printf("Acceso dir física %x usando %p\n", dir, ahci);
	printf("Local APIC ID: %d Versión %d\n", ahci->ID, bits_extrae(ahci->version, 0, 8));
	close(fd);
	munmap((void *)ahci, tam);
	return 0;
}