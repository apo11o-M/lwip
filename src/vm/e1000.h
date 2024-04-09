#ifndef E1000_H
#define E1000_H

#define E1000_STATUS   0x00008  /* Device Status - RO */

void e1000_init(void);
void pci_enable_bus_master (struct pci_dev *pd);

// int e1000_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
// void e1000_close(void);

#endif