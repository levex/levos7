#include <levos/pci.h>
#include <levos/kernel.h>

#define MODULE_NAME pci_ide

static int ata_port_primary;
static int ata_port_primary_control;
static int ata_port_secondary;
static int ata_port_secondary_control;
static int ata_port_busmaster;

static int __ata_dma_possible = 0;

int
ata_dma_possible(void)
{
    return __ata_dma_possible;
}

void
ide_pci_init(void)
{
    mprintk("PCI IDE driver loaded\n");
    ata_port_primary = ata_port_primary_control = 0;
    ata_port_secondary = ata_port_secondary_control = 0;
    __ata_dma_possible = 0;
}

int
ide_pci_probe(struct pci_device *pdev)
{
    if (pdev->pci_class != 0x01) {
        mprintk("this isn't the device we are looking for\n");
        return -ENODEV;
    }

    if (pdev->pci_subclass != 0x01) {
        mprintk("this isn't an IDE controller, subclass: 0x%x\n",
                pdev->pci_subclass);
        return -ENODEV;
    }

    mprintk("found an IDE controller\n");

    return 0;
}

int ide_pci_attach(struct pci_device *pdev)
{
    uint32_t bar0 = pci_dev_config_read32(pdev, 0x10);
    uint32_t bar1 = pci_dev_config_read32(pdev, 0x14);
    uint32_t bar2 = pci_dev_config_read32(pdev, 0x18);
    uint32_t bar3 = pci_dev_config_read32(pdev, 0x1C);
    uint32_t bar4 = pci_dev_config_read32(pdev, 0x20);

    if (bar0 == 0 || bar0 == 1)
        ata_port_primary = 0x1F0;
    if (bar1 == 0 || bar1 == 1)
        ata_port_primary_control = 0x3F6;

    if (bar2 == 0 || bar2 == 1)
        ata_port_secondary = 0x170;
    if (bar3 == 0 || bar3 == 1)
        ata_port_secondary_control = 0x376;

    ata_port_busmaster = bar4 & ~1;

    printk("primary: {0x%x, 0x%x} secondary: {0x%x, 0x%x} busmaster: 0x%x\n",
            ata_port_primary, ata_port_primary_control,
            ata_port_secondary, ata_port_secondary_control,
            ata_port_busmaster);

    /* set the tracking */
    __ata_dma_possible = 1;

    /* enable bus mastering */
    pci_enable_busmaster(pdev);

    /* switch devices to DMA mode */
    ata_switch_dma(ata_port_busmaster);

    return 0;
}

static const struct pci_ident ide_pci_idents[] = {
    PCI_IDENT(0x8086, 0x7010),
    PCI_END_IDENT,
};

struct pci_driver ide_pci_driver = {
    .name = "IDE controller",
    .init = ide_pci_init,
    .probe = ide_pci_probe,
    .attach = ide_pci_attach,
    .idents = (void *) &ide_pci_idents,
};
