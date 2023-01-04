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
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#define DISP_NAME "int_spkr"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Guillermo Lopez Garcia");

extern void set_spkr_frequency(unsigned int frequency);
extern void spkr_on(void);
extern void spkr_off(void);

// variables de dispositivo
static int minor = 0;
static int major;
static dev_t id_disp;
static struct cdev disp;
static struct class *class;

// sincronización
struct mutex open_mutex;
struct mutex write_mutex;
static int open_count = 0;
static int is_blocked;
spinlock_t lock_timer_function;
wait_queue_head_t lista_bloq;

// temporizador
static struct timer_list timer;



module_param(minor, int, S_IRUGO);

// **** Rutina que se activa al finalizar el temporizador ****
void timer_function(struct timer_list *t)
{
    spin_lock_bh(&lock_timer_function);
    wake_up_interruptible(&lista_bloq);
    is_blocked = 1;
    spin_unlock_bh(&lock_timer_function);
    printk("temporizador");
}

// **** Operaciones de apertura, cierre y escritura ****
static int spkr_open(struct inode *inode, struct file *filp)
{

    // si se abre en modo escritura
    if (filp->f_mode & FMODE_WRITE)
    {
        mutex_lock(&open_mutex);
        if (open_count > 0)
        {
            printk(KERN_INFO "BUSY spkr\n");
            mutex_unlock(&open_mutex);
            return -EBUSY;
        }
        open_count += 1;
        mutex_unlock(&open_mutex);
    }

    printk(KERN_INFO "OPEN spkr\n");
    return 0;
}

static int spkr_release(struct inode *inode, struct file *filp)
{
    // si se cierra estando modo escritura
    if (filp->f_mode & FMODE_WRITE)
    {
        mutex_lock(&open_mutex);
        open_count -= 1;
        mutex_unlock(&open_mutex);
    }

    printk(KERN_INFO "RELEASE spkr\n");
    return 0;
}

static ssize_t spkr_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    u_int16_t frequency, time, total;

    printk(KERN_INFO "WRITE spkr\n");

    if (mutex_lock_interruptible(&write_mutex)) return -ERESTARTSYS;

    total = count;
    while (count >= 4)
    {
        // extraemos sonido y frecuencia
        if (get_user(time, (u_int16_t __user *) buf) || get_user(frequency, (u_int16_t __user *) (buf + 2)))
        {
            mutex_unlock(&write_mutex);
            return -EFAULT;
        }

        printk(KERN_INFO "> freq: %d\n> time: %d\n", frequency, time);

        // programamos frecuencia en dispositivo
        set_spkr_frequency(frequency);
        spkr_on();

        // arrancamos temporizador
        timer_setup(&timer, timer_function, 0);
        mod_timer(&timer, jiffies + msecs_to_jiffies(time));

        // bloqueamos proceso
        is_blocked = 0;
        if (wait_event_interruptible(lista_bloq, is_blocked)) {
            mutex_unlock(&write_mutex);
            return -ERESTARTSYS;
        }

        spkr_off();
       
        // actualizamos valores
        count -= 4; // bytes leídos
        buf += 4;   // puntero del buffer de usuario
    }

    mutex_unlock(&write_mutex);
    // si count == 0 se han leído todos los datos
    return total - count;
}

static const struct file_operations spkr_fops = {
    .owner = THIS_MODULE,
    .open = spkr_open,
    .release = spkr_release,
    .write = spkr_write};

// **** Rutina de inicialización del módulo ****
static int init_spkr(void)
{
    // iniciamos estructuras de sincronización
    mutex_init(&open_mutex);
    mutex_init(&write_mutex);
    init_waitqueue_head(&lista_bloq);
    spin_lock_init(&lock_timer_function);

    // Reserva de números major y minor
    if (alloc_chrdev_region(&id_disp, minor, 1, DISP_NAME))
    {
        printk(KERN_ERR "Error en reserva de números major y minor del dispositivo.");
        return -1;
    }

    // Mostramos major seleccionado
    major = MAJOR(id_disp);
    printk(KERN_INFO "MAJOR: %d\n", major);
    printk(KERN_INFO "MINOR: %d\n", minor);

    // Inicialización de parámetros de dispositivo y funciones
    cdev_init(&disp, &spkr_fops);

    // Asociar id dispositivo con struct de dispositivo
    if (cdev_add(&disp, id_disp, 1) == -1)
    {
        cdev_del(&disp);
        printk(KERN_ERR "Error al añadir struct cdev.");
        return -1;
    }

    // Alta de un dispositivo para las aplicaciones
    class = class_create(THIS_MODULE, "speaker");
    device_create(class, NULL, id_disp, NULL, DISP_NAME);

    printk(KERN_INFO "Device created\n");

    return 0;
}

// **** Rutina de terminación del módulo ****
static void exit_spkr(void)
{
    // Eliminación de dispositivo
    cdev_del(&disp);
    // Dar de baja en sysfs
    device_destroy(class, id_disp);
    class_destroy(class);
    // Liberación de major y minor
    unregister_chrdev_region(id_disp, 1);

    printk(KERN_INFO "Device destroyed\n");
}

module_init(init_spkr);
module_exit(exit_spkr);