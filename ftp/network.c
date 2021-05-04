#include <stdio.h>
#include "lwip/debug.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "pktdrv.h"
#include <xboxkrnl/xboxkrnl.h>
#include <hal/xbox.h>
#include <hal/debug.h>
#include "network.h"

#define USE_DHCP 1
#define PKT_TMR_INTERVAL 5 /* ms */

static const ip4_addr_t *ip;
static ip4_addr_t ipaddr, netmask, gw;
static network_state_t network_state = NOT_INITIALISED;
struct netif nforce_netif, *g_pnetif;

err_t nforceif_init(struct netif *netif);
void http_server_netconn_init(void);
static void packet_timer(void *arg);

static void tcpip_init_done(void *arg)
{
    sys_sem_t *init_complete = (sys_sem_t *)arg;
    sys_sem_signal(init_complete);
    network_state = TCPIP_DONE;
}

static void packet_timer(void *arg)
{
    LWIP_UNUSED_ARG(arg);
    Pktdrv_ReceivePackets();
    sys_timeout(PKT_TMR_INTERVAL, packet_timer, NULL);
}

void network_task()
{
    static sys_sem_t init_complete;
    if (network_state == INITIALISED_OK)
    {
        return;
    }
    else if (network_state == NOT_INITIALISED)
    {
        IP4_ADDR(&gw, 0, 0, 0, 0);
        IP4_ADDR(&ipaddr, 0, 0, 0, 0);
        IP4_ADDR(&netmask, 0, 0, 0, 0);
        sys_sem_new(&init_complete, 0);
        tcpip_init(tcpip_init_done, &init_complete);
        network_state = STARTING_TCPIP;
    }
    else if (network_state == STARTING_TCPIP)
    {
        return;
    }
    else if (network_state == TCPIP_DONE)
    {
        sys_sem_free(&init_complete);
        g_pnetif = netif_add(&nforce_netif, &ipaddr, &netmask, &gw,
                             NULL, nforceif_init, ethernet_input);
        if (!g_pnetif)
        {
            network_state = INITIALISED_ERROR;
            return;
        }
        netif_set_default(g_pnetif);
        netif_set_up(g_pnetif);
        dhcp_start(g_pnetif);
        packet_timer(NULL);
        network_state = WAITING_FOR_DHCP;
    }
    else if (network_state == WAITING_FOR_DHCP)
    {
        if (dhcp_supplied_address(g_pnetif) != 0)
            network_state = INITIALISED_OK;
    }
}

network_state_t network_get_state()
{
    return network_state;
}

uint32_t network_get_ip(char *rxbuf, uint32_t max_len)
{
    const char *no_ip = "0.0.0.0";
    if (network_state != INITIALISED_OK || !netif_is_up(g_pnetif) || !netif_is_link_up(g_pnetif))
    {
        strncpy(rxbuf, no_ip, max_len);
        return 0;
    }

    ip4addr_ntoa_r(netif_ip4_addr(g_pnetif), rxbuf, max_len);
    return 1;
}

uint32_t network_get_gateway(char *rxbuf, uint32_t max_len)
{
    if (network_state != INITIALISED_OK)
        return 0;

    uint32_t gw = ip4_addr_get_u32(netif_ip4_gw(g_pnetif));
    snprintf(rxbuf, max_len, "%d.%d.%d.%d\n", (gw & 0xff), ((gw >> 8) & 0xff), ((gw >> 16) & 0xff), (gw >> 24));
    return 1;
}

uint32_t network_get_netmask(char *rxbuf, uint32_t max_len)
{
    if (network_state != INITIALISED_OK)
        return 0;

    uint32_t nm = ip4_addr_get_u32(netif_ip4_netmask(g_pnetif));
    snprintf(rxbuf, max_len, "%d.%d.%d.%d\n\r", (nm & 0xff), ((nm >> 8) & 0xff), ((nm >> 16) & 0xff), (nm >> 24));
    return 1;
}
