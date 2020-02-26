// lwip perf
// to use IDE hack -I into boards.txt
#include "lwip_t41.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/stats.h"

#define swap2 __builtin_bswap16
#define swap4 __builtin_bswap32

uint32_t rtt;

#define PHY_ADDR 0 /*for read/write PHY registers (check link status,...)*/
#define DHCP 0
#define IP "192.168.1.19"
#define MASK "255.255.255.0"
#define GW "192.168.1.1"

// debug stats stuff
extern "C" {
#if LWIP_STATS
  struct stats_ lwip_stats;
#endif
}

void print_stats() {
  // lwip stats_display() needed printf
#if LWIP_STATS
  char str[128];

  // my  LINK stats
  sprintf(str, "LINK in %d out %d drop %d memerr %d",
          lwip_stats.link.recv, lwip_stats.link.xmit, lwip_stats.link.drop, lwip_stats.link.memerr);
  Serial.println(str);
  sprintf(str, "TCP in %d out %d drop %d memerr %d",
          lwip_stats.tcp.recv, lwip_stats.tcp.xmit, lwip_stats.tcp.drop, lwip_stats.tcp.memerr);
  Serial.println(str);
  sprintf(str, "UDP in %d out %d drop %d memerr %d",
          lwip_stats.udp.recv, lwip_stats.udp.xmit, lwip_stats.udp.drop, lwip_stats.udp.memerr);
  Serial.println(str);
  sprintf(str, "ICMP in %d out %d",
          lwip_stats.icmp.recv, lwip_stats.icmp.xmit);
  Serial.println(str);
  sprintf(str, "ARP in %d out %d",
          lwip_stats.etharp.recv, lwip_stats.etharp.xmit);
  Serial.println(str);
#if MEM_STATS
  sprintf(str, "HEAP avail %d used %d max %d err %d",
          lwip_stats.mem.avail, lwip_stats.mem.used, lwip_stats.mem.max, lwip_stats.mem.err);
  Serial.println(str);
#endif
#endif
}

#define PRREG(x) Serial.printf(#x" 0x%x\n",x)

void prregs() {

  PRREG(ENET_PALR);
  PRREG(ENET_PAUR);
  PRREG(ENET_EIR);
  PRREG(ENET_EIMR);
  PRREG(ENET_ECR);
  PRREG(ENET_MSCR);
  PRREG(ENET_MRBR);
  PRREG(ENET_RCR);
  PRREG(ENET_TCR);
  PRREG(ENET_TACC);
  PRREG(ENET_RACC);
  PRREG(ENET_MMFR);
}

static void teensyMAC(uint8_t *mac)
{
  uint32_t m1 = HW_OCOTP_MAC1;
  uint32_t m2 = HW_OCOTP_MAC0;
  mac[0] = m1 >> 8;
  mac[1] = m1 >> 0;
  mac[2] = m2 >> 24;
  mac[3] = m2 >> 16;
  mac[4] = m2 >> 8;
  mac[5] = m2 >> 0;
}


static void netif_status_callback(struct netif *netif)
{
  static char str1[IP4ADDR_STRLEN_MAX], str2[IP4ADDR_STRLEN_MAX], str3[IP4ADDR_STRLEN_MAX];
  Serial.printf("netif status changed: ip %s, mask %s, gw %s\n", ip4addr_ntoa_r(netif_ip_addr4(netif), str1, IP4ADDR_STRLEN_MAX), ip4addr_ntoa_r(netif_ip_netmask4(netif), str2, IP4ADDR_STRLEN_MAX), ip4addr_ntoa_r(netif_ip_gw4(netif), str3, IP4ADDR_STRLEN_MAX));
}

static void link_status_callback(struct netif *netif)
{
  Serial.printf("enet link status: %s\n", netif_is_link_up(netif) ? "up" : "down");
}



// UDP callbacks
//  fancy would be pbuf recv_q per UDP pcb, if q full, drop and free pbuf
static volatile int udp_bytes = 0 , udp_pkts = 0;
static volatile char rx_buffer[256];

// udp recv callback
static void udp_callback(void * arg, struct udp_pcb * upcb, struct pbuf * p, const  ip_addr_t * addr, u16_t port)
{
  if (p == NULL) return;
  udp_bytes =  p->tot_len;
  if(udp_bytes > 0){
    memcpy(rx_buffer, p->payload, p->tot_len);
  }
  udp_pkts = 1;
  pbuf_free(p);
}

static void udp_print_callback(void * arg, struct udp_pcb * upcb, struct pbuf * p, const  ip_addr_t * addr, u16_t port)
{
  if (p == NULL) return;
  udp_bytes +=  p->tot_len;
  udp_pkts++;
  Serial.printf("UDP pkt %d  %d bytes\n", udp_pkts, p->tot_len);
  pbuf_free(p);
}

void udp_echo(int pkts, int bytes) {
  int i, prev = 0;
  struct udp_pcb *pcb;
  pbuf *p;
  uint32_t t, ms;
  ip_addr_t server;

  inet_aton("192.168.1.244", &server);
  pcb = udp_new();
  udp_bind(pcb, IP_ADDR_ANY, 4444);    // local port
  udp_recv(pcb, udp_print_callback, NULL /* *arg */);

  for (i = 0; i < pkts; i++) {
    p = pbuf_alloc(PBUF_TRANSPORT, bytes, PBUF_RAM);
    t = micros();
    udp_sendto(pcb, p, &server, 4444);
    while (udp_bytes <= prev) loop(); // wait for reply
    t = micros() - t;
    prev = udp_bytes;
    pbuf_free(p);
    Serial.print(t); Serial.print(" us  "); Serial.println(udp_bytes);
    ms = millis(); // ether delay
    while (millis() - ms < 2000) loop();
  }

  pbuf_free(p);
  udp_remove(pcb);
}

void udp_sink() {
  int pkts = 0, prev = 0;
  struct udp_pcb *pcb;

  Serial.println("udp sink on port 8888");  //use udpsrc for rate-based send
  pcb = udp_new();
  udp_bind(pcb, IP_ADDR_ANY, 8888);    // local port
  udp_recv(pcb, udp_callback, NULL);  // do once?
  while (1) {
      loop();  // wait for incoming
      if(udp_bytes > 0){
        for(uint8_t ii = 0; ii<udp_bytes; ii++){
          Serial.print(rx_buffer[ii], HEX); Serial.print(", ");
        } Serial.println();
      }
    udp_pkts = udp_bytes = 0;
  }

}

// blast needs to be primed with a few echo pkts, since doesn't poll so ARP will fail
//  use udpsink on desktop
void udp_blast(int pkts, int bytes) {
  int i;
  struct udp_pcb *pcb;
  pbuf *p;
  uint32_t t;
  ip_addr_t server;

  Serial.println("UDP blast");
  delay(1000);
  inet_aton("192.168.1.244", &server);
  pcb = udp_new();
  udp_bind(pcb, IP_ADDR_ANY, 4444);    // local port
  t = micros();
  for (i = 0; i < pkts; i++) {
    p = pbuf_alloc(PBUF_TRANSPORT, bytes, PBUF_RAM);  // ? in the loop
    *(uint32_t *)(p->payload) = swap4(i);  // seq number
    udp_sendto(pcb, p, &server, 2000);
    pbuf_free(p);
    //    delay(100);  // rate limit, need poll/delay ?
  }
  t = micros() - t;
  Serial.println(t);

  pbuf_free(p);
  udp_remove(pcb);
}

void setup()
{
  Serial.begin(9600);
  while (!Serial) delay(100);

  Serial.println(); Serial.print(F_CPU); Serial.print(" ");

  Serial.print(__TIME__); Serial.print(" "); Serial.println(__DATE__);
  Serial.printf("PHY_ADDR %d\n", PHY_ADDR);
  uint8_t mac[6];
  teensyMAC(mac);
  Serial.printf("MAC_ADDR %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.printf("DHCP is %s\n", DHCP == 1 ? "on" : "off");

  ip_addr_t ip, mask, gateway;
  if (DHCP == 1)
  {
    ip = IPADDR4_INIT(IPADDR_ANY);
    mask = IPADDR4_INIT(IPADDR_ANY);
    gateway = IPADDR4_INIT(IPADDR_ANY);
  }
  else
  {
    inet_aton(IP, &ip);
    inet_aton(MASK, &mask);
    inet_aton(GW, &gateway);
  }
  enet_init(PHY_ADDR, mac, &ip, &mask, &gateway);
  netif_set_status_callback(netif_default, netif_status_callback);
  netif_set_link_callback(netif_default, link_status_callback);
  netif_set_up(netif_default);

  if (DHCP == 1)
    dhcp_start(netif_default);

  while (!netif_is_link_up(netif_default)) loop(); // await on link up
  prregs();

  //  pick a test
  //udp_sink();
  udp_echo(10, 8);
  //udp_echo(2, 8); udp_blast(20, 1000); // blast needs echo to run first ?


#if 0
  // optional stats every 5 s, need LWIP_STATS 1 lwipopts.h
  while (1) {
    static uint32_t ms = millis();
    if (millis() - ms > 5000) {
      ms = millis();
      print_stats();
    }
    loop();  // poll
  }
#endif
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
