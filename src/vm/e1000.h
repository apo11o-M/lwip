#ifndef E1000_H
#define E1000_H

#define E1000_STATUS   0x00008  /* Device Status - RO */
#define ENET_PACKET_MAX_BYTES 1518
#define TX_DESC_SIZE 16

void e1000_init(void);
void pci_enable_bus_master (struct pci_dev *pd);

/*
    Gives the e1000 a packet containing up to 1518 bytes to send over ethernet..
    Returns 0 on success, -1 on failure
*/
int e1000_send_packet(const void *data, uint16_t num_bytes);

/* 
    Checks the e1000 for a newly received packet.
    If it finds a packet, it returns 0 and places the contents in the provided buffer.
    If not, it returns -1.
*/
int e1000_recieve_packet(char data_buf[ENET_PACKET_MAX_BYTES]);


#endif