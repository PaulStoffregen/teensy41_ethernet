// stepl's lwIP 2.0.2
// https://forum.pjrc.com/threads/45647-k6x-LAN8720(A)-amp-lwip
//  ftpd with SD lib   TODO
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include "lwip_t41.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"

#include "lwip_ftpd-sd.h"


#define LOG Serial.printf

File *file;



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

static void netif_status_callback(struct netif *netif)
{
  static char str1[IP4ADDR_STRLEN_MAX], str2[IP4ADDR_STRLEN_MAX], str3[IP4ADDR_STRLEN_MAX];
  LOG("netif status changed: ip %s, mask %s, gw %s\n", ip4addr_ntoa_r(netif_ip_addr4(netif), str1, IP4ADDR_STRLEN_MAX), ip4addr_ntoa_r(netif_ip_netmask4(netif), str2, IP4ADDR_STRLEN_MAX), ip4addr_ntoa_r(netif_ip_gw4(netif), str3, IP4ADDR_STRLEN_MAX));
}

static void link_status_callback(struct netif *netif)
{
  LOG("enet link status: %s\n", netif_is_link_up(netif) ? "up" : "down");
}





void setup()
{
  Serial.begin(115200);
  while (!Serial) delay(100);

  // FatFile::dateTimeCallback(dateTime);

  Serial.print("Initializing SD card...");

  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  enet_init(NULL, NULL, NULL);
  netif_set_status_callback(netif_default, netif_status_callback);
  netif_set_link_callback(netif_default, link_status_callback);
  netif_set_up(netif_default);

  dhcp_start(netif_default);

  ftpd_init(&SD);
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
