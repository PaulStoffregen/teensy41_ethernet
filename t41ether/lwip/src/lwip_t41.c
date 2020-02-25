#if defined(ARDUINO_TEENSY41)

#include "lwip_t41.h"
#include "lwipopts.h"
#include "lwip/init.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "lwip/timeouts.h"

#include "core_pins.h"

#define CLRSET(reg, clear, set) ((reg) = ((reg) & ~(clear)) | (set))
#define RMII_PAD_INPUT_PULLDOWN 0x30E9
#define RMII_PAD_INPUT_PULLUP   0xB0E9
#define RMII_PAD_CLOCK          0x0031

#define RX_SIZE 5
#define TX_SIZE 5
#define BFSIZE 1536
#define IRQ_PRIORITY 64

/*! @brief Defines the control and status region of the receive buffer descriptor.*/
typedef enum _enet_rx_bd_control_status
{
    kEnetRxBdEmpty = 0x8000U,    /*!< Empty bit*/
    kEnetRxBdRxSoftOwner1 = 0x4000U,    /*!< Receive software owner*/
    kEnetRxBdWrap = 0x2000U,    /*!< Update buffer descriptor*/
    kEnetRxBdRxSoftOwner2 = 0x1000U,    /*!< Receive software owner*/
    kEnetRxBdLast = 0x0800U,    /*!< Last BD in the frame*/
    kEnetRxBdMiss = 0x0100U,    /*!< Receive for promiscuous mode*/
    kEnetRxBdBroadCast = 0x0080U,    /*!< Broadcast */
    kEnetRxBdMultiCast = 0x0040U,    /*!< Multicast*/
    kEnetRxBdLengthViolation = 0x0020U,    /*!< Receive length violation*/
    kEnetRxBdNoOctet = 0x0010U,    /*!< Receive non-octet aligned frame*/
    kEnetRxBdCrc = 0x0004U,    /*!< Receive CRC error*/
    kEnetRxBdOverRun = 0x0002U,    /*!< Receive FIFO overrun*/
    kEnetRxBdTrunc = 0x0001U     /*!< Frame is truncated    */
} enet_rx_bd_control_status_t;

/*! @brief Defines the control extended region1 of the receive buffer descriptor.*/
typedef enum _enet_rx_bd_control_extend0
{
    kEnetRxBdIpv4 = 0x0001U,  /*!< Ipv4 frame*/
    kEnetRxBdIpv6 = 0x0002U,  /*!< Ipv6 frame*/
    kEnetRxBdVlan = 0x0004U,  /*!< VLAN*/
    kEnetRxBdProtocolChecksumErr = 0x0010U,  /*!< Protocol checksum error*/
    kEnetRxBdIpHeaderChecksumErr = 0x0020U,  /*!< IP header checksum error*/
} enet_rx_bd_control_extend0_t;

/*! @brief Defines the control extended region2 of the receive buffer descriptor.*/
typedef enum _enet_rx_bd_control_extend1
{
    kEnetRxBdIntrrupt = 0x0080U,           /*!< BD interrupt*/
    kEnetRxBdUnicast = 0x0100U,           /*!< Unicast frame*/
    kEnetRxBdCollision = 0x0200U,           /*!< BD collision*/
    kEnetRxBdPhyErr = 0x0400U,           /*!< PHY error*/
    kEnetRxBdMacErr = 0x8000U            /*!< Mac error */
} enet_rx_bd_control_extend1_t;

/*! @brief Defines the control status of the transmit buffer descriptor.*/
typedef enum _enet_tx_bd_control_status
{
    kEnetTxBdReady = 0x8000U,  /*!<  Ready bit*/
    kEnetTxBdTxSoftOwner1 = 0x4000U,  /*!<  Transmit software owner*/
    kEnetTxBdWrap = 0x2000U,  /*!<  Wrap buffer descriptor*/
    kEnetTxBdTxSoftOwner2 = 0x1000U,  /*!<  Transmit software owner*/
    kEnetTxBdLast = 0x0800U,  /*!<  Last BD in the frame*/
    kEnetTxBdTransmitCrc = 0x0400U   /*!<  Receive for transmit CRC   */
} enet_tx_bd_control_status_t;

/*! @brief Defines the control extended region1 of the transmit buffer descriptor.*/
typedef enum _enet_tx_bd_control_extend0
{
    kEnetTxBdTxErr = 0x8000U,      /*!<  Transmit error*/
    kEnetTxBdTxUnderFlowErr = 0x2000U,      /*!<  Underflow error*/
    kEnetTxBdExcessCollisionErr = 0x1000U,      /*!<  Excess collision error*/
    kEnetTxBdTxFrameErr = 0x0800U,      /*!<  Frame error*/
    kEnetTxBdLatecollisionErr = 0x0400U,      /*!<  Late collision error*/
    kEnetTxBdOverFlowErr = 0x0200U,      /*!<  Overflow error*/
    kEnetTxTimestampErr = 0x0100U       /*!<  Timestamp error*/
} enet_tx_bd_control_extend0_t;

/*! @brief Defines the control extended region2 of the transmit buffer descriptor.*/
typedef enum _enet_tx_bd_control_extend1
{
    kEnetTxBdTxInterrupt = 0x4000U,   /*!< Transmit interrupt*/
    kEnetTxBdTimeStamp = 0x2000U,    /*!< Transmit timestamp flag */
    kEnetTxBdProtChecksum = 0x1000U,   /*!< Insert protocol specific checksum*/
    kEnetTxBdIpHdrChecksum = 0x0800U   /*!< Insert IP header checksum*/
} enet_tx_bd_control_extend1_t;

typedef struct
{
    uint16_t length;
    uint16_t status;
    void *buffer;
    uint16_t extend0;
    uint16_t extend1;
    uint16_t checksum;
    uint8_t prototype;
    uint8_t headerlen;
    uint16_t unused0;
    uint16_t extend2;
    uint32_t timestamp;
    uint16_t unused1;
    uint16_t unused2;
    uint16_t unused3;
    uint16_t unused4;
} enetbufferdesc_t;

static uint32_t phy_addr;
static uint8_t mac[ETHARP_HWADDR_LEN];
static enetbufferdesc_t rx_ring[RX_SIZE] __attribute__((aligned(64)));
static enetbufferdesc_t tx_ring[TX_SIZE] __attribute__((aligned(64)));
static uint8_t rxbufs[RX_SIZE * BFSIZE] __attribute__((aligned(32)));
static uint8_t txbufs[TX_SIZE * BFSIZE] __attribute__((aligned(32)));
volatile static enetbufferdesc_t *p_rxbd = &rx_ring[0];
volatile static enetbufferdesc_t *p_txbd = &tx_ring[0];
static struct netif t41_netif;
static rx_frame_fn rx_callback;
static volatile uint32_t rx_ready;

void enet_isr();

#ifdef LWIP_DEBUG
// arch\cc.h 
void assert_printf(char *msg, int line, char *file)
{
    //_printf("assert msg=%s line=%d file=%s\n", msg, line, file);
}

// include\lwip\err.h 
const char *lwip_strerr(err_t err)
{
    static char buf[32];
    snprintf(buf, sizeof(buf) - 1, "err 0x%X", err);
    return buf;
}
#endif

// PHY_MDIO ==========================

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


// K6x low level ==========================

static void t41_low_level_init()
{
	CCM_CCGR1 |= CCM_CCGR1_ENET(CCM_CCGR_ON);
	// configure PLL6 for 50 MHz, pg 1173
	CCM_ANALOG_PLL_ENET_CLR = CCM_ANALOG_PLL_ENET_POWERDOWN
		| CCM_ANALOG_PLL_ENET_BYPASS | 0x0F;
	CCM_ANALOG_PLL_ENET_SET = CCM_ANALOG_PLL_ENET_ENABLE | CCM_ANALOG_PLL_ENET_BYPASS
		/*| CCM_ANALOG_PLL_ENET_ENET2_REF_EN*/ | CCM_ANALOG_PLL_ENET_ENET_25M_REF_EN
		/*| CCM_ANALOG_PLL_ENET_ENET2_DIV_SELECT(1)*/ | CCM_ANALOG_PLL_ENET_DIV_SELECT(1);
	while (!(CCM_ANALOG_PLL_ENET & CCM_ANALOG_PLL_ENET_LOCK)) ; // wait for PLL lock
	CCM_ANALOG_PLL_ENET_CLR = CCM_ANALOG_PLL_ENET_BYPASS;
//	Serial.printf("PLL6 = %08X (should be 80202001)\n", CCM_ANALOG_PLL_ENET);
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

	// LEDCR offset 0x18, set LED_Link_Polarity, pg 62
	mdio_write(0, 0x18, 0x0280); // LED shows link status, active high
	// RCSR offset 0x17, set RMII_Clock_Select, pg 61
	mdio_write(0, 0x17, 0x0081); // config for 50 MHz clock input

	memset(rx_ring, 0, sizeof(rx_ring));
	memset(tx_ring, 0, sizeof(tx_ring));

    for (int i = 0; i < RX_SIZE; i++)
    {
        rx_ring[i].buffer = &rxbufs[i * BFSIZE];
        rx_ring[i].status = kEnetRxBdEmpty;
        rx_ring[i].extend1 = kEnetRxBdIntrrupt;
    }
    /*The last buffer descriptor should be set with the wrap flag*/
    rx_ring[RX_SIZE - 1].status |= kEnetRxBdWrap;

    for (int i = 0; i < TX_SIZE; i++) 
    {
        tx_ring[i].buffer = &txbufs[i * BFSIZE];
        tx_ring[i].status = kEnetTxBdTransmitCrc;
        tx_ring[i].extend1 = kEnetTxBdTxInterrupt;
#if HW_CHKSUMS == 1
        tx_ring[i].extend1 |= kEnetTxBdIpHdrChecksum | kEnetTxBdProtChecksum;
#endif
    }
    tx_ring[TX_SIZE - 1].status |= kEnetTxBdWrap;

    ENET_EIMR = 0;
    
    ENET_MSCR = ENET_MSCR_MII_SPEED(9);  // 12 is fastest which seems to work
    ENET_RCR = ENET_RCR_NLC | ENET_RCR_MAX_FL(1522) | ENET_RCR_CFEN |
        ENET_RCR_CRCFWD | ENET_RCR_PADEN | ENET_RCR_RMII_MODE |
        ENET_RCR_FCE | /*ENET_RCR_PROM |*/ ENET_RCR_MII_MODE;
    ENET_TCR = ENET_TCR_ADDINS | ENET_TCR_ADDSEL(0) | /*ENET_TCR_RFC_PAUSE | ENET_TCR_TFC_PAUSE |*/
        ENET_TCR_FDEN;

    ENET_TACC = 0
#if ETH_PAD_SIZE == 2
        | ENET_TACC_SHIFT16
#endif
#if HW_CHKSUMS == 1
        | ENET_TACC_IPCHK | ENET_TACC_PROCHK
#endif
        ;
    ENET_RACC = 0
#if ETH_PAD_SIZE == 2
        | ENET_RACC_SHIFT16
#endif
        | ENET_RACC_PADREM
        ;

    ENET_TFWR = ENET_TFWR_STRFWD;
    ENET_RSFL = 0;

    ENET_RDSR = (uint32_t)rx_ring;
    ENET_TDSR = (uint32_t)tx_ring;
    ENET_MRBR = BFSIZE;

	ENET_RXIC = 0;
	ENET_TXIC = 0;
    ENET_PALR = mac[0] << 24 | mac[1] << 16 | mac[2] << 8 | mac[3];
    ENET_PAUR = mac[4] << 24 | mac[5] << 16 | 0x8808;
    
    ENET_OPD = 0x10014;
    ENET_RSEM = 0;
     ENET_MIBC = 0;

    ENET_IAUR = 0;
    ENET_IALR = 0;
    ENET_GAUR = 0;
    ENET_GALR = 0;

    ENET_EIMR = ENET_EIMR_RXF;
	attachInterruptVector(IRQ_ENET, enet_isr);
    NVIC_ENABLE_IRQ(IRQ_ENET);

    ENET_ECR = 0xF0000000 | ENET_ECR_DBSWP | ENET_ECR_EN1588 | ENET_ECR_ETHEREN;
    ENET_RDAR = ENET_RDAR_RDAR;
    ENET_TDAR = ENET_TDAR_TDAR;
    
    //phy soft reset
    //phy_mdio_write(0, 1 << 15);
}

static struct pbuf *t41_low_level_input(volatile enetbufferdesc_t *bdPtr)
{
    const u16_t err_mask = kEnetRxBdTrunc | kEnetRxBdCrc | kEnetRxBdNoOctet | kEnetRxBdLengthViolation;

    struct pbuf *p = NULL;

    /* Determine if a frame has been received */
    if (bdPtr->status & err_mask)
    {
        //if ((bdPtr->status & kEnetRxBdLengthViolation) != 0)
        //    LINK_STATS_INC(link.lenerr);
        //else
        //    LINK_STATS_INC(link.chkerr);
        //LINK_STATS_INC(link.drop);
    }
    else
    {
        p = pbuf_alloc(PBUF_RAW, bdPtr->length, PBUF_POOL);
        if (p) pbuf_take(p, bdPtr->buffer, p->tot_len);
        if (NULL == p)
            LINK_STATS_INC(link.drop);
        else
            LINK_STATS_INC(link.recv);
    }

    bdPtr->status = (bdPtr->status & kEnetRxBdWrap) | kEnetRxBdEmpty; /* Set rx bd empty*/

    ENET_RDAR = ENET_RDAR_RDAR;

    return p;
}

err_t t41_low_level_output(struct netif *netif, struct pbuf *p)
{
    volatile enetbufferdesc_t *bdPtr = p_txbd;

    while (bdPtr->status & kEnetTxBdReady);

    bdPtr->length = pbuf_copy_partial(p, bdPtr->buffer, p->tot_len, 0);

    bdPtr->extend1 &= kEnetTxBdIpHdrChecksum | kEnetTxBdProtChecksum;
    bdPtr->status = (bdPtr->status & kEnetTxBdWrap) | kEnetTxBdTransmitCrc | kEnetTxBdLast | kEnetTxBdReady;

    ENET_TDAR = ENET_TDAR_TDAR;

    if (bdPtr->status & kEnetTxBdWrap) p_txbd = &tx_ring[0]; else p_txbd++;

    LINK_STATS_INC(link.xmit);

    return ERR_OK;
}

static err_t t41_netif_init(struct netif *netif)
{
    netif->linkoutput = t41_low_level_output;
    netif->output = etharp_output;
    netif->mtu = 1522;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP;
    MEMCPY(netif->hwaddr, mac, ETHARP_HWADDR_LEN);
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
#if LWIP_NETIF_HOSTNAME
    netif->hostname = "lwip";
#endif
    netif->name[0] = 'e';
    netif->name[1] = '0';

    t41_low_level_init();

    return ERR_OK;
}

inline volatile static enetbufferdesc_t* rxbd_next()
{
    volatile enetbufferdesc_t* p_bd = p_rxbd;
    while (p_bd->status & kEnetRxBdEmpty)
    {
        if (p_bd->status & kEnetRxBdWrap)
            p_bd = &rx_ring[0];
        else
            p_bd++;
        if (p_bd == p_rxbd) return NULL;
    }
    if (p_rxbd->status & kEnetRxBdWrap)
        p_rxbd = &rx_ring[0];
    else
        p_rxbd++;
    return p_bd;
}

void enet_isr()
{
    //struct pbuf *p;
    while (ENET_EIR & ENET_EIMR_RXF)
    {
        ENET_EIR = ENET_EIMR_RXF;
        if (rx_callback) {
            //p = enet_rx_next();
            rx_callback(NULL);
        } else rx_ready = 1;
		asm("dsb");
    }
}

inline static void check_link_status()
{
    int reg_data = mdio_read(0,1);
    if (reg_data != -1)
    {
        uint8_t is_link_up = !!(reg_data & (1 << 2));
        if (netif_is_link_up(&t41_netif) != is_link_up)
        {
            if (is_link_up)
                netif_set_link_up(&t41_netif);
            else
                netif_set_link_down(&t41_netif);
        }
    }
}

// Pub ==========================

void enet_init(uint32_t phy_addr_, uint8_t *mac_, ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
    phy_addr = phy_addr_;
    MEMCPY(mac, mac_, ETHARP_HWADDR_LEN);
    if (t41_netif.flags == 0)
    {
        srand(micros());

        lwip_init();

        netif_add(&t41_netif, ip, mask, gw, NULL, t41_netif_init, ethernet_input);
        netif_set_default(&t41_netif);
    }
    else
    {
        netif_set_addr(&t41_netif, ip, mask, gw);
    }
}

void enet_set_receive_callback(rx_frame_fn rx_cb)
{
    rx_callback = rx_cb;
}

struct pbuf* enet_rx_next()
{
    volatile enetbufferdesc_t *p_bd = rxbd_next();
    return (p_bd ? t41_low_level_input(p_bd) : NULL);
}

void enet_input(struct pbuf* p_frame)
{
    if (t41_netif.input(p_frame, &t41_netif) != ERR_OK)
        pbuf_free(p_frame);
}

void enet_proc_input(void)
{
    struct pbuf *p;

    if (!rx_callback)
    {
        if (rx_ready)
            rx_ready = 0;
        else
            return;
    }
    while ((p = enet_rx_next()) != NULL)
    {
        enet_input(p);
    }
}

void enet_poll()
{
    sys_check_timeouts();
    check_link_status();
}

#endif
