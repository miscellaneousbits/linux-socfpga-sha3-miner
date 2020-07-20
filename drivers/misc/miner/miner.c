#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "miner.h"

#define INFO_HDR "Miner: "

#define REG_WRITE32(v, i) iowrite32((v), (void __iomem*)((uint32_t*)dev->regs + ((i) / REG_BYTES)))

#define REG_READ32(i) ioread32((void __iomem*)((uint32_t*)dev->regs + ((i) / REG_BYTES)))

#define MINER_REGS_BYTES (MINER_REGS * REG_BYTES)

static DEFINE_MUTEX(dev_number_mtx);
static int dev_number = 0;

struct miner_dev
{
    struct miscdevice misc_dev;
    void __iomem* regs;
    int irq_number;
    struct wait_queue_head wait_q;
    loff_t offset;
};

static irqreturn_t isr(int irq, void* dev_id)
{
    struct miner_dev* dev = dev_id;
    /* Clear the interrupt */
    REG_READ32(MINER_CTL_REG_B);
    /* Release all waiting threads */
    wake_up_interruptible(&dev->wait_q);
    return IRQ_HANDLED;
}

static int open(struct inode* inode, struct file* file)
{
    return EOK;
}

static int release(struct inode* inode, struct file* file)
{
    struct miner_dev* dev = container_of(file->private_data, struct miner_dev, misc_dev);
    REG_WRITE32(0, MINER_CTL_REG_B);
    return EOK;
}

static uint32_t poll(struct file* file, poll_table* wait)
{
    struct miner_dev* dev = container_of(file->private_data, struct miner_dev, misc_dev);
    poll_wait(file, &dev->wait_q, wait);
    return (REG_READ32(MINER_STATUS_REG_B) & MINER_STATUS_FOUND_MASK) ? (POLLIN | POLLRDNORM) : EOK;
}

static int check_offset(loff_t offset)
{
    return ((offset & 3) || (offset >= MINER_REGS_BYTES)) ?  -EINVAL : EOK;
}

static int check_offset_len(loff_t offset, size_t len)
{
    return (check_offset(offset) || (len != REG_BYTES) || ((offset + len) > MINER_REGS_BYTES)) ? -EINVAL : EOK;
}

static loff_t llseek(struct file* file, loff_t offset, int whence)
{
    struct miner_dev* dev = container_of(file->private_data, struct miner_dev, misc_dev);
    if ((whence != SEEK_SET) || check_offset(offset))
        return -EINVAL;
    return dev->offset = offset;
}

static ssize_t read(struct file* file, char* buffer, size_t len, loff_t* ofset)
{
    struct miner_dev* dev = container_of(file->private_data, struct miner_dev, misc_dev);
    uint32_t u32;
    if (check_offset_len(dev->offset, len))
        return -EINVAL;
    u32 = REG_READ32(dev->offset);
    if (copy_to_user(buffer, &u32, sizeof(u32)))
        return -EINVAL;
    return dev->offset += len;
}

static ssize_t write(struct file* file, const char* buffer, size_t len, loff_t* ofset)
{
    uint32_t u32;
    struct miner_dev* dev = container_of(file->private_data, struct miner_dev, misc_dev);
    if (check_offset_len(dev->offset, len))
        return -EINVAL;
    if (copy_from_user(&u32, buffer, sizeof(u32)))
        return -EINVAL;
    REG_WRITE32(u32, dev->offset);
    return dev->offset += len;
}

static const struct file_operations fops =
{
    .owner = THIS_MODULE,
    .poll = poll,
    .read = read,
    .write = write,
    .llseek = llseek,
    .open = open,
    .release = release
};

static const char* dev_name_template = "miner%d";

static int miner_probe(struct platform_device* pdev)
{
    int ret_val = -EBUSY;
    struct miner_dev* dev;
    struct resource* r = 0;
    int dev_num;
    char clock;
    uint32_t status, minor, major;

    pr_info(INFO_HDR "Probing.\n");

    // Get the memory resources for this miner device
    r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (r == NULL)
    {
        pr_err(INFO_HDR "No IO resource.\n");
        goto error_exit1;
    }

    ret_val = -ENOMEM;

    dev = devm_kzalloc(&pdev->dev, sizeof(struct miner_dev), GFP_KERNEL);
    if (dev == NULL)
    {
        pr_err(INFO_HDR "No device memory.\n");
        goto error_exit1;
    }

    dev->regs = devm_ioremap_resource(&pdev->dev, r);
    if (IS_ERR(dev->regs))
    {
        pr_err(INFO_HDR "Remap failed.\n");
        goto error_exit1;
    }

    ret_val = -EBUSY;
    // Reset the miner (turn off run bit 0)
    REG_WRITE32(0, MINER_CTL_REG_B);

    // Look for fingerprint
    if (REG_READ32(MINER_SHA3_REG_B) != 0x53484133)
    {  // "SHA3"
        pr_info(INFO_HDR "Incompatible FPGA.\n");
        goto error_exit1;
    }
    pr_info(INFO_HDR "SHA3 miner FPGA detected.\n");

    status = REG_READ32(MINER_STATUS_REG_B);
    major = (status & MINER_STATUS_MAJ_MASK) >> MINER_STATUS_MAJ_SHIFT;
    minor = (status & MINER_STATUS_MIN_MASK) >> MINER_STATUS_MIN_SHIFT;

    if ((major > 1) || ((major == 1) && (minor > 1)))
    {
        pr_info(INFO_HDR "incompatible FPGA version.\n");
        goto error_exit1;
    }

    clock = (status & MINER_STATUS_MHZ_MASK) >> MINER_STATUS_MHZ_SHIFT;

    pr_info(INFO_HDR "Miner FPGA version %d.%d, %u MHz clock.\n", major, minor, clock);

    dev->irq_number = platform_get_irq(pdev, 0);
    if (dev->irq_number < 0)
    {
        pr_info(INFO_HDR "No IRQ resource.\n");
        goto error_exit1;
    }
    ret_val = request_irq(dev->irq_number, isr, 0, dev_name(&pdev->dev), dev);
    if (ret_val < 0)
    {
        pr_info(INFO_HDR "IRQ %d acquire failed.\n", dev->irq_number);
        goto error_exit1;
    }
    pr_info(INFO_HDR "Acquired IRQ %d.\n", dev->irq_number);

    mutex_lock(&dev_number_mtx);
    dev_num = dev_number++;
    mutex_unlock(&dev_number_mtx);

    dev->misc_dev.name = devm_kmalloc(&pdev->dev, strlen(dev_name_template) + 4, GFP_KERNEL);
    if (dev->misc_dev.name == NULL)
    {
        pr_err(INFO_HDR "No device memory.\n");
        goto error_exit0;
    }
    snprintf((char*)dev->misc_dev.name, strlen(dev_name_template) + 4, dev_name_template, dev_num);

    dev->misc_dev.minor = MISC_DYNAMIC_MINOR;  // Dynamically choose a minor number
    dev->misc_dev.fops = &fops;

    init_waitqueue_head(&dev->wait_q);

    ret_val = misc_register(&dev->misc_dev);
    if (ret_val != EOK)
    {
        pr_info(INFO_HDR "Register device failed.\n");
        goto error_exit0;
    }
    pr_info(INFO_HDR "Registered /dev/%s.\n", dev->misc_dev.name);

    platform_set_drvdata(pdev, dev);

    return EOK;

error_exit0:
    free_irq(dev->irq_number, dev);
error_exit1:
    pr_info(INFO_HDR "Probe failed.\n");
    return ret_val;
}

static int miner_remove(struct platform_device* pdev)
{
    // Grab the instance-specific information out of the platform device
    struct miner_dev* dev = (struct miner_dev*)platform_get_drvdata(pdev);

    pr_info(INFO_HDR "Removing.\n");

    // Turn the miner off
    REG_WRITE32(0, MINER_CTL_REG_B);
    if (dev->irq_number >= 0)
        free_irq(dev->irq_number, dev);

    // Unregister the character file (remove it from /dev)
    misc_deregister(&dev->misc_dev);

    return EOK;
}

// Specify which device tree devices this driver supports
static struct of_device_id miner_dt_ids[] =
{
    {.compatible = "dev,miner"},
    {/* end of table */}
};

// Inform the kernel about the devices this driver supports
MODULE_DEVICE_TABLE(of, miner_dt_ids);

static struct platform_driver miner_platform =
{
    .probe = miner_probe,
    .remove = miner_remove,
    .driver =
    {
        .name = "miner",
        .owner = THIS_MODULE,
        .of_match_table = miner_dt_ids
    }
};

static int miner_init(void)
{
    int ret_val = EOK;
    pr_info(
        INFO_HDR "Initializing miner driver version %d.%d.\n", MINER_DRIVER_MAJ, MINER_DRIVER_MIN);

    // Register our driver with the "Platform Driver" bus
    ret_val = platform_driver_register(&miner_platform);
    if (ret_val != EOK)
    {
        pr_err(INFO_HDR "Platform_driver_register returned %d.\n", ret_val);
        return ret_val;
    }

    pr_info(INFO_HDR "Initialized.\n");

    return -EOK;
}

static void miner_exit(void)
{
    platform_driver_unregister(&miner_platform);

    pr_info(INFO_HDR "Unregistered.\n");
}

module_init(miner_init);
module_exit(miner_exit);

// Define information about this kernel module
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jean M. Cyr <jean.m.cyr@gmail.com>");
MODULE_DESCRIPTION("Exposes a miner control miscellaneous device");
MODULE_VERSION("1.2");
