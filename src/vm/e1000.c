#include <stdio.h>
#include "devices/pci.h"
#include "vm/e1000.h"

#define PCI_COMMAND_OFFSET 32 /* offsetof (struct pci_config_header, pci_command) */

static struct pci_dev *e1000_dev;
static struct pci_io *e1000_io;


void
e1000_init (void)
{
    /* Get the e1000's PCI struct */
    e1000_dev = pci_get_device (0x8086, 0x100e, 0, 0);

    /* Get the e100's IO wrapper*/
    e1000_io = pci_io_enum(e1000_dev, NULL);

    /* Give the e1000 bus control */
    pci_enable_bus_master(e1000_dev); 

    /* Print status */
    printf("E1000 Status 0x%x\n", pci_reg_read32(e1000_io, E1000_STATUS));
}


/* Enable PCI device to be DMA bus master and allow memory + I/O input */
void
pci_enable_bus_master (struct pci_dev *pd)
{
  // 0x4 = Bus Master, 0x2 = Memory Space, 0x1 = I/O Space
  // see Figure 6-2 PCI-2.2 spec
  pci_write_config32 (pd, PCI_COMMAND_OFFSET, 0x7);
}