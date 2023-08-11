/*
 * ARM board emulation.
 *
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/arm/boot.h"
#include "hw/arm/armv7m.h"
#include "hw/or-irq.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/misc/unimp.h"
#include "hw/char/cmsdk-apb-uart.h"
#include "hw/timer/cmsdk-apb-timer.h"
#include "hw/timer/cmsdk-apb-dualtimer.h"
#include "hw/ssi/pl022.h"
#include "hw/i2c/arm_sbcon_i2c.h"
#include "hw/net/lan9118.h"
#include "net/net.h"
#include "hw/watchdog/cmsdk-apb-watchdog.h"
#include "hw/qdev-clock.h"
#include "qom/object.h"
#include "hw/char/thomas_test_device.h"
#include "exec/hwaddr.h"
#include "qemu/notify.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/block/flash.h"
#include "sysemu/kvm.h"
#include "hw/intc/arm_gicv3_common.h"
#include "qom/object.h"
#include "hw/char/pl011.h"
#include "hw/intc/arm_gic.h"
#include "hw/intc/arm_gicv3_common.h"
#include "hw/irq.h"
#include "hw/char/thomas_spinlock.h"
#include "hw/char/serial.h"
#include "hw/sd/sdhci.h"

enum {
    THOMAS_TIGER_ROM,
    THOMAS_TIGER_FLASH,
    THOMAS_TIGER_SRAM0,
    THOMAS_TIGER_SRAM1,
    THOMAS_TIGER_AON_SRAM,
    THOMAS_TIGER_HIFI5_SRAM,
    THOMAS_TIGER_SECURE,
    THOMAS_TIGER_EFUSE,
    THOMAS_TIGER_CPU_NOC,
    THOMAS_TIGER_NOC_APB,
    THOMAS_TIGER_CORESIGHT,
    THOMAS_TIGER_GIC_DIST,
    THOMAS_TIGER_GIC_REDIST,
    THOMAS_TIGER_AON_APB,
    THOMAS_TIGER_DSP_APB,
    THOMAS_TIGER_MAILBOX,
    THOMAS_TIGER_A55_APB0_SPI0,
    THOMAS_TIGER_A55_APB0_SPI1,
    THOMAS_TIGER_A55_APB0_SPI2,
    THOMAS_TIGER_A55_APB0_SPI3,
    THOMAS_TIGER_A55_APB0_SPI4,
    THOMAS_TIGER_A55_APB0_SPI5,
    THOMAS_TIGER_A55_APB0_UART0,
    THOMAS_TIGER_A55_APB0_UART1,
    THOMAS_TIGER_A55_APB0_UART2,
    THOMAS_TIGER_A55_APB0_UART3,
    THOMAS_TIGER_A55_APB0_UART4,
    THOMAS_TIGER_A55_APB0_I2C0,
    THOMAS_TIGER_A55_APB0_I2C1,
    THOMAS_TIGER_A55_APB0_I2C2,
    THOMAS_TIGER_A55_APB0_I2C3,
    THOMAS_TIGER_A55_APB0_I2C4,
    THOMAS_TIGER_A55_APB0_LPWM0,
    THOMAS_TIGER_A55_APB0_LPWM1,
    THOMAS_TIGER_A55_APB0_GPIO,
    THOMAS_TIGER_A55_APB0_GPIO1,
    THOMAS_TIGER_A55_APB0_PWM0,
    THOMAS_TIGER_A55_APB0_PWM1,
    THOMAS_TIGER_A55_APB0_PWM2,
    THOMAS_TIGER_A55_APB0_PWM3,
    THOMAS_TIGER_A55_APB0_LSIO_SLCR,
    THOMAS_TIGER_A55_APB0_LSIO_ADC,
    THOMAS_TIGER_A55_APB0_UART5,
    THOMAS_TIGER_A55_APB0_UART6,
    THOMAS_TIGER_A55_APB0_I2C5,
    THOMAS_TIGER_A55_APB0_I2C6,
    THOMAS_TIGER_A55_APB1_DMA,
    THOMAS_TIGER_A55_APB1_WDT,
    THOMAS_TIGER_A55_APB1_ADC,
    THOMAS_TIGER_A55_APB1_TOP_CRM,
    THOMAS_TIGER_A55_APB1_SYS_CTL,
    THOMAS_TIGER_A55_APB1_PVT,
    THOMAS_TIGER_A55_APB1_CRM,
    THOMAS_TIGER_A55_APB1_TIME0,
    THOMAS_TIGER_A55_APB1_TIME1,
    THOMAS_TIGER_AHB_QSPI,
    THOMAS_TIGER_HSIO_GMAC,
    THOMAS_TIGER_HSIO_SDIO0,
    THOMAS_TIGER_HSIO_SDIO1,
    THOMAS_TIGER_HSIO_EMMC,
    THOMAS_TIGER_HSIO_SLCR,
    THOMAS_TIGER_HSIO_USB3,
    THOMAS_TIGER_HSIO_USB2,
    THOMAS_TIGER_DDR_CFG,
    THOMAS_TIGER_BPU,
    THOMAS_TIGER_VEDIO,
    THOMAS_TIGER_GPU,
    THOMAS_TIGER_CAMERA,
    THOMAS_TIGER_DISPLAY,
    THOMAS_TIGER_DDR,
};

#define FPGA_VERSION 6

static const MemMapEntry base_memmap[] = {
#if FPGA_VERSION == 6
    [THOMAS_TIGER_ROM] =               { 0x00000000, 0x00020000 },
    [THOMAS_TIGER_SRAM0] =             { 0x1FE80000, 0x00080000 },
    [THOMAS_TIGER_SRAM1] =             { 0x1FF00000, 0x00100000 },
    [THOMAS_TIGER_AON_SRAM] =          { 0x20000000, 0x00020000 },
    [THOMAS_TIGER_HIFI5_SRAM] =        { 0x20200000, 0x00080000 },
    [THOMAS_TIGER_SECURE] =            { 0x20300000, 0x00100000 },
    [THOMAS_TIGER_EFUSE] =             { 0x20400000, 0x00010000 },
    [THOMAS_TIGER_CPU_NOC] =           { 0x20500000, 0x00010000 },
    [THOMAS_TIGER_NOC_APB] =           { 0x20510000, 0x00070000 },
    [THOMAS_TIGER_CORESIGHT] =         { 0x2D000000, 0x00100000 },
    [THOMAS_TIGER_GIC_DIST] =          { 0x30100000, 0x00010000 },
    /* This redistributor space allows up to 2*64kB*123 CPUs */
    [THOMAS_TIGER_GIC_REDIST] =        { 0x30140000, 0x00F60000 },
    [THOMAS_TIGER_AON_APB] =           { 0x31000000, 0x00080000 },
    [THOMAS_TIGER_DSP_APB] =           { 0x32080000, 0x00100000 },
    [THOMAS_TIGER_MAILBOX] =           { 0x33000000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI0] =     { 0x34000000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI1] =     { 0x34010000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI2] =     { 0x34020000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI3] =     { 0x34030000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI4] =     { 0x34040000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI5] =     { 0x34050000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART0] =    { 0x34060000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART1] =    { 0x34070000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART2] =    { 0x34080000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART3] =    { 0x34090000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART4] =    { 0x340a0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C0] =     { 0x340b0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C1] =     { 0x340c0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C2] =     { 0x340d0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C3] =     { 0x340e0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C4] =     { 0x340f0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LPWM0] =    { 0x34100000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LPWM1] =    { 0x34110000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_GPIO] =     { 0x34120000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_GPIO1] =    { 0x34130000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_PWM0] =     { 0x34140000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_PWM1] =     { 0x34150000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_PWM2] =     { 0x34160000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_PWM3] =     { 0x34170000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LSIO_SLCR] ={ 0x34180000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LSIO_ADC] = { 0x34190000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART5] =    { 0x341a0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART6] =    { 0x341b0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C5] =     { 0x341c0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C6] =     { 0x341d0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_DMA] =      { 0x34200000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_WDT] =      { 0x34210000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_ADC] =      { 0x34190000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_TOP_CRM] =  { 0x34210000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_SYS_CTL] =  { 0x34230000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_PVT] =      { 0x34240000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_CRM] =      { 0x34250000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_TIME0] =    { 0x34260000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_TIME1] =    { 0x34270000, 0x00010000 },
    [THOMAS_TIGER_AHB_QSPI] =          { 0x35000000, 0x00010000 },
    [THOMAS_TIGER_HSIO_GMAC] =         { 0x35010000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SDIO0] =        { 0x35020000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SDIO1] =        { 0x35030000, 0x00010000 },
    [THOMAS_TIGER_HSIO_EMMC] =         { 0x35040000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SLCR] =         { 0x35050000, 0x00010000 },
    [THOMAS_TIGER_HSIO_USB3] =         { 0x35100000, 0x00200000 },
    [THOMAS_TIGER_HSIO_USB2] =         { 0x35300000, 0x00200000 },
    [THOMAS_TIGER_DDR_CFG] =           { 0x36000000, 0x04000000 },
    [THOMAS_TIGER_BPU] =               { 0x3a000000, 0x00020000 },
    [THOMAS_TIGER_VEDIO] =             { 0x3b000000, 0x00030000 },
    [THOMAS_TIGER_GPU] =               { 0x3c000000, 0x00030000 },
    [THOMAS_TIGER_CAMERA] =            { 0x3d000000, 0x000c0000 },
    [THOMAS_TIGER_DISPLAY] =           { 0x3e000000, 0x000b0000 },
    [THOMAS_TIGER_DDR] =               { 0x80000000, 0x180000000 },
#elif FPGA_VERSION == 5
    [THOMAS_TIGER_ROM] =               { 0x00000000, 0x00020000 },
    [THOMAS_TIGER_SRAM0] =             { 0x1FE80000, 0x00080000 },
    [THOMAS_TIGER_SRAM1] =             { 0x1FF00000, 0x00100000 },
    [THOMAS_TIGER_AON_SRAM] =          { 0x20000000, 0x00020000 },
    [THOMAS_TIGER_HIFI5_SRAM] =        { 0x20200000, 0x00080000 },
    [THOMAS_TIGER_SECURE] =            { 0x20300000, 0x00100000 },
    [THOMAS_TIGER_EFUSE] =             { 0x20400000, 0x00010000 },
    [THOMAS_TIGER_CPU_NOC] =           { 0x20500000, 0x00010000 },
    [THOMAS_TIGER_NOC_APB] =           { 0x20510000, 0x00070000 },
    [THOMAS_TIGER_CORESIGHT] =         { 0x30000000, 0x00100000 },
    [THOMAS_TIGER_GIC_DIST] =          { 0x30100000, 0x00010000 },
    /* This redistributor space allows up to 2*64kB*123 CPUs */
    [THOMAS_TIGER_GIC_REDIST] =        { 0x30140000, 0x00F60000 },
    [THOMAS_TIGER_AON_APB] =           { 0x31000000, 0x00060000 },
    [THOMAS_TIGER_DSP_APB] =           { 0x32080000, 0x000d0000 },
    [THOMAS_TIGER_MAILBOX] =           { 0x33000000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI0] =     { 0x34000000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI1] =     { 0x34010000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI2] =     { 0x34020000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI3] =     { 0x34030000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI4] =     { 0x34040000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI5] =     { 0x34050000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART0] =    { 0x34060000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART1] =    { 0x34070000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART2] =    { 0x34080000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART3] =    { 0x34090000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART4] =    { 0x340a0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C0] =     { 0x340b0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C1] =     { 0x340c0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C2] =     { 0x340d0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C3] =     { 0x340e0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C4] =     { 0x340f0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LPWM0] =    { 0x34100000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LPWM1] =    { 0x34110000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_GPIO] =     { 0x34120000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_GPIO1] =    { 0x34130000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_PWM0] =     { 0x34140000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_PWM1] =     { 0x34150000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_PWM2] =     { 0x34160000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_PWM3] =     { 0x34170000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LSIO_SLCR] ={ 0x34180000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_DMA] =      { 0x34200000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_WDT] =      { 0x34210000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_ADC] =      { 0x34190000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_SYS_CTL] =  { 0x34230000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_PVT] =      { 0x34240000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_CRM] =      { 0x34250000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_TIME0] =    { 0x34260000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_TIME1] =    { 0x34270000, 0x00010000 },
    [THOMAS_TIGER_AHB_QSPI] =          { 0x35000000, 0x00010000 },
    [THOMAS_TIGER_HSIO_GMAC] =         { 0x35010000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SDIO0] =        { 0x35020000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SDIO1] =        { 0x35030000, 0x00010000 },
    [THOMAS_TIGER_HSIO_EMMC] =         { 0x35040000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SLCR] =         { 0x35050000, 0x00010000 },
    [THOMAS_TIGER_HSIO_USB3] =         { 0x35100000, 0x00200000 },
    [THOMAS_TIGER_HSIO_USB2] =         { 0x35300000, 0x00200000 },
    [THOMAS_TIGER_DDR_CFG] =           { 0x36000000, 0x04000000 },
    [THOMAS_TIGER_BPU] =               { 0x3a000000, 0x00020000 },
    [THOMAS_TIGER_VEDIO] =             { 0x3b000000, 0x00030000 },
    [THOMAS_TIGER_GPU] =               { 0x3c000000, 0x00030000 },
    [THOMAS_TIGER_CAMERA] =            { 0x3d000000, 0x000c0000 },
    [THOMAS_TIGER_DISPLAY] =           { 0x3e000000, 0x000b0000 },
    [THOMAS_TIGER_DDR] =               { 0x80000000, 0x180000000 },
#else
    [THOMAS_TIGER_ROM] =               { 0x00000000, 0x00020000 },
    [THOMAS_TIGER_SRAM0] =             { 0x20000000, 0x00080000 },
    [THOMAS_TIGER_SRAM1] =             { 0x20080000, 0x00100000 },
    [THOMAS_TIGER_AON_SRAM] =          { 0x20180000, 0x00020000 },
    [THOMAS_TIGER_HIFI5_SRAM] =        { 0x20200000, 0x00080000 },
    [THOMAS_TIGER_SECURE] =            { 0x20300000, 0x00100000 },
    [THOMAS_TIGER_EFUSE] =             { 0x20400000, 0x00010000 },
    [THOMAS_TIGER_CPU_NOC] =           { 0x20500000, 0x00010000 },
    [THOMAS_TIGER_NOC_APB] =           { 0x20510000, 0x00070000 },
    [THOMAS_TIGER_CORESIGHT] =         { 0x30000000, 0x00100000 },
    [THOMAS_TIGER_GIC_DIST] =          { 0x30100000, 0x00010000 },
    /* This redistributor space allows up to 2*64kB*123 CPUs */
    [THOMAS_TIGER_GIC_REDIST] =        { 0x30140000, 0x00F60000 },
    [THOMAS_TIGER_AON_APB] =           { 0x31000000, 0x00060000 },
    [THOMAS_TIGER_DSP_APB] =           { 0x32080000, 0x000d0000 },
    [THOMAS_TIGER_MAILBOX] =           { 0x33000000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI0] =     { 0x34000000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI1] =     { 0x34010000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI2] =     { 0x34020000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI3] =     { 0x34030000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI4] =     { 0x34040000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_SPI5] =     { 0x34050000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART0] =    { 0x34060000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART1] =    { 0x34070000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART2] =    { 0x34080000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART3] =    { 0x34090000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_UART4] =    { 0x340a0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C0] =     { 0x340b0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C1] =     { 0x340c0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C2] =     { 0x340d0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C3] =     { 0x340e0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_I2C4] =     { 0x340f0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LPWM0] =    { 0x34100000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_LPWM1] =    { 0x34110000, 0x00010000 },
    [THOMAS_TIGER_A55_APB0_GPIO] =     { 0x34120000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_DMA] =      { 0x34130000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_WDT] =      { 0x34140000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_ADC] =      { 0x34150000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_SYS_CTL] =  { 0x34160000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_PVT] =      { 0x34180000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_CRM] =      { 0x34190000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_TIME0] =    { 0x341a0000, 0x00010000 },
    [THOMAS_TIGER_A55_APB1_TIME1] =    { 0x341b0000, 0x00010000 },
    [THOMAS_TIGER_AHB_QSPI] =          { 0x35000000, 0x00010000 },
    [THOMAS_TIGER_HSIO_GMAC] =         { 0x35010000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SDIO0] =        { 0x35020000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SDIO1] =        { 0x35030000, 0x00010000 },
    [THOMAS_TIGER_HSIO_EMMC] =         { 0x35040000, 0x00010000 },
    [THOMAS_TIGER_HSIO_SLCR] =         { 0x35050000, 0x00010000 },
    [THOMAS_TIGER_HSIO_USB3] =         { 0x35100000, 0x00200000 },
    [THOMAS_TIGER_HSIO_USB2] =         { 0x35300000, 0x00200000 },
    [THOMAS_TIGER_DDR_CFG] =           { 0x36000000, 0x04000000 },
    [THOMAS_TIGER_BPU] =               { 0x3a000000, 0x00020000 },
    [THOMAS_TIGER_VEDIO] =             { 0x3b000000, 0x00030000 },
    [THOMAS_TIGER_GPU] =               { 0x3c000000, 0x00030000 },
    [THOMAS_TIGER_CAMERA] =            { 0x3d000000, 0x000c0000 },
    [THOMAS_TIGER_DISPLAY] =           { 0x3e000000, 0x000b0000 },
    [THOMAS_TIGER_DDR] =               { 0x80000000, 0x180000000 },
#endif
};

/* Number of external interrupt lines to configure the GIC with */
#define NUM_IRQS 256

#define THOMAS_TIMER_VIRT_IRQ   11
#define THOMAS_TIMER_S_EL1_IRQ  13
#define THOMAS_TIMER_NS_EL1_IRQ 14
#define THOMAS_TIMER_NS_EL2_IRQ 10

#define THOMAS_GIC_MAINT_IRQ  9

/* GIC SPI number, actual number need add 32 */
#define THOMAS_TIGER_UART0_IRQ    (81)
#define THOMAS_TIGER_UART1_IRQ    (82)
#define THOMAS_TIGER_UART2_IRQ    (83)
#define THOMAS_TIGER_UART3_IRQ    (84)
#define THOMAS_TIGER_UART4_IRQ    (85)
#define THOMAS_TIGER_SD_IRQ    (13)

struct THOMASTIGERMachineClass {
    MachineClass parent;
};

struct THOMASTIGERMachineState {
    MachineState parent;

    ARMCPU cpu[2];

    MemoryRegion *flash;
    MemoryRegion *sram0;
    MemoryRegion *sram1;
    MemoryRegion *aon_sram;
    MemoryRegion *hifi_sram;
    MemoryRegion *ddr;

    DeviceState *gic;

    struct arm_boot_info bootinfo;
};

#define TYPE_THOMAS_TIGER_MACHINE MACHINE_TYPE_NAME("thomas-tiger")

OBJECT_DECLARE_TYPE(THOMASTIGERMachineState, THOMASTIGERMachineClass, THOMAS_TIGER_MACHINE)

static void create_gicv3(THOMASTIGERMachineState *mms)
{
    SysBusDevice *gicbusdev;
    MachineState *ms = MACHINE(mms);
    int i;

    unsigned int smp_cpus = ms->smp.cpus;

    mms->gic = qdev_new("arm-gicv3");

    qdev_prop_set_uint32(mms->gic, "revision", 3);
    qdev_prop_set_uint32(mms->gic, "num-cpu", smp_cpus);

    /* Note that the num-irq property counts both internal and external
     * interrupts; there are always 32 of the former (mandated by GIC spec).
     */
    qdev_prop_set_uint32(mms->gic, "num-irq", NUM_IRQS + 32);

    qdev_prop_set_bit(mms->gic, "has-security-extensions", true);

    /* Get redist count*/
    uint32_t redist0_capacity = base_memmap[THOMAS_TIGER_GIC_REDIST].size / GICV3_REDIST_SIZE;
    uint32_t redist0_count = MIN(smp_cpus, redist0_capacity);

    /* One core with one gicr */
    qdev_prop_set_uint32(mms->gic, "len-redist-region-count", 1);
    qdev_prop_set_uint32(mms->gic, "redist-region-count[0]", redist0_count);

    gicbusdev = SYS_BUS_DEVICE(mms->gic);
    sysbus_realize_and_unref(gicbusdev, &error_fatal);

    /* Memory map for GICD and GICR, MR is defined in qemu/hw/intc/arm_gicv3_common.c */
    sysbus_mmio_map(gicbusdev, 0, base_memmap[THOMAS_TIGER_GIC_DIST].base);
    sysbus_mmio_map(gicbusdev, 1, base_memmap[THOMAS_TIGER_GIC_REDIST].base);

    for(i = 0; i < smp_cpus; i++) {
        DeviceState *cpudev = DEVICE(qemu_get_cpu(i));
        int ppibase = NUM_IRQS + i * GIC_INTERNAL + GIC_NR_SGIS;
        int irq;

        const int timer_irq[] = {
            [GTIMER_PHYS] = THOMAS_TIMER_NS_EL1_IRQ,
            [GTIMER_VIRT] = THOMAS_TIMER_VIRT_IRQ,
            [GTIMER_HYP]  = THOMAS_TIMER_NS_EL2_IRQ,
            [GTIMER_SEC]  = THOMAS_TIMER_S_EL1_IRQ,
        };

        for (irq = 0; irq < ARRAY_SIZE(timer_irq); irq++) {
            qdev_connect_gpio_out(cpudev, irq, qdev_get_gpio_in(mms->gic, ppibase + timer_irq[irq]));
        }

        qemu_irq m_irq = qdev_get_gpio_in(mms->gic, ppibase + THOMAS_GIC_MAINT_IRQ);
        qdev_connect_gpio_out_named(cpudev, "gicv3-maintenance-interrupt", 0, m_irq);

        /* Connect cpu and gic */
        sysbus_connect_irq(gicbusdev, i, qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(gicbusdev,  i + smp_cpus, qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
    }
}

#define SDHCI_CAPABILITIES  0x28073ffc1898
static void create_sdhci(hwaddr base, qemu_irq irq)
{
    DeviceState *dev;

    /* sdhci */
    dev = qdev_new(TYPE_SYSBUS_SDHCI);
    qdev_prop_set_uint8(dev, "sd-spec-version", 3);
    qdev_prop_set_uint64(dev, "capareg", SDHCI_CAPABILITIES);
    qdev_prop_set_uint64(dev, "uhs", UHS_I);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq);
    object_property_set_bool(OBJECT(dev), "realized", true, &error_fatal);

    /* sd */
    /* usage: -drive if=sd,file=sd.img,format=raw*/
    DriveInfo *dinfo = drive_get(IF_SD, 0, 0);
    if (dinfo) {
        DeviceState *card;

        card = qdev_new(TYPE_SD_CARD);
        qdev_prop_set_drive_err(card, "drive", blk_by_legacy_dinfo(dinfo),
                                &error_fatal);
        qdev_realize_and_unref(card, qdev_get_child_bus(dev, "sd-bus"),
                               &error_fatal);
    }

}

static void thomas_tiger_common_init(MachineState *machine)
{
    THOMASTIGERMachineState *mms = THOMAS_TIGER_MACHINE(machine);
    unsigned int smp_cpus = machine->smp.cpus;
    int n;

    /* Flash */
    mms->flash = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->flash, NULL, "thomas_tiger.flash", base_memmap[THOMAS_TIGER_ROM].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_TIGER_ROM].base, mms->flash);

    /* Sram */
    mms->sram0 = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram0, NULL, "thomas_tiger.sram0", base_memmap[THOMAS_TIGER_SRAM0].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_TIGER_SRAM0].base, mms->sram0);

    mms->sram1 = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->sram1, NULL, "thomas_tiger.sram1", base_memmap[THOMAS_TIGER_SRAM1].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_TIGER_SRAM1].base, mms->sram1);

    mms->aon_sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->aon_sram, NULL, "thomas_tiger.aon_sram", base_memmap[THOMAS_TIGER_AON_SRAM].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_TIGER_AON_SRAM].base, mms->aon_sram);

    mms->hifi_sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->hifi_sram, NULL, "thomas_tiger.hifi_sram", base_memmap[THOMAS_TIGER_HIFI5_SRAM].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_TIGER_HIFI5_SRAM].base, mms->hifi_sram);

    /* DDR */
    mms->ddr = g_new(MemoryRegion, 1);
    memory_region_init_ram(mms->ddr, NULL, "thomas_tiger.ddr", base_memmap[THOMAS_TIGER_DDR].size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), base_memmap[THOMAS_TIGER_DDR].base, mms->ddr);

    /* Create the actual CPUs */
    for (n = 0; n < smp_cpus; n++) {
        /* Init arm cpu object */
        object_initialize_child(OBJECT(mms), "cpu[*]", &mms->cpu[n], ARM_CPU_TYPE_NAME("cortex-a55"));

        /* All exception levels required */
        qdev_prop_set_bit(DEVICE(&mms->cpu[n]), "has_el3", true);
        qdev_prop_set_bit(DEVICE(&mms->cpu[n]), "has_el2", true);

        /* Mark realized */
        qdev_realize(DEVICE(&mms->cpu[n]), NULL, &error_fatal);
    }

    create_gicv3(mms);

    /* Uart */
    qemu_irq uart0_irq = qdev_get_gpio_in(mms->gic, THOMAS_TIGER_UART0_IRQ);
    serial_mm_init(get_system_memory(), base_memmap[THOMAS_TIGER_A55_APB0_UART0].base, 2,
                   uart0_irq, 115200, serial_hd(1),
                   DEVICE_LITTLE_ENDIAN);
    create_unimplemented_device("dw-apb-uart0", base_memmap[THOMAS_TIGER_A55_APB0_UART0].base + 0x20, 0xe0);


    qemu_irq uart1_irq = qdev_get_gpio_in(mms->gic, THOMAS_TIGER_UART1_IRQ);
    serial_mm_init(get_system_memory(), base_memmap[THOMAS_TIGER_A55_APB0_UART1].base, 2,
                   uart1_irq, 115200, serial_hd(0),
                   DEVICE_LITTLE_ENDIAN);
    create_unimplemented_device("dw-apb-uart1", base_memmap[THOMAS_TIGER_A55_APB0_UART1].base + 0x20, 0xe0);

    /* SD */
    qemu_irq sd_irq = qdev_get_gpio_in(mms->gic, THOMAS_TIGER_SD_IRQ);
    create_sdhci(base_memmap[THOMAS_TIGER_HSIO_SDIO0].base, sd_irq);


    fprintf(stderr, "thomas_common_init kernel name: %s \n", machine->kernel_filename);

    /* Add dummy regions for the devices we don't implement yet,
     * so guest accesses don't cause unlogged crashes.
     */
    create_unimplemented_device("ddr-cfg", base_memmap[THOMAS_TIGER_DDR_CFG].base, base_memmap[THOMAS_TIGER_DDR_CFG].size);
    create_unimplemented_device("aon-apb", base_memmap[THOMAS_TIGER_AON_APB].base, base_memmap[THOMAS_TIGER_AON_APB].size);
    create_unimplemented_device("i2c", base_memmap[THOMAS_TIGER_A55_APB0_I2C0].base, base_memmap[THOMAS_TIGER_A55_APB0_I2C0].size * 5);
    create_unimplemented_device("sysctl", base_memmap[THOMAS_TIGER_A55_APB1_SYS_CTL].base, base_memmap[THOMAS_TIGER_A55_APB1_SYS_CTL].size);
    create_unimplemented_device("dsp-apb", base_memmap[THOMAS_TIGER_DSP_APB].base, base_memmap[THOMAS_TIGER_DSP_APB].size);
    create_unimplemented_device("top-crm", base_memmap[THOMAS_TIGER_A55_APB1_TOP_CRM].base, base_memmap[THOMAS_TIGER_A55_APB1_TOP_CRM].size);
    create_unimplemented_device("dw-apb-uart2", base_memmap[THOMAS_TIGER_A55_APB0_UART2].base, base_memmap[THOMAS_TIGER_A55_APB0_UART2].size);
    create_unimplemented_device("dw-apb-uart3", base_memmap[THOMAS_TIGER_A55_APB0_UART3].base, base_memmap[THOMAS_TIGER_A55_APB0_UART3].size);
    create_unimplemented_device("dw-apb-uart4", base_memmap[THOMAS_TIGER_A55_APB0_UART4].base, base_memmap[THOMAS_TIGER_A55_APB0_UART4].size);
    create_unimplemented_device("dw-apb-uart5", base_memmap[THOMAS_TIGER_A55_APB0_UART5].base, base_memmap[THOMAS_TIGER_A55_APB0_UART5].size);
    create_unimplemented_device("dw-apb-uart6", base_memmap[THOMAS_TIGER_A55_APB0_UART6].base, base_memmap[THOMAS_TIGER_A55_APB0_UART6].size);
    create_unimplemented_device("secure-ip", base_memmap[THOMAS_TIGER_SECURE].base, base_memmap[THOMAS_TIGER_SECURE].size);
    create_unimplemented_device("top-apb0-lsio", base_memmap[THOMAS_TIGER_A55_APB0_LSIO_SLCR].base, base_memmap[THOMAS_TIGER_A55_APB0_LSIO_SLCR].size);
    create_unimplemented_device("top-apb1-crm", base_memmap[THOMAS_TIGER_A55_APB1_CRM].base, base_memmap[THOMAS_TIGER_A55_APB1_CRM].size);

    mms->bootinfo.ram_size = /*256 * MiB;//*/machine->ram_size;
    mms->bootinfo.board_id = -1;
    //mms->bootinfo.nb_cpus = machine->smp.cpus;

    mms->bootinfo.loader_start = base_memmap[THOMAS_TIGER_SRAM1].base;
    mms->bootinfo.smp_loader_start = base_memmap[THOMAS_TIGER_SRAM1].base;

    arm_load_kernel(ARM_CPU(first_cpu), machine, &mms->bootinfo);

}

static void thomas_tiger_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    fprintf(stderr, "thomas_tiger_class_init\n");
    mc->init = thomas_tiger_common_init;
    mc->max_cpus = 256;
    mc->default_ram_size = base_memmap[THOMAS_TIGER_DDR].size;
    mc->default_ram_id = "thomas.ram";

    mc->desc = "ARM THOMAS for Cortex-A55";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a55");
}

static void thomas_tiger_instance_init(Object *obj)
{
    fprintf(stderr, "thomas_tiger_instance_init\n");
}

static const TypeInfo thomas_tiger_info = {
    .name = MACHINE_TYPE_NAME("thomas-tiger"),
    .parent = TYPE_MACHINE,
    .instance_init = thomas_tiger_instance_init,
    .instance_size = sizeof(THOMASTIGERMachineState),
    .class_size = sizeof(THOMASTIGERMachineClass),
    .class_init = thomas_tiger_class_init,
};

static void thomas_tiger_machine_init(void)
{
    type_register_static(&thomas_tiger_info);
}

type_init(thomas_tiger_machine_init);
