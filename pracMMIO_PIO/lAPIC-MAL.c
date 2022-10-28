/* Ejemplo de MMIO: acceso a registros del Local-APIC ID y versión
 * 	https://wiki.osdev.org/APIC#Local_APIC_registers
 * 	https://www.intel.la/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf
 * 		Tabla 10.1 y secciones 10.4.6 y 10.4.8 */

// NO FUNCIONA: En sistemas con MMU no se puede acceder directamente a dir. físicas
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
// Dirección base por defecto de registros MMIO de Local-APIC (/proc/iomem)
#define LOCAL_APIC_BASE_ADDRESS (void *)0xfee00000
struct lapic_info
{
	uint32_t reservado[8];
	uint32_t ID;
	uint32_t relleno[3];
	uint32_t version; // nº versión: 8 bits de menor peso
					  // ... no usados por el programa
};
int main(int argc, char *argv[])
{
	// volatile elimina optimizaciones de compilador
	struct lapic_info volatile *lapic = LOCAL_APIC_BASE_ADDRESS;
	printf("Local APIC ID: %d Versión %d\n", lapic->ID, lapic->version & 0xFF);
	return 0;
}
