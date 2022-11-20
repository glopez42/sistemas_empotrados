// obtiene fabricante y modelo dispo. PCI bus 0, slot 0 y función 0; http://wiki.osdev.org/PCI
#include <sys/io.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define CONFIG_DIR 0xCF8
/* en CONFIG_DIR se especifica dir. a leer/escribir: bus|slot|func|registro con el formato:
|    31    |(30-24) |(23-16)|(15-11)  |(10-8)     |(7-2)      |(1-0)|
|Enable Bit|Reserved|BusNum |DeviceNum|FunctionNum|RegisterNum| 0 0 |    */

#define CONFIG_DAT 0xCFC
// Si se lee de CONFIG_DAT se obtiene contenido de registro especificado en CONFIG_DIR
// Si se escribe en CONFIG_DAT se modifica contenido de registro especificado en CONFIG_DIR

int main(int argc, char *argv[])
{
	if (ioperm(CONFIG_DIR, 8, 1) < 0)
	{ // permiso para acceso a los 2 puertos modo usuario
		perror("ioperm");
		return 1;
	}
	uint32_t dir, dat;
	int vend, prod;

	// al ser bus 0, slot 0, función 0 y registro 0 basta con especificar el "enable" bit
	dir = (uint32_t)0x80000000;

	outl(dir, CONFIG_DIR);
	dat = inl(CONFIG_DAT);

	if (dat == 0xFFFFFFFF)
	{
		fprintf(stderr, "no existe ese dispositivo\n");
		return 1;
	}

	vend = dat & 0x0000FFFF;
	prod = dat >> 16; // extrae vendedor y producto

	printf("Bus 0 Slot 0 Función 0: ID Vendedor %x ID Producto %x\n", vend, prod);
	return 0;
}
