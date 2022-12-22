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

uint32_t read_pci_word(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
	uint32_t address = BASE_ADDRESS | (bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC);
	outl(address, CONFIG_DIR);
	return inl(CONFIG_DAT);
}

uint32_t print_specs(uint8_t bus, uint8_t slot, uint8_t function)
{
	uint32_t data, class, subclass, interface;

	printf("Bus: 0x%x Slot: 0x%x Func: 0x%x ", bus, slot, function);

	data = read_pci_word(bus, slot, function, 0);
	printf("Vendedor: %#x ", data & 0xFFFF);
	printf("Producto: %#x ", (data >> 16) & 0xFFFF);

	data = read_pci_word(bus, slot, function, 8);
	class = (data >> 24) & 0xFF; // byte más significativo
	subclass = (data >> 16) & 0xFF;
	interface = (data >> 8) & 0xFF;
	printf("Clase: 0x%x Subclase: 0x%x Interface: 0x%x ", class, subclass, interface);

	data = read_pci_word(bus, slot, function, 0x10);
	printf("BAR0: %#x ", data);
	data = read_pci_word(bus, slot, function, 0x14);
	printf("BAR1: %#x ", data);
	data = read_pci_word(bus, slot, function, 0x18);
	printf("BAR2: %#x ", data);
	data = read_pci_word(bus, slot, function, 0x1c);
	printf("BAR3: %#x ", data);
	data = read_pci_word(bus, slot, function, 0x20);
	printf("BAR4: %#x ", data);
	data = read_pci_word(bus, slot, function, 0x24);
	printf("BAR5: %#x ", data);

	printf("\n");
}

int main(int argc, char *argv[])
{
	uint8_t class, subclass, interface,
		actual_class, actual_subclass, actual_interface, header_type;
	uint32_t data, bus, slot, function;

	if (argc != 4)
	{
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

	// data = read_pci_word(bus, slot, function, 0xC);
	// header_type = (data >> 16) & 0xFF;

	for (bus = 0; bus < 256; bus++)
	{
		for (slot = 0; slot < 32; slot++)
		{
			for (function = 0; function < 8; function++)
			{
				// leemos del pci para ver si coincide con la clase, subclase e interfaz seleccionada
				data = read_pci_word(bus, slot, function, 8);
				actual_class = (data >> 24) & 0xFF; // byte más significativo
				actual_subclass = (data >> 16) & 0xFF;
				actual_interface = (data >> 8) & 0xFF;
				if (actual_class == class && actual_subclass == subclass && actual_interface == interface)
				{
					print_specs(bus, slot, function);
				}

				// comprobamos si es multifunción
				data = read_pci_word(bus, slot, function, 0xC);
				header_type = (data >> 16) & 0xFF;
				// para ello miramos si el bit 7 tiene el valor a 1
				if (!(header_type & (1 << 7)))
				{
					// si no lo tiene a 1 entonces no hay más funciones
					break;
				}
			}
		}
	}

	return 0;
}
