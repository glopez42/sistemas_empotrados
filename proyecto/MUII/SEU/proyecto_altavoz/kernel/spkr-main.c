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

#define SPKR_SET_MUTE_STATE _IOW('9', 1, int)
#define SPKR_GET_MUTE_STATE _IOR('9', 1, int)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Guillermo Lopez Garcia");

extern void set_spkr_frequency(unsigned int frequency);
extern void spkr_on(void);
extern void spkr_off(void);
void programar_sonido(u_int16_t frequency, u_int16_t time);

// variables de dispositivo
static int spkr_minor = 0;
static int spkr_major;
static int buffersize = 0;
static dev_t id_disp;
static struct cdev disp;
static struct class *class;

// sincronización
struct mutex open_mutex;
struct mutex write_mutex;
static int open_count = 0;
static int is_blocked;
spinlock_t lock_timer_function;
spinlock_t lock_is_mute_variable;
wait_queue_head_t lista_bloq;
static int spkr_is_not_muted = 1;

// temporizador
static struct timer_list timer;

// FIFO
static struct kfifo cola;

// sonidos incompletos
static char incomplete_sound[4];
static int incomplete_count = 0;
static int write_incomplete = 0;
static int bytes_read = 0;

module_param(spkr_minor, int, S_IRUGO);
module_param(buffersize, int, S_IRUGO);

// **** Rutina que se activa al finalizar el temporizador en una escritura SIN buffer ****
void timer_function(struct timer_list *t)
{
    spin_lock_bh(&lock_timer_function);
    is_blocked = 1;
    wake_up_interruptible(&lista_bloq);
    spin_unlock_bh(&lock_timer_function);
}

// **** Rutina que se activa al finalizar el temporizador en una escritura CON buffer ****
void timer_function_buf(struct timer_list *t)
{
    u_int16_t frequency, time;

    spin_lock_bh(&lock_timer_function);

    // apagamos el beeper si estaba sonando
    spkr_off();

    // si la cola está vacía se desbloquea proceso
    if (kfifo_is_empty(&cola))
    {
        is_blocked = 1;
        wake_up_interruptible(&lista_bloq);
    }
    else
    {
        // si no se sacan de la cola el tiempo y la frecuencia
        if (kfifo_out(&cola, &time, sizeof(time)) == 0 || kfifo_out(&cola, &frequency, sizeof(frequency)) == 0)
        {
            printk(KERN_ERR "Error al sacar elementos de la kfifo.\n");
            is_blocked = 1;
            wake_up_interruptible(&lista_bloq);
            spin_unlock_bh(&lock_timer_function);
            return;
        }
        programar_sonido(frequency, time);
    }
    spin_unlock_bh(&lock_timer_function);
}

// **** Rutina para programar un sonido en el altavoz ****
void programar_sonido(u_int16_t frequency, u_int16_t time)
{
    printk(KERN_INFO "spkr set timer: %d\n", time);
    // programamos frecuencia en dispositivo si el spkr no está muteado
    spin_lock_bh(&lock_is_mute_variable);
    if (frequency != 0 && spkr_is_not_muted)
    {
        set_spkr_frequency(frequency);
        spkr_on();
    }
    spin_unlock_bh(&lock_is_mute_variable);

    // arrancamos temporizador dependiendo de si hay buffer de escritura o no
    if (buffersize)
        timer_setup(&timer, timer_function_buf, 0);
    else
        timer_setup(&timer, timer_function, 0);

    mod_timer(&timer, jiffies + msecs_to_jiffies(time));
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
    u_int16_t frequency, time, total, data_to_copy;
    unsigned int copied;

    printk(KERN_INFO "WRITE spkr\n");

    if (mutex_lock_interruptible(&write_mutex))
    {
        return -ERESTARTSYS;
    }

    bytes_read = 0;
    total = count;

    // si ha habido anteriormente una escritura incompleta
    if (incomplete_count > 0)
    {
        // guardamos los bytes incompletos
        if (copy_from_user(incomplete_sound + incomplete_count, buf, 4 - incomplete_count))
        {
            mutex_unlock(&write_mutex);
            return -EFAULT;
        }

        // marcamos que hay una escritura completa que estaba incompleta
        write_incomplete = 1;

        // guardamos el número de bytes incompletos que se han leído
        bytes_read = 4 - incomplete_count;
        incomplete_count = 0;
    }

    // si hay al menos 4 bytes que escribir o una escritura anterior incompleta
    while (count >= 4 || write_incomplete)
    {
        // ESCRITURA CON BUFFER
        if (buffersize != 0)
        {
            // se copian los bytes que quepan o los que queden
            data_to_copy = (count > buffersize) ? buffersize : count;

            // si ha habido anteriormente una escritura incompleta se programa ese sonido
            if (write_incomplete)
            {
                // si es así, leemos los datos guardados previamente
                write_incomplete = 0;
                time = *(u_int16_t *)incomplete_sound;
                frequency = *(u_int16_t *)(incomplete_sound + 2);

                // copiamos el resto de los datos del buffer de usuario a la kfifo
                buf += bytes_read;
                if (count >= 4 && kfifo_from_user(&cola, buf, data_to_copy, &copied))
                {
                    mutex_unlock(&write_mutex);
                    return -EFAULT;
                }
            }
            // si no se copian los datos en la cola y se sacan los 4 primeros bytes
            else
            {
                if (kfifo_from_user(&cola, buf, data_to_copy, &copied))
                {
                    mutex_unlock(&write_mutex);
                    return -EFAULT;
                }

                if (kfifo_out(&cola, &time, sizeof(u_int16_t)) == 0 || kfifo_out(&cola, &frequency, sizeof(u_int16_t)) == 0)
                {
                    mutex_unlock(&write_mutex);
                    return -1;
                }
            }

            programar_sonido(frequency, time);

            // bloqueamos proceso
            is_blocked = 0;
            if (wait_event_interruptible(lista_bloq, is_blocked))
            {
                mutex_unlock(&write_mutex);
                return -ERESTARTSYS;
            }

            // actualizamos puntero a buffer y valor de bytes que quedan por leer
            buf += data_to_copy;
            count -= data_to_copy;
        }
        // ESCRITURA SIN BUFFER
        else
        {
            // miramos si ha habido una escritura incompleta que hay que hacer
            if (write_incomplete)
            {
                // si es así, leemos los datos guardados previamente
                write_incomplete = 0;
                time = *(u_int16_t *)incomplete_sound;
                frequency = *(u_int16_t *)(incomplete_sound + 2);
            }
            // si no la hay, se lee del buffer de usuario
            else
            {
                // extraemos sonido y frecuencia
                if (get_user(time, (u_int16_t __user *)(buf)) || get_user(frequency, (u_int16_t __user *)(buf + 2)))
                {
                    mutex_unlock(&write_mutex);
                    return -EFAULT;
                }
                bytes_read = 4;
            }

            programar_sonido(frequency, time);

            // bloqueamos proceso
            is_blocked = 0;
            if (wait_event_interruptible(lista_bloq, is_blocked))
            {
                mutex_unlock(&write_mutex);
                return -ERESTARTSYS;
            }

            spkr_off();

            // actualizamos valores
            count -= bytes_read; // bytes leídos
            buf += bytes_read;   // puntero del buffer de usuario
        }
    }

    // si la escritura ha sido incompleta
    if (count > 0 && count < 4)
    {
        // se guardan los bytes incompletos para completarlos posteriormente
        if (copy_from_user(incomplete_sound + incomplete_count, buf, count))
        {
            mutex_unlock(&write_mutex);
            return -EFAULT;
        }
        // guardamos el número de bytes leídos
        incomplete_count += count;
        count = 0;
        buf += count;
    }

    mutex_unlock(&write_mutex);
    // si count == 0 se han leído todos los datos
    return total - count;
}

static long spkr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int val = 0;

    if (cmd == SPKR_SET_MUTE_STATE)
    {
        spin_lock_bh(&lock_is_mute_variable);

        // obtenemos el valor del tercer parámetro
        if (copy_from_user(&val, (int __user *)arg, sizeof(int)))
            return -EFAULT;

        // si val == 0 => is not muted, si arg == 1 => is muted
        spkr_is_not_muted = 1 - val;
        if (spkr_is_not_muted == 0)
            spkr_off();

        spin_unlock_bh(&lock_is_mute_variable);
    }
    else if (cmd == SPKR_GET_MUTE_STATE)
    {
        spin_lock_bh(&lock_is_mute_variable);
        // si is_not_muted = 1 => val necesita ser 0
        val = 1 - spkr_is_not_muted;

        // devolvemos el valor del estado del altavoz
        if (copy_to_user((int __user *)arg, &val, sizeof(int)))
            return -EFAULT;

        spin_unlock_bh(&lock_is_mute_variable);
    }
    return 0;
}

static const struct file_operations spkr_fops = {
    .owner = THIS_MODULE,
    .open = spkr_open,
    .release = spkr_release,
    .write = spkr_write,
    .unlocked_ioctl = spkr_ioctl};

// **** Rutina de inicialización del módulo ****
static int init_spkr(void)
{
    // iniciamos estructuras de sincronización
    mutex_init(&open_mutex);
    mutex_init(&write_mutex);
    init_waitqueue_head(&lista_bloq);
    spin_lock_init(&lock_timer_function);
    spin_lock_init(&lock_is_mute_variable);

    // Reserva de números major y minor
    if (alloc_chrdev_region(&id_disp, spkr_minor, 1, DISP_NAME))
    {
        printk(KERN_ERR "Error en reserva de números major y minor del dispositivo.");
        return -1;
    }

    // Mostramos major seleccionado
    spkr_major = MAJOR(id_disp);
    printk(KERN_INFO "MAJOR: %d\n", spkr_major);
    printk(KERN_INFO "MINOR: %d\n", spkr_minor);

    // tamaño del buffer
    if (buffersize)
        printk(KERN_INFO "Modo escritura con buffer (%d bytes).\n", buffersize);
    else
        printk(KERN_INFO "Modo escritura sin buffer.");

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

    // creamos cola FIFO si hay buffersize
    if (buffersize && kfifo_alloc(&cola, buffersize, GFP_KERNEL))
    {
        printk(KERN_ERR "Error al reservar memoria para la kfifo.");
        return -1;
    }

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
    // si había una kfifo libera su memoria
    if (buffersize)
        kfifo_free(&cola);

    spkr_off();
    printk(KERN_INFO "Device destroyed\n");
}

module_init(init_spkr);
module_exit(exit_spkr);