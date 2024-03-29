#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/i8253.h>
#include <asm/io.h>

void set_spkr_frequency(unsigned int frequency)
{
	uint32_t cont;
	unsigned long flags;

	cont = PIT_TICK_RATE / frequency;

	raw_spin_lock_irqsave(&i8253_lock, flags);

	outb(0xb6, 0x43);				  // timer 2 -> square wave generator
	outb((uint8_t)(cont), 0x42);	  // parte baja del contador
	outb((uint8_t)(cont >> 8), 0x42); // parte alta del contador

	raw_spin_unlock_irqrestore(&i8253_lock, flags);

	printk(KERN_INFO "spkr set frequency: %d\n", frequency);
}

void spkr_on(void)
{
	unsigned long flags;
	raw_spin_lock_irqsave(&i8253_lock, flags);

	// hay que poner un 1 en los bits de menor peso del registro 0x61 (reg OR 3)
	outb(inb_p(0x61) | 3, 0x61); // se pone inb_p para no modificar el resto de bits del registro

	raw_spin_unlock_irqrestore(&i8253_lock, flags);

	printk(KERN_INFO "spkr ON\n");
}

void spkr_off(void)
{
	unsigned long flags;
	raw_spin_lock_irqsave(&i8253_lock, flags);

	// hay que poner a 0 los dos bits inferiores del registro 0x61
	outb(inb_p(0x61) & 0xFC, 0x61); // se pone inb_p para no modificar el resto de bits del registro

	raw_spin_unlock_irqrestore(&i8253_lock, flags);

	printk(KERN_INFO "spkr OFF\n");
}
