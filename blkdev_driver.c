#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#ifndef SECTOR_SHIFT
#define SECTOR_SHIFT 9
#endif

#ifndef BLOCK_DEVICE_BUFFER_CAPACITY
#define BLOCK_DEVICE_BUFFER_CAPACITY 204800
#endif

#ifndef SUCCESS
#define SUCCESS 0
#endif

#ifndef LOG
#define LOG(message) printk(KERN_WARNING "Block driver: %s!\n", message)
#endif

#ifndef LOG_WITH_NUM
#define LOG_WITH_NUM(message, number) printk(KERN_WARNING "Block driver: %s!(%d)\n", message, number)
#endif

/* Representation of request */
typedef struct blkdev_cmd_s {

} blkdev_cmd_t;

/* Representation of device */
typedef struct blkdev_device_s {
    sector_t capacity;
    u8 *data;

    struct blk_mq_tag_set tag_set;
    struct request_queue *queue;

    struct gendisk *disk;
} blkdev_device_t;

/* Constants */
static const char *blkdev_driver_name = "blkdev_driver";

/* Global variables */
volatile int cmd_value = 0;

static int blkdev_driver_major;
static blkdev_device_t *blkdev = NULL;
static struct kobject *blkdev_driver_ref = NULL;

/*************** Function prototypes **********************/
static int __init blkdev_init(void);

static void __exit blkdev_exit(void);

/*************** Driver functions **********************/
//...

/*************** Sysfs functions **********************/
static ssize_t sysfs_show(struct kobject *kobj,
                          struct kobj_attribute *attr, char *buf);

static ssize_t sysfs_store(struct kobject *kobj,
                           struct kobj_attribute *attr, const char *buf, size_t count);

struct kobj_attribute blkdev_attr = __ATTR(cmd_value, 0660, sysfs_show, sysfs_store);

/*************** Utility functions **********************/
static int allocate_blkdev(blkdev_device_t *dev);

static void free_blkdev(blkdev_device_t *dev);

/*************** Request proccessors **********************/
static int do_simple_request(struct request *rq);

static blk_status_t rq_process(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd);

/* Request options */
static struct blk_mq_ops _mq_ops = {
        .queue_rq = rq_process,
};

/**
 * Sysfs functions
 */
static ssize_t sysfs_show(struct kobject *kobj,
                          struct kobj_attribute *attr, char *buf) {
    LOG("Sysfs - Read");
    return sprintf(buf, "%d", cmd_value);
}

static ssize_t sysfs_store(struct kobject *kobj,
                           struct kobj_attribute *attr, const char *buf, size_t count) {
    LOG("Sysfs - WRITE");
    sscanf(buf, "%d", &cmd_value);
    return count;
}

/**
 * Working with memory by blkdev_device_t
 */
static int allocate_blkdev(blkdev_device_t *dev) {
    dev->capacity = BLOCK_DEVICE_BUFFER_CAPACITY;
    dev->data = kmalloc(dev->capacity << SECTOR_SHIFT, GFP_KERNEL);
    if (dev->data == NULL) {
        LOG("Device allocation failed");
        return -ENOMEM;
    }
    LOG("Device allocation succeed");
    return SUCCESS;
}

static void free_blkdev(blkdev_device_t *dev) {
    if (dev->data) {
        kfree(dev->data);

        dev->data = NULL;
        dev->capacity = 0;
    }
    LOG("Device memory freed");
}

/**
 * Simple request service implementation
 */
static int do_simple_request(struct request *rq) {
    struct bio_vec bvec;
    struct req_iterator iter;
    blkdev_device_t *dev = rq->q->queuedata;

    loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;
    loff_t dev_size = (loff_t)(dev->capacity << SECTOR_SHIFT);

    rq_for_each_segment(bvec, rq, iter)
    {
        unsigned long b_len = bvec.bv_len;

        void *b_buf = page_address(bvec.bv_page) + bvec.bv_offset;

        if ((pos + b_len) > dev_size)
            b_len = (unsigned long) (dev_size - pos);

        if (rq_data_dir(rq))//WRITE
            memcpy(dev->data + pos, b_buf, b_len);
        else//READ
            memcpy(b_buf, dev->data + pos, b_len);

        pos += b_len;
    }
    return 0;
}

/**
 * Central request processor
 */
static blk_status_t rq_process(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd) {
    struct request *rq = bd->rq;

    blk_mq_start_request(rq); //must be called before starting processing a request(doc)

    (do_simple_request(rq) != SUCCESS)
    ? blk_mq_end_request(rq, BLK_STS_IOERR)
    : blk_mq_end_request(rq, BLK_STS_OK);

    return BLK_STS_OK;
}

/**
 * Block device operations initialization
 */
static const struct block_device_operations _fops = {
        .owner = THIS_MODULE
};

/**
 * Device handle functions
 */
static int blkdev_add(void) {

    blkdev_device_t *dev = kzalloc(sizeof(blkdev_device_t), GFP_NOWAIT);
    if (dev == NULL) {
        LOG("Unable to allocate device structure");
        return -ENOMEM;
    }
    blkdev = dev;

    if (allocate_blkdev(dev) != SUCCESS) {
        LOG("Unable to allocate device memory");
        return -ENOMEM;
    }

    dev->tag_set.cmd_size = sizeof(blkdev_cmd_t);
    dev->tag_set.driver_data = dev;
    dev->queue = blk_mq_init_sq_queue(&dev->tag_set,
                                      &_mq_ops, 128,
                                      BLK_MQ_F_SHOULD_MERGE | BLK_MQ_F_SG_MERGE);
    dev->queue->queuedata = dev;

    struct gendisk *disk = alloc_disk(1); //only one partition
    disk->flags |= GENHD_FL_NO_PART_SCAN;
    disk->flags |= GENHD_FL_REMOVABLE;
    disk->major = blkdev_driver_major;
    disk->first_minor = 0;
    disk->fops = &_fops;
    disk->private_data = dev;
    disk->queue = dev->queue;
    sprintf(disk->disk_name, "sblkdev%d", 0);
    set_capacity(disk, dev->capacity);

    dev->disk = disk;
    add_disk(disk);
    return 0;
}

static void blkdev_remove(void) {
    if (blkdev == NULL)
        return;

    if (blkdev->disk)
        del_gendisk(blkdev->disk);

    if (blkdev->queue) {
        blk_cleanup_queue(blkdev->queue);
        blkdev->queue = NULL;
    }

    if (blkdev->tag_set.tags)
        blk_mq_free_tag_set(&blkdev->tag_set);

    if (blkdev->disk) {
        put_disk(blkdev->disk);
        blkdev->disk = NULL;
    }

    free_blkdev(blkdev);

    kfree(blkdev);
    blkdev = NULL;

    LOG("Simple block device was removed");
}

/**
 * Create driver on sysfs
 */
static int register_driver_in_sys(void) {
    blkdev_driver_ref = kobject_create_and_add(blkdev_driver_name, NULL);

    if (blkdev_driver_ref == NULL) {
        LOG("Cannot create sysfs driver directory");
        return -ENOMEM;
    }
    LOG("Block device driver was registered in sysfs");

    if (sysfs_create_file(blkdev_driver_ref, &blkdev_attr.attr)) {
        LOG("Cannot create sysfs file");
        return -ENOMEM;
    }
    LOG("Driver attribute was added");

    return 0;
}

/**
 * Module life cycle functions
 */
static int __init blkdev_init(void) {
    blkdev_driver_major = register_blkdev(0, blkdev_driver_name);
    if (blkdev_driver_major < 0) {
        LOG("Unable to get major number");
        return -EBUSY;
    }
    LOG("Major number registered");

    if (blkdev_add()) {
        unregister_blkdev(blkdev_driver_major, blkdev_driver_name);
    }

    if (register_driver_in_sys()) {
        blkdev_remove();
        unregister_blkdev(blkdev_driver_major, blkdev_driver_name);
    }

    return 0;
}

static void __exit blkdev_exit(void) {
    kobject_put(blkdev_driver_ref);

    blkdev_remove();

    if (blkdev_driver_major > 0)
        unregister_blkdev(blkdev_driver_major, blkdev_driver_name);
}

module_init(blkdev_init);
module_exit(blkdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dmittrey");