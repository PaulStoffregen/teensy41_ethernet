    Teensy 4.1 ethernet :: https://forum.pjrc.com/threads/59776-Ethernet-and-lwip

etherraw/   UDP additions to Paul's low-level ethernet sketch, hand-crafted UDP
            packets

lwip/            put this in your sketchbook/libraries/
lwip-ftpd-sd/    put this in your sketchbook/libraries/  for ftpd

lwip sketches
  lwip_dns/         demonstrate DNS queries
  lwip_echosrv/     TCP and UDP echo servers on port 7
  lwip_ftpd/        lwIP ftpd server (get/put) using SD lib and lwip-ftpd-sd lib
  lwip_httpd/       lwIP web server using SD lib and microSD
  lwip_httpd_sdfat/ lwIP web server using SdFat-beta v2 and microSD
  lwip_iperf/       TCP iperf(v2) server on port 5001
  lwip_mcast/       multicast listener, does one chirp
  lwip_perf/        various UDP/TCP client/server tests
  lwip_sntp/        NTP (UDP) poll of server (default 10 minutes)
  lwip_tftpd/       tftp server (UDP) using SD lib and microSD
  lwip_tftpd_sdfat/ tftp server (UDP) using SdFat-beta v2 and microSD
  lwip_tftpd_SPIFFS/  tftp server (UDP) using SPIFFS lib EFLASH
  lwip_webclnt/     send http GET to a web server
  lwip_websrv/      serve up embedded html and manipulate LED


To build lwip apps in IDE you must add an include path to boards.txt
 teensy41.build.flags.common=-g -Wall -ffunction-sections -fdata-sections -nostdlib -I/home/dunigan/sketchbook/libraries/lwip/src/include
 
 And for Windows
 teensy41.build.flags.common=-g -Wall -ffunction-sections -fdata-sections -nostdlib -IT:\tCode\libraries\lwip\src\include

The lwip lib (2.0.2) and apps are adapted from the 2016 T3.6 beta test of 
limited-production ethernet shield. T41 lwIP configured to use 64-byte aligned
5 transmit descriptors and 5 receive descriptors with 32-byte aligned packet
buffers (1536 bytes in DTCM). See
https://forum.pjrc.com/threads/34808-K66-Beta-Test?p=109161&viewfull=1#post109161


TODO:
  -update lwIP

--------------------------------------------------------------------

                         Ethernet performance
                   T41e   1062SDK  T41USBe  T35e  T4+W5500    info
TCP xmit (mbs)       73        87      78     59         9 
TCP recv (mbs)       93        71      30     51        11  

UDP xmit (mbs)       97        97      95     85        11   blast 20 1000-byte pkts
UDP xmit (pps)   149476    137453   32331  66534     21514   blast 1000 8-byte pkts
UDP recv (mbs)       91        95      40     67         9   no-loss recv of 20 1000-byte pkts
UDP RTT (us)         94       104    1651    183       150   RTT latency of 8-byte pkts

ping RTT (us)       120       108    2000    127        82 

ePower (ma)          59       100     174    100       132  ethernet module current

  tests on 100mbs full-duplex Ether with linux box on switch
  W5500 SPI @37.5MHz, 2KB buffers

  microSD read times (seconds)  (4.2MB file, 2KB reads)
      T4 SD lib  T3.5 SdFatv1  T4.1 SdFatv2
tftp    4.76        2.2          2.2 s
http    3.6         0.9          0.9
ftp     2.8         0.6  
read()  2.76        0.26         0.19 s
-----------------------------------------------------------------------
References:
https://forum.pjrc.com/threads/60532-Teensy-4-1-Beta-Test?p=237096&viewfull=1#post237096
https://forum.pjrc.com/threads/59776-Ethernet-and-lwip?p=230993&viewfull=1#post230993  private 
https://forum.pjrc.com/threads/34808-K66-Beta-Test?p=109161&viewfull=1#post109161
https://forum.pjrc.com/threads/57701-USB-Host-Ethernet-Driver?p=218866&viewfull=1#post218866
https://forum.pjrc.com/threads/54265-Teensy-4-testing-mbed-NXP-MXRT1050-EVKB-(600-Mhz-M7)
https://github.com/manitou48/DUEZoo/blob/master/wizperf.txt
