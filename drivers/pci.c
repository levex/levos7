#include <levos/kernel.h>
#include <levos/pci.h>
#include <levos/arch.h>

static struct pci_device pci_devices[16];
static int num_pci_devices;

extern struct pci_driver e1000_driver;
extern struct pci_driver ide_pci_driver;
extern struct pci_driver bga_pci_driver;

#define MODULE_NAME pci

static struct pci_driver *pci_drivers[] = 
{
    &e1000_driver,
    &ide_pci_driver,
    &bga_pci_driver,
    NULL,
};

uint16_t pci_config_read(uint8_t bus, uint8_t slot,
                         uint8_t func, uint8_t offset)
{
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;

    /* create configuration address */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

    /* write out the address */
    outportl(0xCF8, address);
    /* read in the data */
    /* (offset & 2) * 8) = 0 will choose the first word of the 32 bits register */
    tmp = (uint16_t)((inportl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
}

void pci_config_write(uint8_t bus, uint8_t slot,
                         uint8_t func, uint8_t offset, uint32_t data)
{
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;

    /* create configuration address */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

    /* write out the address */
    outportl(0xCF8, address);
    /* read in the data */
    /* (offset & 2) * 8) = 0 will choose the first word of the 32 bits register */
    outportl(0xCFC, data);
}

uint16_t pci_dev_config_read(struct pci_device *pdev, uint8_t offset)
{
    return pci_config_read(pdev->pci_bus, pdev->pci_slot, pdev->pci_func, offset);
}

void pci_dev_config_write(struct pci_device *pdev, uint8_t offset, uint32_t data)
{
    pci_config_write(pdev->pci_bus, pdev->pci_slot, pdev->pci_func, offset, data);
}

uint32_t pci_dev_config_read32(struct pci_device *pdev, uint8_t offset)
{
    uint32_t bot = pci_dev_config_read(pdev, offset);
    uint32_t top = pci_dev_config_read(pdev, offset + 2);

    return (top << 16) | bot;
}

static inline uint16_t
pci_get_vendor(uint16_t bus, uint16_t device, uint16_t function)
{
    return pci_config_read(bus,device,function,0);
}

static inline uint16_t
pci_get_device_id(uint16_t bus, uint16_t device, uint16_t function)
{
    return pci_config_read(bus,device,function,2);
}

static inline uint16_t
pci_get_class_id(uint16_t bus, uint16_t device, uint16_t function)
{
    uint32_t r0 = pci_config_read(bus,device,function,0xA);
    return (r0 & ~0x00FF) >> 8;
}

static inline uint16_t
pci_get_subclass_id(uint16_t bus, uint16_t device, uint16_t function)
{
    uint32_t r0 = pci_config_read(bus,device,function,0xA);
    return (r0 & ~0xFF00);
}

uint32_t
pci_device_get_bar0(struct pci_device *pdev)
{
    uint16_t r1 = pci_config_read(pdev->pci_bus, pdev->pci_slot, pdev->pci_func, 0x10);
    uint16_t r0 = pci_config_read(pdev->pci_bus, pdev->pci_slot, pdev->pci_func, 0x12);
    uint32_t ret = (uint32_t)r0 << 16 | (uint32_t)r1;
    return ret;
}

uint8_t
pci_device_get_irqline(struct pci_device *pdev)
{
    uint16_t intdata = pci_dev_config_read(pdev, 0x3C);
    printk("read 0x%x, made it 0x%x, then 0x%x\n", intdata,
            intdata & 0x00ff, (uint8_t)(intdata & 0x00ff));

    return (uint8_t) (intdata & 0x00ff);
}

void
pci_enable_busmaster(struct pci_device *pdev)
{
    uint16_t cmd = pci_dev_config_read(pdev, 0x04);
    uint16_t status = pci_dev_config_read(pdev, 0x06);
    cmd |= (1 << 2);
    pci_dev_config_write(pdev, 0x04, (uint32_t)status << 16 | (uint32_t) cmd);
    mprintk("busmastering enabled for [0x%x:0x%x]\n", pdev->ident.pci_vendor,
                pdev->ident.pci_device);
}

void
pci_enumerate(void)
{
    memset(pci_devices, 0, sizeof(pci_devices));
    num_pci_devices = 0;

	for(uint32_t bus = 0; bus < 256; bus++)
    {
        for(uint32_t slot = 0; slot < 32; slot++)
        {
            for(uint32_t function = 0; function < 8; function++)
            {
                uint16_t vendor = pci_get_vendor(bus, slot, function);
                if(vendor == 0xffff)
                    continue;
                uint16_t device = pci_get_device_id(bus, slot, function);

                printk("PCI device: vendor 0x%x device 0x%x\n", vendor, device);

                pci_devices[num_pci_devices].ident.pci_vendor = vendor;
                pci_devices[num_pci_devices].ident.pci_device = device;
                pci_devices[num_pci_devices].pci_bus = bus;
                pci_devices[num_pci_devices].pci_slot = slot;
                pci_devices[num_pci_devices].pci_func = function;
                pci_devices[num_pci_devices].pci_class =
                    pci_get_class_id(bus, slot, function);
                pci_devices[num_pci_devices].pci_subclass =
                    pci_get_subclass_id(bus, slot, function);
                pci_devices[num_pci_devices].priv   = NULL;
                
                num_pci_devices ++;
            }
        }
    }
}

inline struct pci_driver *
pci_device_get_driver(struct pci_device *pdev)
{
    return pdev->driver;
}

void
pci_attach_driver(struct pci_device *pdev, struct pci_driver *driver)
{
    pdev->driver = driver;
    driver->attach(pdev);
}

struct pci_device *
pci_find(struct pci_ident *id)
{
    int i;

    for (i = 0; i < num_pci_devices; i ++) {
        struct pci_ident *other = &pci_devices[i].ident;

        if (other->pci_vendor == id->pci_vendor &&
                other->pci_device == id->pci_device &&
                other->pci_vendor != 0xFFFF)
            return container_of(other, struct pci_device, ident);
    }
    return NULL;
}

void
pci_driver_init()
{
    int i, j, rc;
    struct pci_driver *driver;
    struct pci_device *device;
    struct pci_ident *ident;

    for (i = 0, driver = pci_drivers[i]; driver != NULL; driver = pci_drivers[++i]) {
        printk("loading driver %s\n", driver->name);
        if (driver->init)
            driver->init();
        for (j = 0, ident = &driver->idents[0];
                ident->pci_vendor != 0xFFFF;
                ident = &driver->idents[++j]) {
            printk("probing ident vendor 0x%x device 0x%x \n",
                    ident->pci_vendor, ident->pci_device);
            if (device = pci_find(ident)) {
                if (driver->probe) {
                    rc = driver->probe(device);
                    if (rc)
                        continue;

                    if (pci_device_get_driver(device))
                        printk("WARNING: PCI device fight detected, OS unstable\n");

                    pci_attach_driver(device, driver);
                }
            }
        }
    }
}

int
pci_init()
{
    printk("pci: enumerating buses...");
    pci_enumerate();
    printk("pci: initializing drivers\n");
    pci_driver_init();
    return 0;
}
