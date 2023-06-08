// stepl's lwIP 2.0.2, for IDE add -I to boards.txt
// https://forum.pjrc.com/threads/45647-k6x-LAN8720(A)-amp-lwip
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include "lwip_t41.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"

#include "lwip/apps/tftp_server.h"


#define LOG Serial.printf
#define PHY_ADDR 0 /*for read/write PHY registers (check link status,...)*/
#define DHCP 0
#define IP "192.168.1.19"
#define MASK "255.255.255.0"
#define GW "192.168.1.1"

#pragma region SD


File file;

static void dateTime(uint16_t* p_date, uint16_t* p_time)
{
  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  *p_date = FAT_DATE(timeinfo->tm_year + 1900, timeinfo->tm_mon, timeinfo->tm_mday);
  *p_time = FAT_TIME(timeinfo->tm_hour, timeinfo->tm_min, 0);
}

extern "C" {
  extern int _gettimeofday_r(struct _reent *r, struct timeval *__tp, void *__tzp);

  //sd,ftp - set current time for new files, dirs.
  int _gettimeofday_r(struct _reent *r, struct timeval *__tp, void *__tzp)
  {
    //seconds and microseconds since the Epoch
    //__tp->tv_sec = 0;
    //__tp->tv_usec = 0;
    return 0;
  }
}

#pragma endregion

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

#pragma region lwip-httpd


#pragma endregion

#pragma region lwip-tftp

static uint32_t nbytes, us;

void* tftp_fs_open(const char *fname, const char *mode, uint8_t write)
{
  char  *f = "xx";

  nbytes = 0;
  us = micros();
  Serial.printf("opening %s  %d %d\n", fname, write, O_READ);
  if (write == 0) {
    Serial.println("opening for read");
    file = SD.open(fname);
  }
  else file = SD.open(fname, FILE_WRITE);
  if (file) Serial.println("opened");
  else return NULL;

  return f;
}

void tftp_fs_close(void *handle)
{
  us = micros() - us;
  file.close();
  Serial.printf("closed %d bytes %d us\n", nbytes, us);
}

int tftp_fs_read(void *handle, void *buf, int bytes)
{
  int ret = 0;

  //Serial.printf("read avail %d\n", file.available());
  if (file.available()) {
    ret = file.read((uint8_t*)buf, bytes);
    nbytes += ret;
    // Serial.printf("read  %d %d\n", bytes, ret);
  }
  return ret;
}

int tftp_fs_write(void *handle, struct pbuf *p)
{
  int ret;

  nbytes += p->tot_len;
  ret = file.write((char *)(p->payload), p->tot_len);
  return ret;
}

#pragma endregion

void setup()
{
  static const tftp_context tftp_ctx = { tftp_fs_open, tftp_fs_close, tftp_fs_read, tftp_fs_write };

  Serial.begin(115200);
  while (!Serial) delay(100);

  // FatFile::dateTimeCallback(dateTime);

  Serial.print("Initializing SD card...");

  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  LOG("PHY_ADDR %d\n", PHY_ADDR);
  uint8_t mac[6];
  teensyMAC(mac);
  LOG("MAC_ADDR %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  LOG("DHCP is %s\n", DHCP == 1 ? "on" : "off");

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
  enet_init(&ip, &mask, &gateway);
  netif_set_status_callback(netif_default, netif_status_callback);
  netif_set_link_callback(netif_default, link_status_callback);
  netif_set_up(netif_default);

  if (DHCP == 1)
    dhcp_start(netif_default);



  tftp_init(&tftp_ctx);

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
