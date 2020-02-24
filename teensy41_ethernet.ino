#include "IPAddress.h"

// set this to an unused IP number for your network
IPAddress myaddress(192, 168, 194, 67);

#define MACADDR1 0x04E9E5
#define MACADDR2 0x000001

// This test program prints a *lot* of info to the Arduino Serial Monitor
// Ping response time is approx 1.3 ms with 180 MHz clock, due to all the
// time spent printing.  To get a realistic idea of ping time, you would
// need to delete or comment out all the Serial print stuff.

#define EXTDESC

typedef struct {
	uint16_t length;
	uint16_t flags;
	void *buffer;
#ifdef EXTDESC
	uint32_t moreflags;
	uint16_t checksum;
	uint16_t header;
	uint32_t dmadone;
	uint32_t timestamp;
	uint32_t unused1;
	uint32_t unused2;
#endif
} enetbufferdesc_t;

#define RXSIZE 12
#define TXSIZE 10
static enetbufferdesc_t rx_ring[RXSIZE] __attribute__ ((aligned(64)));
static enetbufferdesc_t tx_ring[TXSIZE] __attribute__ ((aligned(64)));
uint32_t rxbufs[RXSIZE*128] __attribute__ ((aligned(32)));
uint32_t txbufs[TXSIZE*128] __attribute__ ((aligned(32)));


#define CLRSET(reg, clear, set) ((reg) = ((reg) & ~(clear)) | (set))
#define RMII_PAD_INPUT_PULLDOWN 0x30E9
#define RMII_PAD_INPUT_PULLUP   0xB0E9
#define RMII_PAD_CLOCK          0x0031

// initialize the ethernet hardware
void setup()
{
	while (!Serial) ; // wait
	print("Ethernet Testing");
	print("----------------\n");

	CCM_CCGR1 |= CCM_CCGR1_ENET(CCM_CCGR_ON);
	// configure PLL6 for 50 MHz, pg 1173
	CCM_ANALOG_PLL_ENET_CLR = CCM_ANALOG_PLL_ENET_POWERDOWN
		| CCM_ANALOG_PLL_ENET_BYPASS | 0x0F;
	CCM_ANALOG_PLL_ENET_SET = CCM_ANALOG_PLL_ENET_ENABLE | CCM_ANALOG_PLL_ENET_BYPASS
		/*| CCM_ANALOG_PLL_ENET_ENET2_REF_EN*/ | CCM_ANALOG_PLL_ENET_ENET_25M_REF_EN
		/*| CCM_ANALOG_PLL_ENET_ENET2_DIV_SELECT(1)*/ | CCM_ANALOG_PLL_ENET_DIV_SELECT(1);
	while (!(CCM_ANALOG_PLL_ENET & CCM_ANALOG_PLL_ENET_LOCK)) ; // wait for PLL lock
	CCM_ANALOG_PLL_ENET_CLR = CCM_ANALOG_PLL_ENET_BYPASS;
	Serial.printf("PLL6 = %08X (should be 80202001)\n", CCM_ANALOG_PLL_ENET);
	// configure REFCLK to be driven as output by PLL6, pg 326
#if 1
	CLRSET(IOMUXC_GPR_GPR1, IOMUXC_GPR_GPR1_ENET1_CLK_SEL | IOMUXC_GPR_GPR1_ENET_IPG_CLK_S_EN,
		IOMUXC_GPR_GPR1_ENET1_TX_CLK_DIR);
#else
	//IOMUXC_GPR_GPR1 &= ~IOMUXC_GPR_GPR1_ENET1_TX_CLK_DIR; // do not use
	IOMUXC_GPR_GPR1 |= IOMUXC_GPR_GPR1_ENET1_TX_CLK_DIR; // 50 MHz REFCLK
	IOMUXC_GPR_GPR1 &= ~IOMUXC_GPR_GPR1_ENET_IPG_CLK_S_EN;
	//IOMUXC_GPR_GPR1 |= IOMUXC_GPR_GPR1_ENET_IPG_CLK_S_EN; // clock always on
	IOMUXC_GPR_GPR1 &= ~IOMUXC_GPR_GPR1_ENET1_CLK_SEL;
	////IOMUXC_GPR_GPR1 |= IOMUXC_GPR_GPR1_ENET1_CLK_SEL;
#endif
	Serial.printf("GPR1 = %08X\n", IOMUXC_GPR_GPR1);

	// configure pins
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_14 = 5; // Reset   B0_14 Alt5 GPIO7.15
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_15 = 5; // Power   B0_15 Alt5 GPIO7.14
	GPIO7_GDIR |= (1<<14) | (1<<15);
	GPIO7_DR_SET = (1<<15);   // power on
	GPIO7_DR_CLEAR = (1<<14); // reset PHY chip
	IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_04 = RMII_PAD_INPUT_PULLDOWN; // PhyAdd[0] = 0
	IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_06 = RMII_PAD_INPUT_PULLDOWN; // PhyAdd[1] = 1
	IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_05 = RMII_PAD_INPUT_PULLUP;   // Master/Slave = slave mode
	IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_11 = RMII_PAD_INPUT_PULLDOWN; // Auto MDIX Enable
	IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_07 = RMII_PAD_INPUT_PULLUP;
	IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_08 = RMII_PAD_INPUT_PULLUP;
	IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_09 = RMII_PAD_INPUT_PULLUP;
	IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_10 = RMII_PAD_CLOCK;
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_05 = 3; // RXD1    B1_05 Alt3, pg 525
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_04 = 3; // RXD0    B1_04 Alt3, pg 524
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_10 = 6 | 0x10; // REFCLK  B1_10 Alt6, pg 530
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_11 = 3; // RXER    B1_11 Alt3, pg 531
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_06 = 3; // RXEN    B1_06 Alt3, pg 526
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_09 = 3; // TXEN    B1_09 Alt3, pg 529
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_07 = 3; // TXD0    B1_07 Alt3, pg 527
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_08 = 3; // TXD1    B1_08 Alt3, pg 528
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_15 = 0; // MDIO    B1_15 Alt0, pg 535
	IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_14 = 0; // MDC     B1_14 Alt0, pg 534
	IOMUXC_ENET_MDIO_SELECT_INPUT = 2; // GPIO_B1_15_ALT0, pg 792
	IOMUXC_ENET0_RXDATA_SELECT_INPUT = 1; // GPIO_B1_04_ALT3, pg 792
	IOMUXC_ENET1_RXDATA_SELECT_INPUT = 1; // GPIO_B1_05_ALT3, pg 793
	IOMUXC_ENET_RXEN_SELECT_INPUT = 1; // GPIO_B1_06_ALT3, pg 794
	IOMUXC_ENET_RXERR_SELECT_INPUT = 1; // GPIO_B1_11_ALT3, pg 795
	IOMUXC_ENET_IPG_CLK_RMII_SELECT_INPUT = 1; // GPIO_B1_10_ALT6, pg 791
	delayMicroseconds(2);
	GPIO7_DR_SET = (1<<14); // start PHY chip
	ENET_MSCR = ENET_MSCR_MII_SPEED(9);
	delayMicroseconds(5);
#if 0
	while (1) {
		mdio_write(0, 0x18, 0x492); // force LED on
		delay(500);
		mdio_write(0, 0x18, 0x490); // force LED off
		delay(500);
	}
#endif
	Serial.printf("RCSR:%04X, LEDCR:%04X, PHYCR %04X\n",
		mdio_read(0, 0x17), mdio_read(0, 0x18), mdio_read(0, 0x19));

	// LEDCR offset 0x18, set LED_Link_Polarity, pg 62
	mdio_write(0, 0x18, 0x0280); // LED shows link status, active high
	// RCSR offset 0x17, set RMII_Clock_Select, pg 61
	mdio_write(0, 0x17, 0x0081); // config for 50 MHz clock input

	Serial.printf("RCSR:%04X, LEDCR:%04X, PHYCR %04X\n",
		mdio_read(0, 0x17), mdio_read(0, 0x18), mdio_read(0, 0x19));

	// ENET_EIR	2174	Interrupt Event Register
	// ENET_EIMR	2177	Interrupt Mask Register
	// ENET_RDAR	2180	Receive Descriptor Active Register
	// ENET_TDAR	2181	Transmit Descriptor Active Register
	// ENET_ECR	2181	Ethernet Control Register
	// ENET_RCR	2187	Receive Control Register
	// ENET_TCR	2190	Transmit Control Register
	// ENET_PALR/UR	2192	Physical Address
	// ENET_RDSR	2199	Receive Descriptor Ring Start
	// ENET_TDSR	2199	Transmit Buffer Descriptor Ring
	// ENET_MRBR	2200	Maximum Receive Buffer Size
	//		2278	receive buffer descriptor
	//		2281	transmit buffer descriptor

	print("enetbufferdesc_t size = ", sizeof(enetbufferdesc_t));
	print("rx_ring size = ", sizeof(rx_ring));
	memset(rx_ring, 0, sizeof(rx_ring));
	memset(tx_ring, 0, sizeof(tx_ring));

	for (int i=0; i < RXSIZE; i++) {
		rx_ring[i].flags = 0x8000; // empty flag
		#ifdef EXTDESC
		rx_ring[i].moreflags = 0x00800000; // INT flag
		#endif
		rx_ring[i].buffer = rxbufs + i * 128;
	}
	rx_ring[RXSIZE-1].flags = 0xA000; // empty & wrap flags
	for (int i=0; i < TXSIZE; i++) {
		tx_ring[i].buffer = txbufs + i * 128;
		#ifdef EXTDESC
		tx_ring[i].moreflags = 0x40000000; // INT flag
		#endif
	}
	tx_ring[TXSIZE-1].flags = 0x2000; // wrap flag

	//ENET_ECR |= ENET_ECR_RESET;

	ENET_EIMR = 0;
	ENET_MSCR = ENET_MSCR_MII_SPEED(9);  // 12 is fastest which seems to work
#if 1
	ENET_RCR = ENET_RCR_NLC | ENET_RCR_MAX_FL(1522) | /* ENET_RCR_CFEN | */
		ENET_RCR_CRCFWD | ENET_RCR_PADEN | ENET_RCR_RMII_MODE |
		///* ENET_RCR_FCE | ENET_RCR_PROM | */ ENET_RCR_MII_MODE;
		ENET_RCR_PROM | ENET_RCR_MII_MODE;
	ENET_TCR = ENET_TCR_ADDINS | /* ENET_TCR_RFC_PAUSE | ENET_TCR_TFC_PAUSE | */
		ENET_TCR_FDEN;
#else
	ENET_RCR = ENET_RCR_MAX_FL(1518) | ENET_RCR_RMII_MODE | ENET_RCR_MII_MODE
		| ENET_RCR_PROM;
	ENET_TCR = ENET_TCR_FDEN;
#endif
	ENET_RXIC = 0;
	ENET_TXIC = 0;
	
	ENET_PALR = (MACADDR1 << 8) | ((MACADDR2 >> 16) & 255);
	ENET_PAUR = ((MACADDR2 << 16) & 0xFFFF0000) | 0x8808;
	ENET_OPD = 0x10014;
	ENET_IAUR = 0;
	ENET_IALR = 0;
	ENET_GAUR = 0;
	ENET_GALR = 0;
	ENET_RDSR = (uint32_t)rx_ring;
	ENET_TDSR = (uint32_t)tx_ring;
	ENET_MRBR = 512;
	ENET_TACC = ENET_TACC_SHIFT16;
	//ENET_TACC = ENET_TACC_SHIFT16 | ENET_TACC_IPCHK | ENET_TACC_PROCHK;
	ENET_RACC = ENET_RACC_SHIFT16;

	//ENET_RSEM = 0;
	//ENET_RAEM = 16;
	//ENET_RAFL = 16;
	//ENET_TSEM = 16;
	//ENET_TAEM = 16;

	ENET_MIBC = 0;
	Serial.printf("MIBC=%08X\n", ENET_MIBC);
	Serial.printf("ECR=%08X\n", ENET_ECR);
	//ENET_ECR = 0x70000000 | ENET_ECR_DBSWP | ENET_ECR_EN1588 | ENET_ECR_ETHEREN;
#ifdef EXTDESC
	ENET_ECR |= ENET_ECR_DBSWP | ENET_ECR_EN1588 | ENET_ECR_ETHEREN;
#else
	ENET_ECR |= ENET_ECR_DBSWP | ENET_ECR_ETHEREN;
#endif
	//ENET_ECR = 0xF0000000 | ENET_ECR_DBSWP | ENET_ECR_EN1588 | ENET_ECR_ETHEREN;
	Serial.printf("ECR=%08X\n", ENET_ECR);
	ENET_RDAR = ENET_RDAR_RDAR;
	ENET_TDAR = ENET_TDAR_TDAR;

	printhex("MDIO PHY ID2 (LAN8720A is 0007, DP83825I is 2000): ", mdio_read(0, 2));
	printhex("MDIO PHY ID3 (LAN8720A is C0F?, DP83825I is A140): ", mdio_read(0, 3));
	delay(2500);
	printhex("BMCR: ", mdio_read(0, 0));
	printhex("BMSR: ", mdio_read(0, 1));
}

elapsedMillis msec;

// watch for data to arrive
void loop()
{
	static uint32_t rx_packet_count=0;
	static int rxnum=0;
	volatile enetbufferdesc_t *buf;

	buf = rx_ring + rxnum;

	if ((buf->flags & 0x8000) == 0) {
		incoming(buf->buffer, buf->length);
		if (rxnum < RXSIZE-1) {
			buf->flags = 0x8000;
			rxnum++;
		} else {
			buf->flags = 0xA000;
			rxnum = 0;
		}
	}
	if (!(ENET_RDAR & ENET_RDAR_RDAR)) {
		print("receiver not active\n");
	}
	uint32_t n = ENET_RMON_R_PACKETS;
	if (n != rx_packet_count) {
		rx_packet_count = n;
		Serial.printf("rx packets: %u\n", n);
	}
	if (msec > 1000) {
		msec = 0;
		Serial.printf("EIR=%08X, len=%d, R=%X\n", ENET_EIR, rx_ring[0].length, ENET_RMON_R_OCTETS);
	}
	// TODO: if too many packets arrive too quickly, which is
	// a distinct possibility when we spend so much time printing
	// to the serial monitor, ENET_RDAR_RDAR can be cleared if
	// the receive ring buffer fills up.  After we free up space,
	// ENET_RDAR_RDAR needs to be set again to restart reception
	// of incoming packets.
}

// when we get data, try to parse it
void incoming(void *packet, unsigned int len)
{
	const uint8_t *p8;
	const uint16_t *p16;
	const uint32_t *p32;
	IPAddress src, dst;
	uint16_t type;

	Serial.println();
	print("data, len=", len);
	p8 = (const uint8_t *)packet + 2;
	p16 = (const uint16_t *)p8;
	p32 = (const uint32_t *)packet;
	type = p16[6];
	if (type == 0x0008) {
		src = p32[7];
		dst = p32[8];
		Serial.print("IPv4 Packet, src=");
		Serial.print(src);
		Serial.print(", dst=");
		Serial.print(dst);
		Serial.println();
		printpacket(p8, len - 2);
		if (p8[23] == 1 && dst == myaddress) {
			Serial.println("  Protocol is ICMP:");
			if (p8[34] == 8) {
				print("  echo request:");
				uint16_t id = __builtin_bswap16(p16[19]);
				uint16_t seqnum = __builtin_bswap16(p16[20]);
				printhex("   id = ", id);
				print("   sequence number = ", seqnum);
				ping_reply((uint32_t *)packet, len);
			}
		}
	} else if (type == 0x0608) {
		Serial.println("ARP Packet:");
		printpacket(p8, len - 2);
		if (p32[4] == 0x00080100 && p32[5] == 0x01000406) {
			// request is for IPv4 address of ethernet mac
			IPAddress from((p16[15] << 16) | p16[14]);
			IPAddress to(p32[10]);
			Serial.print("  Who is ");
			Serial.print(to);
			Serial.print(" from ");
			Serial.print(from);
			Serial.print(" (");
			printmac(p8 + 22);
			Serial.println(")");
			if (to == myaddress) {
				arp_reply(p8+22, from);
			}
		}
	}
}

// compose an answer to ARP requests
void arp_reply(const uint8_t *mac, IPAddress &ip)
{
	uint32_t packet[11]; // 42 bytes needed + 2 pad
	uint8_t *p = (uint8_t *)packet + 2;

	packet[0] = 0;       // first 2 bytes are padding
	memcpy(p, mac, 6);
	memset(p + 6, 0, 6); // hardware automatically adds our mac addr
	//p[6] = (MACADDR1 >> 16) & 255;
	//p[7] = (MACADDR1 >> 8) & 255;
	//p[8] = (MACADDR1) & 255;
	//p[9] = (MACADDR2 >> 16) & 255; // this is how to do it the hard way
	//p[10] = (MACADDR2 >> 8) & 255;
	//p[11] = (MACADDR2) & 255;
	p[12] = 8;
	p[13] = 6;  // arp protocol
	packet[4] = 0x00080100; // IPv4 on ethernet
	packet[5] = 0x02000406; // reply, ip 4 byte, macaddr 6 bytes
	packet[6] = (__builtin_bswap32(MACADDR1) >> 8) | ((MACADDR2 << 8) & 0xFF000000);
	packet[7] = __builtin_bswap16(MACADDR2 & 0xFFFF) | ((uint32_t)myaddress << 16);
	packet[8] = (((uint32_t)myaddress & 0xFFFF0000) >> 16) | (mac[0] << 16) | (mac[1] << 24);
	packet[9] = (mac[5] << 24) | (mac[4] << 16) | (mac[3] << 8) | mac[2];
	packet[10] = (uint32_t)ip;
	Serial.println("ARP Reply:");
	printpacket(p, 42);
	outgoing(packet, 44);
}

// compose an reply to pings
void ping_reply(const uint32_t *recv, unsigned int len)
{
	uint32_t packet[32];
	uint8_t *p8 = (uint8_t *)packet + 2;

	if (len > sizeof(packet)) return;
	memcpy(packet, recv, len);
	memcpy(p8, p8 + 6, 6); // send to the mac address we received
	// hardware automatically adds our mac addr
	packet[8] = packet[7]; // send to the IP number we received
	packet[7] = (uint32_t)myaddress;
	p8[34] = 0;            // type = echo reply
	// TODO: checksums in IP and ICMP headers - is the hardware
	// really inserting correct checksums automatically?
	printpacket((uint8_t *)packet + 2, len - 2);
	outgoing(packet, len);
}

// transmit a packet
void outgoing(void *packet, unsigned int len)
{
	static int txnum=0;
	volatile enetbufferdesc_t *buf;
	uint16_t flags;

	buf = tx_ring + txnum;
	flags = buf->flags;
	if ((flags & 0x8000) == 0) {
		print("tx, num=", txnum);
		buf->length = len;
		memcpy(buf->buffer, packet, len);
		buf->flags = flags | 0x8C00;
		ENET_TDAR = ENET_TDAR_TDAR;
		if (txnum < TXSIZE-1) {
			txnum++;
		} else {
			txnum = 0;
		}
	}
}

// read a PHY register (using MDIO & MDC signals)
uint16_t mdio_read(int phyaddr, int regaddr)
{
	ENET_MMFR = ENET_MMFR_ST(1) | ENET_MMFR_OP(2) | ENET_MMFR_TA(0)
		| ENET_MMFR_PA(phyaddr) | ENET_MMFR_RA(regaddr);
	// TODO: what is the proper value for ENET_MMFR_TA ???
	//int count=0;
	while ((ENET_EIR & ENET_EIR_MII) == 0) {
		//count++; // wait
	}
	//print("mdio read waited ", count);
	uint16_t data = ENET_MMFR;
	ENET_EIR = ENET_EIR_MII;
	//printhex("mdio read:", data);
	return data;
}

// write a PHY register (using MDIO & MDC signals)
void mdio_write(int phyaddr, int regaddr, uint16_t data)
{
	ENET_MMFR = ENET_MMFR_ST(1) | ENET_MMFR_OP(1) | ENET_MMFR_TA(0)
		| ENET_MMFR_PA(phyaddr) | ENET_MMFR_RA(regaddr) | ENET_MMFR_DATA(data);
	// TODO: what is the proper value for ENET_MMFR_TA ???
	int count=0;
	while ((ENET_EIR & ENET_EIR_MII) == 0) {
		count++; // wait
	}
	ENET_EIR = ENET_EIR_MII;
	//print("mdio write waited ", count);
	//printhex("mdio write :", data);
}


// misc print functions, for lots of info in the serial monitor.
// this stuff probably slows things down and would need to go
// for any hope of keeping up with full ethernet data rate!

void print(const char *s)
{
	Serial.println(s);
}

void print(const char *s, int num)
{
	Serial.print(s);
	Serial.println(num);
}

void printhex(const char *s, int num)
{
	Serial.print(s);
	Serial.println(num, HEX);
}

void printmac(const uint8_t *data)
{
	Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
		data[0], data[1], data[2], data[3], data[4], data[5]);
}

void printpacket(const uint8_t *data, unsigned int len)
{
#if 1
	unsigned int i;

	for (i=0; i < len; i++) {
		Serial.printf(" %02X", *data++);
		if ((i & 15) == 15) Serial.println();
	}
	Serial.println();
#endif
}


