#if defined(__MK64FX512__) || defined(__MK66FX1M0__)

#include "lwip_k6x.h"
#include "lwipopts.h"
#include "lwip/init.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "lwip/timeouts.h"

#include "core_pins.h"

#define RX_SIZE 5
#define TX_SIZE 5
#define BFSIZE 1600
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
static enetbufferdesc_t rx_ring[RX_SIZE] __attribute__((aligned(16)));
static enetbufferdesc_t tx_ring[TX_SIZE] __attribute__((aligned(16)));
static uint8_t rxbufs[RX_SIZE * BFSIZE] __attribute__((aligned(16)));
static uint8_t txbufs[TX_SIZE * BFSIZE] __attribute__((aligned(16)));
volatile static enetbufferdesc_t *p_rxbd = &rx_ring[0];
volatile static enetbufferdesc_t *p_txbd = &tx_ring[0];
static struct netif k6x_netif;
static rx_frame_fn rx_callback;
static volatile uint32_t rx_ready;

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

void phy_mdio_read_begin(int regaddr)
{
    ENET_MMFR = ENET_MMFR_ST(1) | ENET_MMFR_OP(2) | ENET_MMFR_TA(2)
        | ENET_MMFR_PA(phy_addr) | ENET_MMFR_RA(regaddr);
}

int phy_mdio_read_end()
{
    int ret = -1, count = 0;
    while ((ENET_EIR & ENET_EIRM_MII) == 0)
    {
        if (count++ > 2) return ret;
        delayMicroseconds(7);
    }
    ret = ENET_MMFR;
    ENET_EIR = ENET_EIRM_MII;
    return ret;
}

// read a PHY register (using MDIO & MDC signals)
int phy_mdio_read(int regaddr)
{
    phy_mdio_read_begin(regaddr);
    return phy_mdio_read_end();
}

// write a PHY register (using MDIO & MDC signals)
int phy_mdio_write(int regaddr, uint16_t data)
{
    //uint16_t count = 0;
    ENET_MMFR = ENET_MMFR_ST(1) | ENET_MMFR_OP(1) | ENET_MMFR_TA(2)
        | ENET_MMFR_PA(phy_addr) | ENET_MMFR_RA(regaddr) | ENET_MMFR_DATA(data);
    while ((ENET_EIR & ENET_EIRM_MII) == 0) 
    {
        //if (count++ > 2) return;
        delayMicroseconds(7);
    }
    ENET_EIR = ENET_EIRM_MII;
    return -1;
}

// K6x low level ==========================

static void k6x_low_level_init()
{
    MPU_RGDAAC0 |= 0x007C0000;  // bus master 3 access
    SIM_SCGC2 |= SIM_SCGC2_ENET;   // enet peripheral

    CORE_PIN3_CONFIG = PORT_PCR_MUX(4); // RXD1
    CORE_PIN4_CONFIG = PORT_PCR_MUX(4); // RXD0
    CORE_PIN24_CONFIG = PORT_PCR_MUX(2); // REFCLK
    CORE_PIN25_CONFIG = PORT_PCR_MUX(4); // RXER
    CORE_PIN26_CONFIG = PORT_PCR_MUX(4); // RXDV
    CORE_PIN27_CONFIG = PORT_PCR_MUX(4); // TXEN
    CORE_PIN28_CONFIG = PORT_PCR_MUX(4); // TXD0
    CORE_PIN39_CONFIG = PORT_PCR_MUX(4); // TXD1
    CORE_PIN16_CONFIG = PORT_PCR_MUX(4); // MDIO
    CORE_PIN17_CONFIG = PORT_PCR_MUX(4); // MDC
    
    SIM_SOPT2 |= SIM_SOPT2_RMIISRC | SIM_SOPT2_TIMESRC(3);

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
    
    ENET_MSCR = /*ENET_MSCR_HOLDTIME(0x7) | ENET_MSCR_DIS_PRE |*/ ENET_MSCR_MII_SPEED(15);  // 12 is fastest which seems to work
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

    ENET_PALR = mac[0] << 24 | mac[1] << 16 | mac[2] << 8 | mac[3];
    ENET_PAUR = mac[4] << 24 | mac[5] << 16 | 0x8808;
    
    ENET_OPD = 0x10014;
    ENET_RSEM = 0;

    ENET_IAUR = 0;
    ENET_IALR = 0;
    ENET_GAUR = 0;
    ENET_GALR = 0;

    ENET_EIMR = ENET_EIRM_RXF;
    NVIC_SET_PRIORITY(IRQ_ENET_RX, IRQ_PRIORITY);
    NVIC_ENABLE_IRQ(IRQ_ENET_RX);

    ENET_ECR = 0xF0000000 | ENET_ECR_DBSWP | ENET_ECR_EN1588 | ENET_ECR_ETHEREN;
    ENET_RDAR = ENET_RDAR_RDAR;
    ENET_TDAR = ENET_TDAR_TDAR;
    
    //phy soft reset
    //phy_mdio_write(0, 1 << 15);
}

static struct pbuf *k6x_low_level_input(volatile enetbufferdesc_t *bdPtr)
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

err_t k6x_low_level_output(struct netif *netif, struct pbuf *p)
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

static err_t k6x_netif_init(struct netif *netif)
{
    netif->linkoutput = k6x_low_level_output;
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

    k6x_low_level_init();

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

//mk20dx128.c
void enet_rx_isr()
{
    //struct pbuf *p;
    while (ENET_EIR & ENET_EIRM_RXF)
    {
        ENET_EIR = ENET_EIRM_RXF;
        if (rx_callback)
        {
            //p = enet_rx_next();
            rx_callback(NULL);
        }
        else
            rx_ready = 1;
    }
}

inline static void check_link_status()
{
    int reg_data = phy_mdio_read_end();
    if (reg_data != -1)
    {
        uint8_t is_link_up = !!(reg_data & (1 << 2));
        if (netif_is_link_up(&k6x_netif) != is_link_up)
        {
            if (is_link_up)
                netif_set_link_up(&k6x_netif);
            else
                netif_set_link_down(&k6x_netif);
        }
    }
    phy_mdio_read_begin(1);
}

// Pub ==========================

void enet_init(uint32_t phy_addr_, uint8_t *mac_, ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
    phy_addr = phy_addr_;
    MEMCPY(mac, mac_, ETHARP_HWADDR_LEN);
    if (k6x_netif.flags == 0)
    {
        srand(micros());

        lwip_init();

        netif_add(&k6x_netif, ip, mask, gw, NULL, k6x_netif_init, ethernet_input);
        netif_set_default(&k6x_netif);
    }
    else
    {
        netif_set_addr(&k6x_netif, ip, mask, gw);
    }
}

void enet_set_receive_callback(rx_frame_fn rx_cb)
{
    rx_callback = rx_cb;
}

struct pbuf* enet_rx_next()
{
    volatile enetbufferdesc_t *p_bd = rxbd_next();
    return (p_bd ? k6x_low_level_input(p_bd) : NULL);
}

void enet_input(struct pbuf* p_frame)
{
    if (k6x_netif.input(p_frame, &k6x_netif) != ERR_OK)
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
