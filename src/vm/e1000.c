#include <stdio.h>
#include "devices/pci.h"
#include "vm/e1000.h"
#include "vm/e1000-hw.h"
#include "threads/malloc.h"
#include <string.h>

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

    /* TODO: initialize transmit descriptor ring */

    /* Print status */
    printf("E1000 Status 0x%x\n", pci_reg_read32(e1000_io, E1000_STATUS));
}

/*
    Gives the e1000 a packet containing up to 1518 bytes to send over ethernet..
    Returns 0 on success, -1 on failure
*/
int 
e1000_send_packet(const void *data, uint16_t num_bytes){ // TODO: how do you address the data?
  /* Ensure the packet is within the allowed size range */
  if (num_bytes > ENET_PACKET_MAX_BYTES){
    return -1;
  }

  /* Get necessary data about the transmit descriptor ring */
  void * base_ptr = pci_reg_read32(e1000_io, E1000_TDBAH) << 32;
  base_ptr += pci_reg_read32(e1000_io, E1000_TDBAL);
  uint32_t head_offset = pci_reg_read32(e1000_io, E1000_TDH);
  uint32_t tail_offset = pci_reg_read32(e1000_io, E1000_TDT);
  uint32_t ring_size = pci_reg_read32(e1000_io, E1000_TDLEN);

  /* Get the descriptor at the tail of the ring */
  struct e1000_tx_desc *transmit_descriptor = base_ptr + tail_offset * TX_DESC_SIZE; //TODO: is multiplying by TX_DESC_SIZE necessary
  void *buffer_address = transmit_descriptor->buffer_addr;
  
  /* Check if the descriptor ring is overflowing*/
  if(tail_offset == head_offset){
    // TODO: handle ring overflow
    return -1;
  }

  /* 
    If the the descriptor's data is finished transmitting, free the data. Otherwise, return an error 
    The "Descriptor Done" bit can be found at status bit 0, which is bit 32 of the buffer
  */
  if (transmit_descriptor->upper.fields.status & E1000_TXD_STAT_DD)
  {
    free(transmit_descriptor->buffer_addr); // TODO
  }
  else
  {
    return -1;
  }

  /* Fill in the descriptor */
  transmit_descriptor->buffer_addr = data; // point buffer_addr to the data
  transmit_descriptor->lower.flags.length = num_bytes; // set length

  transmit_descriptor->upper.fields.status = 0; // reset status
  transmit_descriptor->lower.flags.cmd = 0 | 0x1000; // set Report Status bit

  /* Update the descriptor ring's tail pointer */
  pci_reg_write32(e1000_io, E1000_TDT, (tail_offset + TX_DESC_SIZE) % ring_size); //TODO: is TX_DESC_SIZE necessary?

  return 0;
}


/* 
    Checks the e1000 for a newly received packet.
    If it finds a packet, it returns 0 and places the contents in the provided buffer.
    If not, it returns -1.
*/
int 
e1000_recieve_packet(char data_buf[ENET_PACKET_MAX_BYTES]){ //TODO: how do you choose the location to receive data from?

  /* Get necessary data about the receive descriptor ring */
  void * base_ptr = pci_reg_read32(e1000_io, E1000_RDBAH) << 32;
  base_ptr += pci_reg_read32(e1000_io, E1000_RDBAL);
  uint32_t head_offset = pci_reg_read32(e1000_io, E1000_RDH);
  uint32_t tail_offset = pci_reg_read32(e1000_io, E1000_RDT);
  uint32_t ring_size = pci_reg_read32(e1000_io, E1000_RDLEN);

  /* Get the next waiting receive descriptor */
  struct e1000_tx_desc *receive_descriptor = base_ptr + tail_offset; // TODO: do we need an offset
  void *buffer_address = receive_descriptor->buffer_addr;

  /* If there is no new packet available, return */
  if (!(receive_descriptor->upper.fields.status & E1000_TXD_STAT_DD)){
    return -1;
  }

  /* Copy the received data to the provided buffer*/
  void *data_ptr = receive_descriptor->upper.data << 32;
  data_ptr += receive_descriptor->lower.data;
  void *received_data = (void *)strlcpy(data_buf, data_ptr, receive_descriptor->lower.flags.length); //TODO: what is the best way to handle allocation of this data?

  /* Clear the descriptor's status bits */
  receive_descriptor->upper.fields.status = 0;

  /* Update the descriptor ring's tail pointer */
  pci_reg_write32(e1000_io, E1000_RDT, (tail_offset + 1) % ring_size); //TODO: should we use 1 or RX_DESC_SIZE?

  return 0;
}


/* Enable PCI device to be DMA bus master and allow memory + I/O input */
void
pci_enable_bus_master (struct pci_dev *pd)
{
  // 0x4 = Bus Master, 0x2 = Memory Space, 0x1 = I/O Space
  // see Figure 6-2 PCI-2.2 spec
  pci_write_config32 (pd, PCI_COMMAND_OFFSET, 0x7);
}