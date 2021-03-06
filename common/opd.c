#include "hal.h"
#include "max7310.h"
#include "opd.h"

#define OPD_ADDR_MAX        (MAX7310_MAX_ADDR + 1)

static struct {
    MAX7310Driver dev;
    MAX7310Config config;
    bool valid;
} opd_dev[OPD_ADDR_MAX];

static const I2CConfig i2cconfig = {
    OPMODE_I2C,
    100000,
    STD_DUTY_CYCLE,
};

static MAX7310Config defconfig = {
    &I2CD1,
    &i2cconfig,
    0x00,
    OPD_ODR_VAL,
    OPD_POL_VAL,
    OPD_MODE_VAL,
    MAX7310_TIMEOUT_ENABLED
};

void opd_discover(void)
{
    uint8_t temp;
    i2cAcquireBus(&I2CD1);
    for (i2caddr_t i = MAX7310_MIN_ADDR; i <= MAX7310_MAX_ADDR; i++) {
        if (i2cMasterReceiveTimeout(&I2CD1, i, &temp, 1, TIME_MS2I(10)) == MSG_OK)
            opd_dev[i].valid = true;
        else
            opd_dev[i].valid = false;
    }
    i2cReleaseBus(&I2CD1);
}

void opd_init(void)
{
    for (i2caddr_t i = MAX7310_MIN_ADDR; i <= MAX7310_MAX_ADDR; i++) {
        max7310ObjectInit(&opd_dev[i].dev);
        opd_dev[i].config = defconfig;
        opd_dev[i].config.saddr = i;
    }
    i2cStart(&I2CD1, &i2cconfig);
    opd_discover();
}

void opd_start(void)
{
    for (i2caddr_t i = MAX7310_MIN_ADDR; i <= MAX7310_MAX_ADDR; i++) {
        if (opd_dev[i].valid)
            max7310Start(&opd_dev[i].dev, &opd_dev[i].config);
    }
}

void opd_stop(void)
{
    for (i2caddr_t i = MAX7310_MIN_ADDR; i <= MAX7310_MAX_ADDR; i++) {
        max7310Stop(&opd_dev[i].dev);
    }
    i2cStop(&I2CD1);
}

void opd_enable(opd_addr_t opd_addr)
{
    if (opd_dev[opd_addr].valid != true)
        return;
    uint8_t regval;
    regval = max7310ReadRaw(&opd_dev[opd_addr].dev, MAX7310_AD_ODR);
    regval |= MAX7310_PIN_MASK(OPD_LED);
    max7310WriteRaw(&opd_dev[opd_addr].dev, MAX7310_AD_ODR, regval);
}

void opd_disable(opd_addr_t opd_addr)
{
    if (opd_dev[opd_addr].valid != true)
        return;
    uint8_t regval;
    regval = max7310ReadRaw(&opd_dev[opd_addr].dev, MAX7310_AD_ODR);
    regval &= ~MAX7310_PIN_MASK(OPD_LED);
    max7310WriteRaw(&opd_dev[opd_addr].dev, MAX7310_AD_ODR, regval);
}

void opd_reset(opd_addr_t opd_addr)
{
    uint8_t regval;
    regval = max7310ReadRaw(&opd_dev[opd_addr].dev, MAX7310_AD_ODR);
    regval |= MAX7310_PIN_MASK(OPD_CB_RESET);
    max7310WriteRaw(&opd_dev[opd_addr].dev, MAX7310_AD_ODR, regval);
    chThdSleepMilliseconds(10);
    regval &= ~MAX7310_PIN_MASK(OPD_CB_RESET);
    max7310WriteRaw(&opd_dev[opd_addr].dev, MAX7310_AD_ODR, regval);
}

int opd_status(opd_addr_t opd_addr, opd_status_t *status)
{
    if (opd_dev[opd_addr].valid != true)
        return -1;
    status->input = max7310ReadRaw(&opd_dev[opd_addr].dev, MAX7310_AD_INPUT);
    status->odr = max7310ReadRaw(&opd_dev[opd_addr].dev, MAX7310_AD_ODR);
    status->pol = max7310ReadRaw(&opd_dev[opd_addr].dev, MAX7310_AD_POL);
    status->mode = max7310ReadRaw(&opd_dev[opd_addr].dev, MAX7310_AD_MODE);
    status->timeout = max7310ReadRaw(&opd_dev[opd_addr].dev, MAX7310_AD_TIMEOUT);
    return 0;
}
