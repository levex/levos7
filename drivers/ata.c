#include <levos/kernel.h>
#include <levos/ata.h>
#include <levos/arch.h>
#include <levos/device.h>
#include <levos/intr.h>

#define ATA_PRIMARY_IRQ 14
#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_DCR_AS 0x3F6

#define ATA_SECONDARY_IRQ 15
#define ATA_SECONDARY_IO 0x170
#define ATA_SECONDARY_DCR_AS 0x376

#define ATA_PRIMARY 0x00
#define ATA_SECONDARY 0x01

#define ATA_MASTER 0x00
#define ATA_SLAVE  0x01

void ide_select_drive(uint8_t bus, uint8_t i)
{
    if (bus == ATA_PRIMARY)
        if (i == ATA_MASTER)
            outportb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xA0);
        else outportb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xB0);
    else
        if (i == ATA_MASTER)
            outportb(ATA_SECONDARY_IO + ATA_REG_HDDEVSEL, 0xA0);
        else outportb(ATA_SECONDARY_IO + ATA_REG_HDDEVSEL, 0xB0);
}

void ide_400ns_delay(uint16_t io)
{
    for (int i = 0; i < 4; i++)
        inportb(io + ATA_REG_ALTSTATUS);
}

void
ide_poll(uint16_t io)
{
    uint8_t status;

    /* read the ALTSTATUS 4 times */
    inportb(io + ATA_REG_ALTSTATUS);
    inportb(io + ATA_REG_ALTSTATUS);
    inportb(io + ATA_REG_ALTSTATUS);
    inportb(io + ATA_REG_ALTSTATUS);

    /* now wait for the BSY bit to clear */
    while ((status = inportb(io + ATA_REG_STATUS)) & ATA_SR_BSY)
        ;

    status = inportb(io + ATA_REG_STATUS);

    if (status & ATA_SR_ERR)
        panic("ATA ERR bit is set\n");

    return;
}

int
ata_status_wait(int io_base, int timeout) {
	int status;

	if (timeout > 0) {
		int i = 0;
		while ((status = inportb(io_base + ATA_REG_STATUS)) & ATA_SR_BSY && (i < timeout)) i++;
	} else {
		while ((status = inportb(io_base + ATA_REG_STATUS)) & ATA_SR_BSY);
	}
	return status;
}

void
ata_io_wait(int io_base) {
	inportb(io_base + ATA_REG_ALTSTATUS);
	inportb(io_base + ATA_REG_ALTSTATUS);
	inportb(io_base + ATA_REG_ALTSTATUS);
	inportb(io_base + ATA_REG_ALTSTATUS);
}

int
ata_wait(int io, int adv)
{
    uint8_t status = 0;

    ata_io_wait(io);

    status = ata_status_wait(io, -1);

    if (adv) {
        status = inportb(io + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return 1;
        if (status & ATA_SR_DF)  return 1;
        if (!(status & ATA_SR_DRQ)) return 1;
    }

    return 0;
}

void
ata_read_one_sector(char *buf, size_t lba)
{
    uint16_t io = ATA_PRIMARY_IO;
    uint8_t  dr = ATA_MASTER;

    uint8_t cmd = 0xE0;
    int errors = 0;
    uint8_t slavebit = 0x00;

    //printk("ata: lba: %d\n", lba);
try_a:
    outportb(io + ATA_REG_CONTROL, 0x02);

    ata_wait(io, 0);

    outportb(io + ATA_REG_HDDEVSEL, (cmd | (uint8_t)((lba >> 24 & 0x0F))));
    outportb(io + ATA_REG_FEATURES, 0x00);
    outportb(io + ATA_REG_SECCOUNT0, 1);
    outportb(io + ATA_REG_LBA0, (uint8_t)(lba));
    outportb(io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outportb(io + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outportb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait(io, 1)) {
        errors ++;
        if (errors > 4)
            panic("Failed to read ATA block %d\n", lba);

        goto try_a;
    }

    for (int i = 0; i < 256; i++) {
        uint16_t d = inportw(io + ATA_REG_DATA);
        *(uint16_t *)(buf + i * 2) = d;
    }

    ata_wait(io, 0);
    return;
}

int
ata_read(struct device *dev, void *buf, size_t count)
{
    unsigned long pos = dev->pos;

    DISABLE_IRQ();

    for (int i = 0; i < count; i++)
    {
        ata_read_one_sector(buf, pos + i);
        buf += 512;
    }
    dev->pos += count;

    ENABLE_IRQ();
    return count;
}

int
ata_write_one_sector(uint16_t *buf, size_t lba)
{
    uint16_t io = ATA_PRIMARY_IO;
    uint8_t  dr = ATA_MASTER;

    uint8_t cmd = 0xE0;
    uint8_t slavebit = 0x00;

    outportb(io + ATA_REG_CONTROL, 0x02);

    ata_wait(io, 0);

    outportb(io + ATA_REG_HDDEVSEL, (cmd | (uint8_t)((lba >> 24 & 0x0F))));
    ata_wait(io, 0);
    outportb(io + ATA_REG_FEATURES, 0x00);
    outportb(io + ATA_REG_SECCOUNT0, 1);
    outportb(io + ATA_REG_LBA0, (uint8_t)(lba));
    outportb(io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outportb(io + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outportb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    ata_wait(io, 0);

    for (int i = 0; i < 256; i++) {
        outportw(io + ATA_REG_DATA, buf[i]);
        asm volatile("nop; nop; nop");
    }
    outportb(io + 0x07, ATA_CMD_CACHE_FLUSH);

    ata_wait(io, 0);

    return 0;
}

int
ata_write(struct device *dev, void *buf, size_t count)
{
    unsigned long pos = dev->pos;

    DISABLE_IRQ();
    for (int i = 0; i < count; i++)
    {
        ata_write_one_sector(buf, pos + i);
        buf += 512;
        for (int j = 0; j < 1000; j ++)
            ;
    }
    dev->pos += count;
    ENABLE_IRQ();
    return count;
}

int
ata_sync()
{
    return -ENOSYS;
}

int
ide_identify(void)
{
    uint16_t io = 0;
    io = 0x1F0;
    outportb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xA0);
    outportb(io + ATA_REG_SECCOUNT0, 0);
    outportb(io + ATA_REG_LBA0, 0);
    outportb(io + ATA_REG_LBA1, 0);
    outportb(io + ATA_REG_LBA2, 0);
    outportb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inportb(io + ATA_REG_STATUS);
    if (status)
    {
        /* read the IDENTIFY data */
        struct device *dev;
        void *ide_buf = malloc(512);
        if (!ide_buf)
            return -ENOMEM;

        dev = malloc(sizeof(*dev));
        if (!dev) {
            free(ide_buf);
            return -ENOMEM;
        }

        for (int i = 0; i < 256; i++)
            *(uint16_t *)(ide_buf + i*2) = inportw(io + ATA_REG_DATA);
        free(ide_buf);

        dev->read = ata_read;
        dev->write = ata_write;
        dev->pos = 0;
        dev->type = DEV_TYPE_BLOCK;
        dev->priv = NULL;
        dev->name = "ata";
        device_register(dev);
        return 1;
    } else {
        printk("ata: IDENTIFY error on b0d0 -> no status\n");
        return 0;
    }
}

void
ata_probe(void)
{
    int devs = 0;
    if (ide_identify() > 0) {
        printk("ata: primary master is online\n");
        devs ++;
    }
    printk("ata: %d devices brought online\n", devs);
}

void
ide_prim_irq(struct pt_regs *r)
{
    return;
}

void
ata_init(void)
{
    printk("ata: using PIO mode, disregarding secondary\n");
    intr_register_hw(0x20 + ATA_PRIMARY_IRQ, ide_prim_irq);
    ata_probe();
}
