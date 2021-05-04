#ifndef _NETWORK_H_
#define _NETWORK_H_

typedef enum network_state_t
{
    NOT_INITIALISED,
    STARTING_TCPIP,
    TCPIP_DONE,
    WAITING_FOR_DHCP,
    INITIALISED_OK,
    INITIALISED_ERROR
} network_state_t;

void network_task();
network_state_t network_get_state();
uint32_t network_get_ip(char *rxbuf, uint32_t max_len);
uint32_t network_get_gateway(char *rxbuf, uint32_t max_len);
uint32_t network_get_netmask(char *rxbuf, uint32_t max_len);


#endif