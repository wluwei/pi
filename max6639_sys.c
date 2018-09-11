/*
 * max6639.c - Support for Maxim MAX6639
 *
 * 2-Channel Temperature Monitor with Dual PWM Fan-Speed Controller
 *
 */

#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include "i2cbusses.h"
#include "util.h"
#include "../version.h"

#include <stdint.h>
#include <stdbool.h>

#define I2C_BUS  1
#define I2C_ADDR 0x2f

// #define ENODEV 124

/* Addresses to scan */
const unsigned short normal_i2c[] = { 0x2c, 0x2e, 0x2f};
/* The MAX6639 registers, valid channel numbers: 0, 1 */
#define MAX6639_REG_TEMP(ch)			(0x00 + (ch))
#define MAX6639_REG_STATUS			0x02
#define MAX6639_REG_OUTPUT_MASK			0x03
#define MAX6639_REG_GCONFIG			0x04
#define MAX6639_REG_TEMP_EXT(ch)		(0x05 + (ch))
#define MAX6639_REG_ALERT_LIMIT(ch)		(0x08 + (ch))
#define MAX6639_REG_OT_LIMIT(ch)		(0x0A + (ch))
#define MAX6639_REG_THERM_LIMIT(ch)		(0x0C + (ch))
#define MAX6639_REG_FAN_CONFIG1(ch)		(0x10 + (ch) * 4)
#define MAX6639_REG_FAN_CONFIG2a(ch)		(0x11 + (ch) * 4)
#define MAX6639_REG_FAN_CONFIG2b(ch)		(0x12 + (ch) * 4)
#define MAX6639_REG_FAN_CONFIG3(ch)		(0x13 + (ch) * 4)
#define MAX6639_REG_FAN_CNT(ch)			(0x20 + (ch))
#define MAX6639_REG_TARGET_CNT(ch)		(0x22 + (ch))
#define MAX6639_REG_FAN_PPR(ch)			(0x24 + (ch))
#define MAX6639_REG_TARGTDUTY(ch)		(0x26 + (ch))
#define MAX6639_REG_FAN_START_TEMP(ch)		(0x28 + (ch))
#define MAX6639_REG_DEVID			0x3D
#define MAX6639_REG_MANUID			0x3E
#define MAX6639_REG_DEVREV			0x3F

/* Register bits */
#define MAX6639_GCONFIG_STANDBY			0x80
#define MAX6639_GCONFIG_POR			0x40
#define MAX6639_GCONFIG_DISABLE_TIMEOUT		0x20
#define MAX6639_GCONFIG_CH2_LOCAL		0x10
#define MAX6639_GCONFIG_PWM_FREQ_HI		0x08

#define MAX6639_FAN_CONFIG1_PWM			0x80

#define MAX6639_FAN_CONFIG3_THERM_FULL_SPEED	0x40

const int rpm_ranges[] = { 2000, 4000, 8000, 16000 };

#define FAN_FROM_REG(val, rpm_range)	((val) == 0 || (val) == 255 ? \
0 : (rpm_ranges[rpm_range] * 30) / (val))

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define TEMP_LIMIT_TO_REG(val)	clamp_val((val) / 1000, 0, 255)

/*
 * Client data (each client gets its own)
 */
typedef struct max6639_data_t {
    int file;
    bool pwm_polarity;	/* Polarity low (0) or high (1, default) */

    /* Register values sampled regularly */
    uint16_t temp[2];		/* Temperature, in 1/8 C, 0..255 C */
    bool temp_fault[2];	/* Detected temperature diode failure */
    uint8_t fan[2];		/* Register value: TACH count for fans >=30 */
    uint8_t status;		/* Detected channel alarms and fan failures */

    /* Register values only written to */
    uint8_t pwm[2];		/* Register value: Duty cycle 0..120 */
    uint8_t temp_therm[2];	/* THERM Temperature, 0..255 C (->_max) */
    uint8_t temp_alert[2];	/* ALERT Temperature, 0..255 C (->_crit) */
    uint8_t temp_ot[2];		/* OT Temperature, 0..255 C (->_emergency) */

    /* Register values initialized only once */
    uint8_t ppr;			/* Pulses per rotation 0..3 for 1..4 ppr */
    uint8_t rpm_range;		/* Index in above rpm_ranges table */
} max6639_data;


void max6639_dump(max6639_data *data);
uint8_t clamp_val(uint8_t val, uint8_t lo, uint8_t hi);
int max6639_update_device(max6639_data *data);
void set_temp_max(max6639_data *data, int channel, uint8_t val);
void set_temp_crit(max6639_data *data, int channel, uint8_t val);
void set_temp_emergency(max6639_data *data, int channel, uint8_t val);
void set_pwm(max6639_data *data, int channel, uint8_t val);
int rpm_range_to_reg(int range);
int max6639_init_client(max6639_data *input, max6639_data *data);
int max6639_detect(int file);
int max6639_suspend(int file);
int max6639_resume(int file);

uint8_t clamp_val(uint8_t val, uint8_t lo, uint8_t hi)
{
    if(val < lo)
        return lo;
    else if(val > hi)
        return hi;
    else
        return hi;
}

void max6639_dump(max6639_data *data)
{
    int i;

    printf("\n   ");
    for (i = 0; i < 0x10; ++i)
    {
        printf("  %x", i);
    }
    printf("\n");

    for (i = 0; i < 0x40; ++i)
    {
        if (i%0x10 == 0)
        {
            printf("%02x: ", i);
        }
        printf("%02x ", (uint8_t)i2c_smbus_read_byte_data(data->file, i));
        if (i%0x10 == 0xf)
        {
            printf("\n");
        }
    }

    printf("\n");
}

int max6639_update_device(max6639_data *data)
{
    int file = data->file;
    int ret;
    int i;
    int status_reg;
    int res;

    printf("\nStarting max6639 update\n");

    status_reg = i2c_smbus_read_byte_data(file, MAX6639_REG_STATUS);
    if (status_reg < 0) {
        printf("Read status reg failed: 0x%2x\n", (uint8_t)status_reg);
        ret = (status_reg);
        goto abort;
    }

    data->status = status_reg;
    printf("Status: %d\n", data->status);

    for (i = 0; i < 2; i++) {
        res = i2c_smbus_read_byte_data(file, MAX6639_REG_FAN_CNT(i));
        if (res < 0) {
            ret = (res);
            goto abort;
        }
        data->fan[i] = res;

        res = i2c_smbus_read_byte_data(file, MAX6639_REG_TEMP_EXT(i));
        if (res < 0) {
            ret = (res);
            goto abort;
        }
        data->temp[i] = res >> 5;
        data->temp_fault[i] = res & 0x01;

        res = i2c_smbus_read_byte_data(file, MAX6639_REG_TEMP(i));
        if (res < 0) {
            ret = (res);
            goto abort;
        }
        data->temp[i] |= res << 3;

        printf("Temp[%d] input: %d\n", i, data->temp[i] * 125);
        // printf("Temp[%d] fault: %d\n", i, data->temp_fault[i]);
        // printf("Temp[%d] max:   %d\n", i, data->temp_therm[i]);
        // printf("Temp[%d] crit:  %d\n", i, data->temp_alert[i]);
        printf("Temp[%d] pwm:   %d\n", i, data->pwm[i] * 255 / 120);
        printf("Temp[%d] alarm: %d\n", i, !!(data->status & (1 << i)));
        printf("fan tach %d %d %d\n", data->fan[i], data->rpm_range, FAN_FROM_REG(data->fan[i], data->rpm_range));
        printf("Fan [%d] input: %d\n", i, FAN_FROM_REG(data->fan[i], data->rpm_range));
        printf("\n");
    }
    return 0;

abort:
    return ret;
}

void set_temp_max(max6639_data *data, int channel, uint8_t val)
{
    int file = data->file;

    data->temp_therm[channel] = TEMP_LIMIT_TO_REG(val);
    i2c_smbus_write_byte_data(file, MAX6639_REG_THERM_LIMIT(channel), data->temp_therm[channel]);
    return;
}

void set_temp_crit(max6639_data *data, int channel, uint8_t val)
{
    int file = data->file;

    data->temp_alert[channel] = TEMP_LIMIT_TO_REG(val);
    i2c_smbus_write_byte_data(file, MAX6639_REG_ALERT_LIMIT(channel), data->temp_alert[channel]);
    return;
}

void set_temp_emergency(max6639_data *data, int channel, uint8_t val)
{
    int file = data->file;

    data->temp_ot[channel] = TEMP_LIMIT_TO_REG(val);
    i2c_smbus_write_byte_data(file, MAX6639_REG_OT_LIMIT(channel), data->temp_ot[channel]);
    return;
}

void set_pwm(max6639_data *data, int channel, uint8_t val)
{
    int file = data->file;

    val = clamp_val(val, 0, 255);

    data->pwm[channel] = (uint8_t)(val * 120 / 255);
    i2c_smbus_write_byte_data(file, MAX6639_REG_TARGTDUTY(channel), data->pwm[channel]);
    return;
}

/*
 *  returns respective index in rpm_ranges table
 *  1 by default on invalid range
 */
int rpm_range_to_reg(int range)
{
    uint8_t i;

    for (i = 0; i < ARRAY_SIZE(rpm_ranges); i++) {
        if (rpm_ranges[i] == range)
            return i;
    }

    return 1; /* default: 4000 RPM */
}

int max6639_init_client(max6639_data *input, max6639_data *data)
{
    int i;
    int err = 0;
    int file = data->file;

    if(!input || !data)
    {
        printf("Null pointer!\n");
        goto exit;
    }

    /* Reset chip to default values, see below for GCONFIG setup */
    err = i2c_smbus_write_byte_data(file, MAX6639_REG_GCONFIG, MAX6639_GCONFIG_POR);
    if (err)
    {
        printf("Reset chip failed!\n");
        goto exit;
    }
    // Wait reset finish, otherwise temperature will read fail all.
    sleep(1);

    /* Fans pulse per revolution is 2 by default */
    if (!(input->ppr > 0 && input->ppr < 5))
        input->ppr = 2; /* default: 4000 RPM */
    data->ppr = input->ppr - 1;

    for (i = 0; i < 2; i++) {

        /* Set Fan pulse per revolution */
        err = i2c_smbus_write_byte_data(file, MAX6639_REG_FAN_PPR(i), data->ppr << 6);
        if (err)
            goto exit;

        /* Fans config PWM, RPM */
        err = i2c_smbus_write_byte_data(file, MAX6639_REG_FAN_CONFIG1(i), MAX6639_FAN_CONFIG1_PWM | input->rpm_range);
        if (err)
            goto exit;
        data->rpm_range = input->rpm_range;

        /* Fans PWM polarity high by default */
        if (input->pwm_polarity == 0)
            err = i2c_smbus_write_byte_data(file, MAX6639_REG_FAN_CONFIG2a(i), 0x00);
        else
            err = i2c_smbus_write_byte_data(file, MAX6639_REG_FAN_CONFIG2a(i), 0x02);
        if (err)
            goto exit;

        /*
         * /THERM full speed enable,
         * PWM frequency 25kHz, see also GCONFIG below
         */
        err = i2c_smbus_write_byte_data(file, MAX6639_REG_FAN_CONFIG3(i), MAX6639_FAN_CONFIG3_THERM_FULL_SPEED | 0x03);
        if (err)
            goto exit;

        err = i2c_smbus_write_byte_data(file, MAX6639_REG_THERM_LIMIT(i), input->temp_therm[i]);
        if (err)
            goto exit;
        err = i2c_smbus_write_byte_data(file, MAX6639_REG_ALERT_LIMIT(i), input->temp_alert[i]);
        if (err)
            goto exit;
        err = i2c_smbus_write_byte_data(file, MAX6639_REG_OT_LIMIT(i), input->temp_ot[i]);
        if (err)
            goto exit;

        err = i2c_smbus_write_byte_data(file, MAX6639_REG_TARGTDUTY(i), input->pwm[i]);
        if (err)
            goto exit;
        data->pwm[i] = input->pwm[i];
    }
    /* Start monitoring */
    err = i2c_smbus_write_byte_data(file, MAX6639_REG_GCONFIG, MAX6639_GCONFIG_DISABLE_TIMEOUT | MAX6639_GCONFIG_CH2_LOCAL | MAX6639_GCONFIG_PWM_FREQ_HI);
exit:
    return err;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
int max6639_detect(int file)
{
    int dev_id, manu_id;

    /* Actual detection via device and manufacturer ID */
    dev_id = i2c_smbus_read_byte_data(file, MAX6639_REG_DEVID);
    manu_id = i2c_smbus_read_byte_data(file, MAX6639_REG_MANUID);
    printf("dev 0x%x, manu 0x%x\n", dev_id, manu_id);
    if (dev_id != 0x58 || manu_id != 0x4D)
        return -ENODEV;

    return 0;
}

#define CONFIG_PM_SLEEP
#ifdef CONFIG_PM_SLEEP
int max6639_suspend(int file)
{
    int data;

    data = i2c_smbus_read_byte_data(file, MAX6639_REG_GCONFIG);
    if (data < 0)
        return data;

    return i2c_smbus_write_byte_data(file, MAX6639_REG_GCONFIG, data | MAX6639_GCONFIG_STANDBY);
}

int max6639_resume(int file)
{
    int data;

    data = i2c_smbus_read_byte_data(file, MAX6639_REG_GCONFIG);
    if (data < 0)
        return data;

    return i2c_smbus_write_byte_data(file, MAX6639_REG_GCONFIG, data & ~MAX6639_GCONFIG_STANDBY);
}
#endif /* CONFIG_PM_SLEEP */


int main (void)
{
    int i2cbus = I2C_BUS;
    int address = I2C_ADDR;
    int file = 0;
    int force = 0;
    int i = 0;
    char filename[20];
    max6639_data input, data;

    memset(&input, 0, sizeof(max6639_data));
    memset(&data, 0, sizeof(max6639_data));
    file = open_i2c_dev(i2cbus, filename, sizeof(filename), 0);
    if (file < 0)
    {
        exit(1);
    }

    if (set_slave_addr(file, address, force))
    {
        exit(1);
    }

    if(max6639_detect(file))
    {
    	printf("No MAX6639 detected!\n");
    	goto abort;
    }

    input.file = data.file = file;
    input.ppr = 2; // 1, 2, 3, 4
    input.rpm_range = 1; //0,1,2,3:2000,4000,8000,16000
    input.pwm_polarity = 1;
    max6639_dump(&data);

    for(i=0; i<2; i++)
    {
        /* Max. temp. 80C/90C/100C */
        input.temp_therm[i] = 80;
        input.temp_alert[i] = 90;
        input.temp_ot[i] = 100;
        // The MAX6639 divides each PWM cycle into 120 time slots. 
        // For example, the PWM1 output duty cycle is 25% when register 26h reads 1Eh (30/120)
        input.pwm[i] = 30;
    }
    if(max6639_init_client(&input, &data))
    {
    	printf("MAX6639 init failed!\n");
    	goto abort;
    }
    printf("MAX6639 initialized with ppr:%d, rpm_range:%d, pwm[0]:%d%%\n",
        input.ppr, rpm_ranges[input.rpm_range], input.pwm[0]*100/120);

    max6639_dump(&data);

    if(max6639_update_device(&data))
    {
    	printf("MAX6639 update failed!\n");
    	goto abort;
    }

    // max6639_dump(&data);

    close(file);
    exit(0);

abort:
    close(file);
    exit(1);
}

