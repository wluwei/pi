
#include "bcm2835.h"
#include <stdio.h>
#include <stdlib.h>
// #include <unistd.h>
#include <string.h>

#define BCM2835_PHY_BASE  0x7E000000
#define BCM2835_MU_BASE     0x215040
#define BCM2835_DMA_BASE    0x007000
#define BCM2835_DMA15_BASE  0xE05000
#define BCM2835_EMMC_BASE   0x300000
#define BCM2835_IRQ_BASE    0x00B000
#define BCM2835_PCM_BASE    0x203000
#define BCM2835_BSC2_BASE   0x805000

#define MODULE_HEAD() \
    printf("\n%-9s %-10s %-5s %-4s\n", "Module", "PhyAddr", "Sect", "Page");
#define MODULE_BASE(title) \
    bcm2835_get_page(title, 1); \

#define REG_TITLE(title) \
    bcm2835_get_page(title, 0); \
    printf("%-25s %-4s %-10s\n", "Register Name", "Ofst", "Value");
#define GET_REG(offset) \
    printf("%-25s 0x%02x 0x%08x\n", #offset, offset, *(uint32_t*)(bcm2835_reg_base(#offset)+offset));
#define GET_REG_ADDR(base, offset) \
    printf("%-25s 0x%02x 0x%08x\n", #offset, offset, *(uint32_t*)((uint32_t)base+offset));

typedef struct {
    char *   prefix;
    uint32_t offset;
    char *   section;
    uint32_t page;
} reg_info_st;

// see also https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
reg_info_st reg_info[] = {
    { "BCM2835_AUX_MU",     BCM2835_MU_BASE   , "2.2.2", 11 },
    { "BCM2835_AUX_SPI",    BCM2835_SPI1_BASE , "2.3.4", 22 },
    { "BCM2835_AUX",        BCM2835_AUX_BASE  , "2.1.1",  9 },
    { "BCM2835_BSC0",       BCM2835_BSC0_BASE , "3.2"  , 28 },
    { "BCM2835_BSC1",       BCM2835_BSC1_BASE , "3.2"  , 28 },
    { "BCM2835_BSC2",       BCM2835_BSC2_BASE , "3.2"  , 28 },
    { "BCM2835_BSC",        BCM2835_BSC0_BASE , "3.2"  , 28 },
    { "BCM2835_DMA",        BCM2835_DMA_BASE  , "4.2.1", 40 },
    { "BCM2835_EMMC",       BCM2835_EMMC_BASE , "4.2.1", 40 },
    { "BCM2835_GP",         BCM2835_GPIO_BASE , "6.1"  , 90 },
    { "BCM2835_PWMCLK",     BCM2835_CLOCK_BASE, "6.3"  ,107 },  
    { "BCM2835_IRQ",        BCM2835_IRQ_BASE  , "7.5"  ,159 },
    { "BCM2835_PCM",        BCM2835_PCM_BASE  , "8.8"  ,125 },
    { "BCM2835_PWM",        BCM2835_GPIO_PWM  , "9.6"  ,141 },
    { "BCM2835_PADS_GPIO",  BCM2835_GPIO_PADS , "?"    ,  0 },
    { "BCM2835_SPI0",       BCM2835_SPI0_BASE , "10.5" ,152 },
    { "BCM2835_ST",         BCM2835_ST_BASE   , "12.1" ,172 },
};

uint32_t bcm2835_get_page(char *title, int addr)
{
    int i;
    int len = 0;

    len = strlen("BCM2835_");
    for(i=0; i<sizeof(reg_info)/sizeof(reg_info[0]); i++) {
        if (strncmp(reg_info[i].prefix+len, title, strlen(reg_info[i].prefix)-len) == 0) {
            if(addr) {
                printf("%-9s 0x%08x %-5s %3d\n", title, BCM2835_PHY_BASE+reg_info[i].offset,
                    reg_info[i].section, reg_info[i].page);
            }
            else {
                printf("\n%-9s %-5s P%d\n", title,
                    reg_info[i].section, reg_info[i].page);
            }
            break;
        }
    }

    return reg_info[i].page;
}

uint32_t bcm2835_reg_base(char *reg_name)
{
    int i;
    uint32_t reg_base = 0;

    for(i=0; i<sizeof(reg_info)/sizeof(reg_info[0]); i++) {
        if (strncmp(reg_info[i].prefix, reg_name, strlen(reg_info[i].prefix)) == 0) {
            reg_base = (uint32_t)bcm2835_peripherals + reg_info[i].offset;
            break;
        }
    }

    return reg_base;
}

void bcm2835_dump_reg_base()
{
    // printf("euid: %d\n", geteuid());
    printf("Read base and size from %s\n", BMC2835_RPI2_DT_FILENAME);
    printf("bcm2835_peripherals_base %p\n",     bcm2835_peripherals_base);
    printf("bcm2835_peripherals_size 0x%08x\n", bcm2835_peripherals_size);
    // printf("Map PM %p to VM %p\n", bcm2835_peripherals_base, bcm2835_peripherals);
    printf("bcm2835_peripherals(VM) %p\n",     bcm2835_peripherals);

    MODULE_HEAD();
    MODULE_BASE("AUX_MU");
    MODULE_BASE("AUX_SPI");
    MODULE_BASE("AUX");
    MODULE_BASE("BSC0");
    MODULE_BASE("BSC1");
    MODULE_BASE("DMA");
    MODULE_BASE("EMMC");
    MODULE_BASE("GP");
    MODULE_BASE("PWMCLK");
    MODULE_BASE("IRQ");
    MODULE_BASE("PCM");
    MODULE_BASE("PWM");
    MODULE_BASE("PADS_GPIO");
    MODULE_BASE("SPI0");
    MODULE_BASE("ST");

#if 0
    printf("GPIO %p 0x%08x\n", bcm2835_gpio, BCM2835_GPIO_BASE );
    printf("PWM  %p 0x%08x\n", bcm2835_pwm , BCM2835_GPIO_PWM  );
    printf("CLK  %p 0x%08x\n", bcm2835_clk , BCM2835_CLOCK_BASE);
    printf("PADS %p 0x%08x\n", bcm2835_pads, BCM2835_GPIO_PADS );
    printf("SPI0 %p 0x%08x\n", bcm2835_spi0, BCM2835_SPI0_BASE );
    printf("BSC0 %p 0x%08x\n", bcm2835_bsc0, BCM2835_BSC0_BASE );
    printf("BSC1 %p 0x%08x\n", bcm2835_bsc1, BCM2835_BSC1_BASE );
    printf("ST   %p 0x%08x\n", bcm2835_st  , BCM2835_ST_BASE   );
    printf("AUX  %p 0x%08x\n", bcm2835_aux , BCM2835_AUX_BASE  );
    printf("SPI1 %p 0x%08x\n", bcm2835_spi1, BCM2835_SPI1_BASE );
#endif
    return;
}

void bcm2835_dump_reg_spi()
{
    REG_TITLE("SPI0");
    GET_REG(BCM2835_SPI0_CS  );
    GET_REG(BCM2835_SPI0_FIFO);
    GET_REG(BCM2835_SPI0_CLK );
    GET_REG(BCM2835_SPI0_DLEN);
    GET_REG(BCM2835_SPI0_LTOH);
    GET_REG(BCM2835_SPI0_DC  );
    REG_TITLE("AUX");
    GET_REG(BCM2835_AUX_IRQ       );
    GET_REG(BCM2835_AUX_ENABLE    );
    REG_TITLE("AUX_SPI");
    GET_REG(BCM2835_AUX_SPI_CNTL0 );
    GET_REG(BCM2835_AUX_SPI_CNTL1 );
    GET_REG(BCM2835_AUX_SPI_STAT  );
    GET_REG(BCM2835_AUX_SPI_PEEK  );
    GET_REG(BCM2835_AUX_SPI_IO    );
    GET_REG(BCM2835_AUX_SPI_TXHOLD);
}

void bcm2835_dump_reg_bsc()
{
    REG_TITLE("BSC0");
    GET_REG_ADDR(bcm2835_bsc0, BCM2835_BSC_C   );
    GET_REG_ADDR(bcm2835_bsc0, BCM2835_BSC_S   );
    GET_REG_ADDR(bcm2835_bsc0, BCM2835_BSC_DLEN);
    GET_REG_ADDR(bcm2835_bsc0, BCM2835_BSC_A   );
    GET_REG_ADDR(bcm2835_bsc0, BCM2835_BSC_FIFO);
    GET_REG_ADDR(bcm2835_bsc0, BCM2835_BSC_DIV );
    GET_REG_ADDR(bcm2835_bsc0, BCM2835_BSC_DEL );
    GET_REG_ADDR(bcm2835_bsc0, BCM2835_BSC_CLKT);
    REG_TITLE("BSC1");
    GET_REG_ADDR(bcm2835_bsc1, BCM2835_BSC_C   );
    GET_REG_ADDR(bcm2835_bsc1, BCM2835_BSC_S   );
    GET_REG_ADDR(bcm2835_bsc1, BCM2835_BSC_DLEN);
    GET_REG_ADDR(bcm2835_bsc1, BCM2835_BSC_A   );
    GET_REG_ADDR(bcm2835_bsc1, BCM2835_BSC_FIFO);
    GET_REG_ADDR(bcm2835_bsc1, BCM2835_BSC_DIV );
    GET_REG_ADDR(bcm2835_bsc1, BCM2835_BSC_DEL );
    GET_REG_ADDR(bcm2835_bsc1, BCM2835_BSC_CLKT);
}

void bcm2835_dump_reg_gpio()
{
    REG_TITLE("GP");
    GET_REG(BCM2835_GPFSEL0  );
    GET_REG(BCM2835_GPFSEL1  );
    GET_REG(BCM2835_GPFSEL2  );
    GET_REG(BCM2835_GPFSEL3  );
    GET_REG(BCM2835_GPFSEL4  );
    GET_REG(BCM2835_GPFSEL5  );
    GET_REG(BCM2835_GPSET0   );
    GET_REG(BCM2835_GPSET1   );
    GET_REG(BCM2835_GPCLR0   );
    GET_REG(BCM2835_GPCLR1   );
    GET_REG(BCM2835_GPLEV0   );
    GET_REG(BCM2835_GPLEV1   );
    GET_REG(BCM2835_GPEDS0   );
    GET_REG(BCM2835_GPEDS1   );
    GET_REG(BCM2835_GPREN0   );
    GET_REG(BCM2835_GPREN1   );
    GET_REG(BCM2835_GPFEN0   );
    GET_REG(BCM2835_GPFEN1   );
    GET_REG(BCM2835_GPHEN0   );
    GET_REG(BCM2835_GPHEN1   );
    GET_REG(BCM2835_GPLEN0   );
    GET_REG(BCM2835_GPLEN1   );
    GET_REG(BCM2835_GPAREN0  );
    GET_REG(BCM2835_GPAREN1  );
    GET_REG(BCM2835_GPAFEN0  );
    GET_REG(BCM2835_GPAFEN1  );
    GET_REG(BCM2835_GPPUD    );
    GET_REG(BCM2835_GPPUDCLK0);
    GET_REG(BCM2835_GPPUDCLK1);
}

#undef BCM2835_PWM_CONTROL
#undef BCM2835_PWM_STATUS 
#undef BCM2835_PWM_DMAC   
#undef BCM2835_PWM0_RANGE 
#undef BCM2835_PWM0_DATA  
#undef BCM2835_PWM_FIF1   
#undef BCM2835_PWM1_RANGE 
#undef BCM2835_PWM1_DATA  

#define BCM2835_PWM_CONTROL 0x00
#define BCM2835_PWM_STATUS  0x04
#define BCM2835_PWM_DMAC    0x08
#define BCM2835_PWM0_RANGE  0x0C
#define BCM2835_PWM0_DATA   0x14
#define BCM2835_PWM_FIF1    0x18
#define BCM2835_PWM1_RANGE  0x20
#define BCM2835_PWM1_DATA   0x24

void bcm2835_dump_reg_pwm()
{
    REG_TITLE("PWM");
    GET_REG(BCM2835_PWM_CONTROL);
    GET_REG(BCM2835_PWM_STATUS );
    GET_REG(BCM2835_PWM_DMAC   );
    GET_REG(BCM2835_PWM0_RANGE );
    GET_REG(BCM2835_PWM0_DATA  );
    GET_REG(BCM2835_PWM_FIF1   );
    GET_REG(BCM2835_PWM1_RANGE );
    GET_REG(BCM2835_PWM1_DATA  );
}

#define BCM2835_ST_C0 0x0c
#define BCM2835_ST_C1 0x10
#define BCM2835_ST_C2 0x14
#define BCM2835_ST_C3 0x18

void bcm2835_dump_reg_st()
{
    REG_TITLE("ST");
    GET_REG(BCM2835_ST_CS );
    GET_REG(BCM2835_ST_CLO);
    GET_REG(BCM2835_ST_CHI);
    GET_REG(BCM2835_ST_C0 );
    GET_REG(BCM2835_ST_C1 );
    GET_REG(BCM2835_ST_C2 );
    GET_REG(BCM2835_ST_C3 );
}

typedef enum
{
    REG_BASE = 0,
    REG_GPIO,
    REG_PWM,
    REG_BSC,
    REG_SPI,
    REG_ST,
} module_st;

char *reg_module[] = {
    "base",
    "gpio",
    "pwm",
    "bsc",
    "spi",
    "st",
};

void usage()
{
    int i;

    printf("Usage dump_reg <module>\n  module: ");
    for(i=0; i<sizeof(reg_module)/sizeof(reg_module[0]); i++) {
        printf("%s, ", reg_module[i]);
    }
    printf("all.\n");
    exit(-1);
}

int main(int argc, char **argv)
{
    uint32_t verbose = 0;
    int i;

    if(argc < 2) {
        usage();
    }

    if (strncmp("all", argv[1], strlen("all")) == 0) {
        verbose = (uint32_t)(-1);
    } else {
        for(i=0; i<sizeof(reg_module)/sizeof(reg_module[0]); i++) {
            if (strncmp(reg_module[i], argv[1], strlen(reg_module[i])) == 0) {
                verbose = 1<<i;
                break;
            }
        }
    }

    if(verbose == 0) {
        usage();
    }

    // bcm2835_set_debug(1);
    bcm2835_init();

    if(verbose & 1<<REG_BASE) bcm2835_dump_reg_base();
    if(verbose & 1<<REG_GPIO) bcm2835_dump_reg_gpio();
    if(verbose & 1<<REG_PWM)  bcm2835_dump_reg_pwm();
    if(verbose & 1<<REG_BSC)  bcm2835_dump_reg_bsc();
    if(verbose & 1<<REG_SPI)  bcm2835_dump_reg_spi();
    if(verbose & 1<<REG_ST)   bcm2835_dump_reg_st();

    return 0;
}

