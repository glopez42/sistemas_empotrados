// obtiene fabricante y modelo dispo. PCI bus 0, slot 0 y función 0; http://wiki.osdev.org/PCI
#include <sys/io.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define CONFIG_DIR 0xCF8
/* en CONFIG_DIR se especifica dir. a leer/escribir: bus|slot|func|registro con el formato:
|    31    |(30-24) |(23-16)|(15-11)  |(10-8)     |(7-2)      |(1-0)|
|Enable Bit|Reserved|BusNum |slotNum|FunctionNum|RegisterNum| 0 0 |    */

#define CONFIG_DAT 0xCFC
// Si se lee de CONFIG_DAT se obtiene contenido de registro especificado en CONFIG_DIR
// Si se escribe en CONFIG_DAT se modifica contenido de registro especificado en CONFIG_DIR

#define BASE_ADDRESS 0x80000000
// dirección base con el bit enable activado

uint32_t read_pci_word(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset){
	uint32_t address = BASE_ADDRESS | (bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC);
	printf("%x\n", address);
	outl(address, CONFIG_DIR);
	return inl(CONFIG_DAT);
}

int main(int argc, char *argv[])
{
	uint8_t bus, slot, function, class, subclass, interface,
	 actual_class, actual_subclass, actual_interface, header_type;
	uint32_t data;

	if (argc != 4) {
		printf("Uso: parte2 clase subclase interfaz\n");
		return 1;
	}

	class = strtol(argv[1], NULL, 16);
	subclass = strtol(argv[2], NULL, 16);
	interface = strtol(argv[3], NULL, 16);

	if (ioperm(CONFIG_DIR, 8, 1) < 0)
	{ // permiso para acceso a los 2 puertos modo usuario
		perror("ioperm");
		return 1;
	}

	// uint32_t dir, dat;
	// int vend, prod;

	// // al ser bus 0, slot 0, función 0 y registro 0 basta con especificar el "enable" bit
	// dir = (uint32_t)0x80000000;

	// outl(dir, CONFIG_DIR);
	// dat = inl(CONFIG_DAT);

	// if (dat == 0xFFFFFFFF)
	// {
	// 	fprintf(stderr, "no existe ese dispositivo\n");
	// 	return 1;
	// }

	// vend = dat & 0x0000FFFF;
	// prod = dat >> 16; // extrae vendedor y producto

	// printf("Bus 0 Slot 0 Función 0: ID Vendedor %x ID Producto %x\n", vend, prod);

	for ( bus = 0; bus < 256; bus++)
	{
		for (slot = 0; slot < 32; slot++)
		{
			for (function = 0; function < 8; function++)
			{
				// leemos del pci para ver si coincide con la clase, subclase e interfaz seleccionada
				data = read_pci_word(bus, slot, function, 0);
				printf("vendor_id: %x\n", data & 0xFFFF);
				data = read_pci_word(bus, slot, function, 8);
				printf("%x\n", data);
				actual_class = (data >> 24) & 0xFF; // byte más significativo
				actual_subclass = (data >> 16) & 0xFF;
				actual_interface = (data >> 8) & 0xFF;
				data = read_pci_word(bus, slot, function, 0xC);
				printf("%x\n", data);
				header_type = (data >> 16) & 0xFF;
				printf("b %x\n", header_type);
				if (actual_class == class && actual_subclass == subclass && actual_interface == interface){
					printf("%d %d %d\n", actual_class, actual_subclass, actual_interface);
					return 0;
				}
				printf("%x %x %x\n", actual_class, actual_subclass, actual_interface);
				printf("%x %x %x\n", class, subclass, interface);

			}
			return 0;
		}
	}
	
	return 0;
}
