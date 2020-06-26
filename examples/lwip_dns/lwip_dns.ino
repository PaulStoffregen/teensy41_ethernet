// stepl's lwIP 2.0.2
// https://forum.pjrc.com/threads/45647-k6x-LAN8720(A)-amp-lwip

#include "lwip_t41.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"

#define LOG Serial.printf

#pragma region lwip

static void netif_status_callback(struct netif *netif)
{
  static char str1[IP4ADDR_STRLEN_MAX], str2[IP4ADDR_STRLEN_MAX], str3[IP4ADDR_STRLEN_MAX];
  LOG("netif status changed: ip %s, mask %s, gw %s\n", ip4addr_ntoa_r(netif_ip_addr4(netif), str1, IP4ADDR_STRLEN_MAX), ip4addr_ntoa_r(netif_ip_netmask4(netif), str2, IP4ADDR_STRLEN_MAX), ip4addr_ntoa_r(netif_ip_gw4(netif), str3, IP4ADDR_STRLEN_MAX));
}

static void link_status_callback(struct netif *netif)
{
  LOG("enet link status: %s\n", netif_is_link_up(netif) ? "up" : "down");
}

#pragma endregion

static  ip_addr_t dnsaddr;

static void sntp_dns_found(const char* hostname, const ip_addr_t *ipaddr, void *arg)
{
  LWIP_UNUSED_ARG(hostname);
  LWIP_UNUSED_ARG(arg);

  if (ipaddr != NULL) {
    /* Address resolved, do something */
    //LOG("resolved %s\n", inet_ntoa(*ipaddr));
    dnsaddr = *ipaddr;

  } else {
    /* DNS resolving failed  */
    Serial.println("DNS failed");
  }
}

const char * hosts[] = {"tnlandforms.us", "google.com", NULL};
void do_dns() {
  err_t err;
  uint32_t t;
  static int cnt = 0;

  while (1) {
    LOG("host %s\n", hosts[cnt]);
    ip_addr_set_zero(&dnsaddr);

    err = dns_gethostbyname(hosts[cnt], &dnsaddr, sntp_dns_found, NULL);
    if (err == ERR_INPROGRESS) {
      /* DNS request sent, wait for sntp_dns_found being called */
      while ( dnsaddr.addr == 0) loop();  // timeout ?
      LOG("DNS x %s\n", inet_ntoa(dnsaddr));
    } else if (err == ERR_OK) {
      LOG("DNS ok %s\n", inet_ntoa(dnsaddr));
    } else {
      LOG("DNS status = %d\n", err);
    }
    t = millis();
    while ( millis() - t < 5000); loop(); // ether friendly delay
    cnt++;
    if (hosts[cnt] == NULL) cnt = 0;
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) delay(100);

  enet_init(NULL, NULL, NULL);
  netif_set_status_callback(netif_default, netif_status_callback);
  netif_set_link_callback(netif_default, link_status_callback);
  netif_set_up(netif_default);

  dhcp_start(netif_default);

  while (!netif_is_link_up(netif_default)) loop(); // await on link up

  do_dns();
}

void loop()
{
  static uint32_t last_ms;
  uint32_t ms;

  enet_proc_input();

  ms = millis();
  if (ms - last_ms > 100)
  {
    last_ms = ms;
    enet_poll();
  }
}
