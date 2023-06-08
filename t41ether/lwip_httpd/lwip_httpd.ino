// stepl's lwIP 2.0.2, for IDE add -I to boards.txt
// https://forum.pjrc.com/threads/45647-k6x-LAN8720(A)-amp-lwip
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include "lwip_t41.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"

#include "lwip/apps/fs.h"
#include "lwip/apps/httpd.h"


#define LOG Serial.printf
#define PHY_ADDR 0 /*for read/write PHY registers (check link status,...)*/
#define DHCP 0
#define IP "192.168.1.19"
#define MASK "255.255.255.0"
#define GW "192.168.1.1"

#pragma region SD


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
static uint32_t nbytes, us;

extern "C" {
  extern int fs_open_custom(struct fs_file *file, const char *name);
  extern void fs_close_custom(struct fs_file *file);
  extern int fs_read_custom(struct fs_file *file, char *buffer, int count);

  int fs_open_custom(struct fs_file *file, const char *name)
  {
    int ret = 0;
    if (file && name)
    {
      File *f = new File();
      if (*f = SD.open(name))
      {
        file->data = NULL;
        file->len = f->size();
        file->index = 0;
        file->pextension = f;
        ret = 1;
      }
      else
        free(f);
    }
    return ret;
  }

  void fs_close_custom(struct fs_file *file)
  {
    if (file && file->pextension)
    {
      File *f = (File*)file->pextension;
      f->close();
      free(f);
      file->pextension = NULL;
    }
  }

  int fs_read_custom(struct fs_file *file, char *buffer, int count)
  {
    int ret;
    if (file && file->pextension && buffer && file->index < file->len)
    {
      File *f = (File*)file->pextension;
      ret = f->read(buffer, count);
      file->index += ret;
    }
    else
      ret = FS_READ_EOF;
    return ret;
  }
}
#pragma endregion


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

  httpd_init();

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
