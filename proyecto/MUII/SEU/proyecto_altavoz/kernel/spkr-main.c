#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kfifo.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>

#define DISP_NAME "int_spkr"

MODULE_LICENSE("GPL");

extern void set_spkr_frequency(unsigned int frequency);
extern void spkr_on(void);
extern void spkr_off(void);

// variables de dispositivo
static int minor = 0;
static dev_t id_disp;
static struct cdev disp;

module_param(minor, int, S_IRUGO);

// **** Operaciones de apertura, cierre y escritura ****
static int spkr_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "OPEN spkr\n");
    return 0;
}

static int spkr_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "RELEASE spkr\n");
    return 0;
}

static int spkr_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    printk(KERN_INFO "WRITE spkr\n");
    return count;
}

static struct file_operations spkr_fops = {
    .owner = THIS_MODULE,
    .open = spkr_open,
    .release = spkr_release,
    .write = spkr_write
};


// **** Rutina de inicialización del módulo ****
static void init_spkr()
{

    // Reserva de números major y minor
    if (alloc_chrdev_region(&id_disp, minor, 1, DISP_NAME))
    {
        printk(KERN_ERR "Error en reserva de números major y minor del dispositivo.");
        return -1;
    }

    // Inicialización de parámetros de dispositivo y funciones
    cdev_init(&disp, &spkr_fops);

    // Asociar id dispositivo con struct de dispositivo
    if (cdev_add(&disp, id_disp, 1))
    {
        printk(KERN_ERR "Error al añadir struct cdev.");
        return -1;
    }
}

// **** Rutina de terminación del módulo ****
static void exit_spkr()
{
    // Liberación de major y minor
    unregister_chrdev_region(id_disp, 1);
    // Eliminación de dispositivo
    cdev_del(&disp);
}

module_init(init_spkr);
module_exit(exit_spkr);