#include "arch.h"
#include "reg.h"
#include "u.h"

#define BOOT_FLAG_USB_BOOT  (1 << 30)

static u32int 
ost_read32(void)
{
    /* Read OST_2CNTL using raw address to avoid exposing internal headers.
     * The 64-bit counter is read with IRQs disabled and since threads are
     * not pre-emptive in Rockbox we won't trash anybody's 64-bit read by
     * reading the low count without locking.
     */
    return *(const volatile u32int*)0xb2000020;
}

static void 
ost_delay(u32int count)
{
    /* Add one to ensure we delay for at least the time given */
    count += 1;
    u32int start = ost_read32();
    while(ost_read32() - start < count);
}

static void
udelay(u32int us)
{
    while(us > MAX_UDELAY_ARG) {
        ost_delay(MAX_UDELAY_ARG * OST_TICKS_PER_US);
        us -= MAX_UDELAY_ARG;
    }
}

static void
mdelay(u32int ms)
{
    while(ms > MAX_MDELAY_ARG) {
        ost_delay(MAX_MDELAY_ARG * 1000 * OST_TICKS_PER_US);
        ms -= MAX_MDELAY_ARG;
    }
}

static void
init_boot_flags(void)
{
    REG_CPM_SCRATCH_PROT = 0x5a5a;
    REG_CPM_SCRATCH = 0;
    REG_CPM_SCRATCH_PROT = 0xa5a5;
}

static void
set_boot_flag(void)
{
    REG_CPM_SCRATCH_PROT = 0x5a5a;
	REG_CPM_SCRATCH = REG_CPM_SCRATCH | BOOT_FLAG_USB_BOOT;
    REG_CPM_SCRATCH_PROT = 0xa5a5;
}

static void
init_ost(void)
{
    /* NOTE: the prescaler needs to be the same as in system-x1000.c */
    jz_writef(CPM_CLKGR, OST(0));
    jz_writef(OST_CTRL, PRESCALE2_V(BY_4));
    jz_overwritef(OST_CLEAR, OST2(1));
    jz_write(OST_2CNTH, 0);
    jz_write(OST_2CNTL, 0);
    jz_setf(OST_ENABLE, OST2);
}

static void
clk_set_ddr(x1000_clk_t src, u32int div)
{
    /* Write new configuration */
    jz_writef(CPM_DDRCDR, CE(1), CLKDIV(div - 1),
              CLKSRC(src == X1000_CLK_MPLL ? 2 : 1));

    /* Wait until mux and divider change are complete */
    while(jz_readf(CPM_CSR, DDR_MUX) == 0);
    while(jz_readf(CPM_DDRCDR, BUSY));

    /* Disable CE bit after change */
    jz_writef(CPM_DDRCDR, CE(0));
}

static void
clk_init_early(void)
{
    jz_writef(CPM_MPCR, ENABLE(0));
    while(jz_readf(CPM_MPCR, ON));

    /* 24 MHz * 25 = 600 MHz */
    jz_writef(CPM_MPCR, BS(1), PLLM(25 - 1), PLLN(0), PLLOD(0), ENABLE(1));
    while(jz_readf(CPM_MPCR, ON) == 0);

    /* 600 MHz / 3 = 200 MHz */
    clk_set_ddr(X1000_CLK_MPLL, 3);
}

static int 
init_dram(void)
{
    jz_writef(CPM_CLKGR, DDR(0));

    REG_CPM_DRCG = 0x73;
    mdelay(3);
    REG_CPM_DRCG = 0x71;
    mdelay(3);
    REG_DDRC_APB_PHYRST_CFG = 0x1a00001;
    mdelay(3);
    REG_DDRC_APB_PHYRST_CFG = 0;
    mdelay(3);
    REG_DDRC_CTRL = 0xf00000;
    mdelay(3);
    REG_DDRC_CTRL = 0;
    mdelay(3);

    REG_DDRC_CFG = 0xa468a6c;
    REG_DDRC_CTRL = 2;
    REG_DDRPHY_DTAR = 0x150000;
    REG_DDRPHY_DCR = 0;
    REG_DDRPHY_MR0 = 0x42;
    REG_DDRPHY_MR2 = 0x98;
    REG_DDRPHY_PTR0 = 0x21000a;
    REG_DDRPHY_PTR1 = 0xa09c40;
    REG_DDRPHY_PTR2 = 0x280014;
    REG_DDRPHY_DTPR0 = 0x1a69444a;
    REG_DDRPHY_DTPR1 = 0x180090;
    REG_DDRPHY_DTPR2 = 0x1ff99428;
    REG_DDRPHY_DXGCR(0) = 0x90881;
    REG_DDRPHY_DXGCR(1) = 0x90881;
    REG_DDRPHY_DXGCR(2) = 0x90e80;
    REG_DDRPHY_DXGCR(3) = 0x90e80;
    REG_DDRPHY_PGCR = 0x1042e03;
    REG_DDRPHY_ACIOCR = 0x30c00813;
    REG_DDRPHY_DXCCR = 0x4912;

    int i = 10000;
    while(i > 0 && REG_DDRPHY_PGSR != 7 && REG_DDRPHY_PGSR != 0x1f)
        i -= 1;
    if(i == 0)
        return 1;

    REG_DDRPHY_ACDLLCR = 0x80000000;
    REG_DDRPHY_DSGCR &= ~0x10;
    REG_DDRPHY_DLLGCR |= 0x800000;
    REG_DDRPHY_PIR = 0x20020041;
    while(i > 0 && REG_DDRPHY_PGSR != 0xf && REG_DDRPHY_PGSR != 0x1f)
        i -= 1;
    if(i == 0)
        return 2;

    REG_DDRC_APB_PHYRST_CFG = 0x400000;
    mdelay(3);
    REG_DDRC_APB_PHYRST_CFG = 0;
    mdelay(3);

    REG_DDRC_CFG = 0xa468aec;
    REG_DDRC_CTRL = 2;
    REG_DDRPHY_PIR = 0x20020081;

    i = 500000;
    while(REG_DDRPHY_PGSR != 0x1f) {
        if(REG_DDRPHY_PGSR & 0x70)
            break;
        i -= 1;
    }

    if(i == 0)
        return 3;

    if((REG_DDRPHY_PGSR & 0x60) != 0 && REG_DDRPHY_PGSR != 0)
        return 4;

    REG_DDRC_CTRL = 0;
    REG_DDRC_CTRL = 10;
    REG_DDRC_CTRL = 0;
    REG_DDRC_CFG  = 0xa468a6c;
    REG_DDRC_TIMING1 = 0x2050501;
    REG_DDRC_TIMING2 = 0x4090404;
    REG_DDRC_TIMING3 = 0x2704030d;
    REG_DDRC_TIMING4 = 0xb7a0251;
    REG_DDRC_TIMING5 = 0xff090200;
    REG_DDRC_TIMING6 = 0xa0a0202;
    REG_DDRC_MMAP0   = 0x20fc;
    REG_DDRC_MMAP1   = 0x2400;
    REG_DDRC_CTRL    = 10;
    REG_DDRC_REFCNT  = 0x2f0003; /* is this adjustable for 32M? */
    REG_DDRC_CTRL    = 0xc91e;

    REG_DDRC_REMAP1 = 0x03020c0b;
    REG_DDRC_REMAP2 = 0x07060504;
    REG_DDRC_REMAP3 = 0x000a0908;
    REG_DDRC_REMAP4 = 0x0f0e0d01;
    REG_DDRC_REMAP5 = 0x13121110;
    REG_DDRC_STATUS &= ~0x40;

    jz_writef(CPM_DDRCDR, GATE_EN(1));
    REG_DDRC_APB_CLKSTP_CFG = 0x9000000f;
    REG_DDRC_AUTOSR_EN = 1;
    return 0;
}

void
spl_main(void)
{
	u32int saved_cpm_scratch;
    int rc;
    void* load_buffer;
    /* magic */
    REG_CPM_PSWC0ST = 0x00;
    REG_CPM_PSWC1ST = 0x10;
    REG_CPM_PSWC2ST = 0x18;
    REG_CPM_PSWC3ST = 0x08;

    saved_cpm_scratch = REG_CPM_SCRATCH;

	init_boot_flags();
	clk_init_early();
	init_ost();
	init_dram();
	set_boot_flag();
	return;
}
