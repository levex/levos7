#ifndef __LEVOS_PCI_H
#define __LEVOS_PCI_H

#include <levos/types.h>

struct pci_device;

struct pci_ident {
    uint16_t pci_vendor;
    uint16_t pci_device;
};

#define PCI_END_IDENT {.pci_vendor = 0xFFFF, .pci_vendor = 0xFFFF}

struct pci_driver {
    char *name;

    int (*probe)(struct pci_device *);
    int (*attach)(struct pci_device *);
    void (*init)(void);

    struct pci_ident *idents;
};

struct pci_device {
    struct pci_ident ident;

    struct pci_driver *driver;

    uint32_t pci_bus;
    uint32_t pci_slot;
    uint32_t pci_func;

    void *priv;
};

int pci_init(void);

uint32_t pci_device_get_bar0(struct pci_device *);
void pci_enable_busmaster(struct pci_device *);
uint8_t pci_device_get_irqline(struct pci_device *);
#endif /* __LEVOS_PCI_H */
