#if LAB >= 6
// LAB 6: Your driver code here
#if SOL >= 6
#include <inc/x86.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/error.h>
#include <kern/pci.h>
#include <kern/e100.h>
#include <kern/pmap.h>
#include <kern/picirq.h>

uint8_t e100_irq;

#define E100_TX_SLOTS			64
#define E100_RX_SLOTS			64
#define E100_NULL			0xffffffff

#define	E100_CSR_SCB_STATACK		0x01	// scb_statack (1 byte)
#define	E100_CSR_SCB_COMMAND		0x02	// scb_command (1 byte)
#define	E100_CSR_SCB_GENERAL		0x04	// scb_general (4 bytes)
#define	E100_CSR_PORT			0x08	// port (4 bytes)

#define E100_PORT_SOFTWARE_RESET	0

#define E100_SCB_COMMAND_CU_START	0x10
#define E100_SCB_COMMAND_CU_RESUME	0x20

#define	E100_SCB_STATACK_FCP		0x01
#define	E100_SCB_STATACK_ER		0x02
#define E100_SCB_STATACK_SWI		0x04
#define E100_SCB_STATACK_MDI		0x08
#define E100_SCB_STATACK_RNR		0x10
#define E100_SCB_STATACK_CNA		0x20
#define E100_SCB_STATACK_FR		0x40
#define E100_SCB_STATACK_CXTNO		0x80

// commands
#define E100_CB_COMMAND_XMIT		0x4

// command flags
#define E100_CB_COMMAND_SF		0x0008	// simple/flexible mode
#define E100_CB_COMMAND_I		0x2000	// generate interrupt on completion
#define E100_CB_COMMAND_S		0x4000	// suspend on completion
#define E100_CB_COMMAND_EL		0x8000	// end of list

// status
#define E100_CB_STATUS_OK		0x2000
#define E100_CB_STATUS_C		0x8000

struct e100_cb_tx {
	volatile uint16_t cb_status;
	volatile uint16_t cb_command;
	volatile uint32_t link_addr;
	volatile uint32_t tbd_array_addr;
	volatile uint16_t byte_count;
	volatile uint8_t tx_threshold;
	volatile uint8_t tbd_number;
};

// Transmit Buffer Descriptor (TBD)
struct e100_tbd {
	volatile uint32_t tb_addr;
	volatile uint16_t tb_size;
	volatile uint16_t tb_pad;
};

// Receive Frame Area (RFA)
struct e100_rfa {
	// Fields common to all i8255x chips.
	volatile uint16_t rfa_status;
	volatile uint16_t rfa_control;
	volatile uint32_t link_addr;
	volatile uint32_t rbd_addr;
	volatile uint16_t actual_size;
	volatile uint16_t size;
};

// Receive Buffer Descriptor (RBD)
struct e100_rbd {
	volatile uint16_t rbd_count;
	volatile uint16_t rbd_pad0;
	volatile uint32_t rbd_link;
	volatile uint32_t rbd_buffer;
	volatile uint16_t rbd_size;
	volatile uint16_t rbd_pad1;
};

#define E100_SIZE_MASK			0x3fff	// mask out status/control bits

struct e100_tx_slot {
	struct e100_cb_tx tcb;
	struct e100_tbd tbd;
	struct Page *p;
};

struct e100_rx_slot {
	struct e100_rfa rfd;
	struct e100_rbd rbd;
	struct Page *p;
};

static struct {
	uint32_t iobase;

	struct e100_tx_slot tx[E100_TX_SLOTS];
	int tx_head;
	int tx_tail;
	char tx_idle;

	struct e100_rx_slot rx[E100_RX_SLOTS];
	int rx_head;
	int rx_tail;
} the_e100;

static void udelay(unsigned int u)
{
	unsigned int i;
	for (i = 0; i < u; i++)
		inb(0x84);
}

static void
e100_scb_wait(void)
{
	int i;

	for (i = 0; i < 100000; i++) {
		if (inb(the_e100.iobase + E100_CSR_SCB_COMMAND) == 0)
			return;
	}
	
	cprintf("e100_scb_wait: timeout\n");
}

static void
e100_scb_cmd(uint8_t cmd)
{
    outb(the_e100.iobase + E100_CSR_SCB_COMMAND, cmd);
}

static void e100_tx_start(void)
{
	int i = the_e100.tx_tail % E100_TX_SLOTS;

	if (the_e100.tx_tail == the_e100.tx_head)
		panic("oops, no TCBs");

	if (the_e100.tx_idle) {
		e100_scb_wait();
		outl(the_e100.iobase + E100_CSR_SCB_GENERAL, PADDR(&the_e100.tx[i].tcb));
		e100_scb_cmd(E100_SCB_COMMAND_CU_START);
		the_e100.tx_idle = 0;
	} else {
		e100_scb_wait();
		e100_scb_cmd(E100_SCB_COMMAND_CU_RESUME);
	}
}

int e100_txbuf(struct Page *pp, unsigned int size, unsigned int offset)
{
	int i;

	if (the_e100.tx_head - the_e100.tx_tail == E100_TX_SLOTS) {
		cprintf("e100_txbuf: no space\n");
		return -E_NO_MEM;
	}

	i = the_e100.tx_head % E100_TX_SLOTS;

	the_e100.tx[i].tbd.tb_addr = page2pa(pp) + offset;
	the_e100.tx[i].tbd.tb_size = size & E100_SIZE_MASK;
	the_e100.tx[i].tcb.cb_status = 0;
	the_e100.tx[i].tcb.cb_command = E100_CB_COMMAND_XMIT |
		E100_CB_COMMAND_SF | E100_CB_COMMAND_I | E100_CB_COMMAND_S;

	pp->pp_ref++;
	the_e100.tx[i].p = pp;
	the_e100.tx_head++;
	
	e100_tx_start();

	return 0;
}

int e100_rxbuf(struct Page *pp, unsigned int size, unsigned int offset)
{
	// The first 4 bytes will hold the number of recieved bytes
	return -1;
}

static void e100_intr_tx(void)
{
	int i;

	for ( ;the_e100.tx_head != the_e100.tx_tail; the_e100.tx_tail++) {
		i = the_e100.tx_tail % E100_TX_SLOTS;
		
		if (!(the_e100.tx[i].tcb.cb_status & E100_CB_STATUS_C))
			break;

		page_decref(the_e100.tx[i].p);
		the_e100.tx[i].p = 0;
	}
}

void e100_intr(void)
{
	int r;
	
	r = inb(the_e100.iobase + E100_CSR_SCB_STATACK);
	outb(the_e100.iobase + E100_CSR_SCB_STATACK, r);
	
	if (r & E100_SCB_STATACK_CXTNO)
		e100_intr_tx();
	else 
		cprintf("e100_intr: unhandled STAT/ACK %x\n", r);
}

int e100_attach(struct pci_func *pcif)
{
	int i, next;

	pci_func_enable(pcif);

	e100_irq = pcif->irq_line;
	the_e100.iobase = pcif->reg_base[1];
	the_e100.tx_idle = 1;

	// Reset the card
	outl(the_e100.iobase + E100_CSR_PORT, E100_PORT_SOFTWARE_RESET);
	udelay(10);

	// Setup TX DMA ring for CU
	for (i = 0; i < E100_TX_SLOTS; i++) {
		next = (i + 1) % E100_TX_SLOTS;
		memset(&the_e100.tx[i], 0, sizeof(the_e100.tx[i]));
		the_e100.tx[i].tcb.link_addr = PADDR(&the_e100.tx[next].tcb);
		the_e100.tx[i].tcb.tbd_array_addr = PADDR(&the_e100.tx[i].tbd);
		the_e100.tx[i].tcb.tbd_number = 1;
		the_e100.tx[i].tcb.tx_threshold = 4;
	}

	// Setup RX DMA ring for RU
	for (i = 0; i < E100_RX_SLOTS; i++) {
		next = (i + 1) % E100_RX_SLOTS;
		memset(&the_e100.rx[i], 0, sizeof(the_e100.rx[i]));
		the_e100.rx[i].rfd.link_addr = PADDR(&the_e100.rx[next].rfd);
		the_e100.rx[i].rfd.rbd_addr = PADDR(&the_e100.rx[i].rbd);
		the_e100.rx[i].rbd.rbd_link = PADDR(&the_e100.rx[next].rbd);
	}
	
	irq_setmask_8259A(irq_mask_8259A & ~(1 << e100_irq));
	return 1;
}
#endif  // SOL >= 6
#endif  // LAB >= 6
