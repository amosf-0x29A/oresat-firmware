#include <stdlib.h>
#include <string.h>

#include "command.h"
#include "CO_master.h"
#include "opd.h"
#include "max7310.h"
#include "mmc.h"
#include "chprintf.h"
#include "shell.h"

#ifndef BUF_SIZE
#define BUF_SIZE 0x10000 /* 64k */
#endif

static uint8_t data[BUF_SIZE];

/*===========================================================================*/
/* CAN Network Management                                                    */
/*===========================================================================*/
void nmt_usage(BaseSequentialStream *chp)
{
    chprintf(chp, "Usage: nmt start|stop|preop|resetcomm|resetnode <NodeID>\r\n");
}

void cmd_nmt(BaseSequentialStream *chp, int argc, char *argv[])
{
    uint8_t node_id = 0;
    if (argc < 2) {
        nmt_usage(chp);
        return;
    }
    node_id = strtoul(argv[1], NULL, 0);

    if (!strcmp(argv[0], "start")) {
        CO_sendNMTcommand(CO, CO_NMT_ENTER_OPERATIONAL, node_id);
    } else if (!strcmp(argv[0], "stop")) {
        CO_sendNMTcommand(CO, CO_NMT_ENTER_STOPPED, node_id);
    } else if (!strcmp(argv[0], "preop")) {
        CO_sendNMTcommand(CO, CO_NMT_ENTER_PRE_OPERATIONAL, node_id);
    } else if (!strcmp(argv[0], "resetcomm")) {
        CO_sendNMTcommand(CO, CO_NMT_RESET_COMMUNICATION, node_id);
    } else if (!strcmp(argv[0], "resetnode")) {
        CO_sendNMTcommand(CO, CO_NMT_RESET_NODE, node_id);
    } else {
        chprintf(chp, "Invalid command: %s\r\n", argv[0]);
        nmt_usage(chp);
        return;
    }
}

/*===========================================================================*/
/* CAN SDO Master                                                            */
/*===========================================================================*/
void sdo_usage(BaseSequentialStream *chp)
{
    chprintf(chp, "Usage: sdo read|write <NodeID> <index> <subindex> [blockmode]\r\n");
}

void cmd_sdo(BaseSequentialStream *chp, int argc, char *argv[])
{
    uint32_t data_len = 0;
    uint32_t abrt_code = 0;
    uint8_t node_id = 0;
    uint16_t index = 0;
    uint8_t subindex = 0;
    bool block = false;

    if (argc < 4) {
        sdo_usage(chp);
        return;
    }

    node_id = strtoul(argv[1], NULL, 0);
    index = strtoul(argv[2], NULL, 0);
    subindex = strtoul(argv[3], NULL, 0);
    if (argc == 5)
        block = !strcmp(argv[4], "true");

    if (!strcmp(argv[0], "read")) {
        sdo_upload(CO->SDOclient[0], node_id, index, subindex, data, sizeof(data) - 1, &data_len, &abrt_code, 1000, block);
        data[data_len] = '\0';
        if (abrt_code == CO_SDO_AB_NONE) {
            chprintf(chp, "Received %u bytes of data:", data_len);
            for (uint32_t i = 0; i < data_len; i++)
                chprintf(chp, " %02X", data[i]);
            chprintf(chp, "\r\n");
        } else {
            chprintf(chp, "Received abort code: %x\r\n", abrt_code);
        }
    } else if (!strcmp(argv[0], "write")) {
        chprintf(chp, "Disabled for now\r\n");
        /*sdo_download(CO->SDOclient[0], node_id, index, subindex, data, data_len, &abrt_code, 1000, block);*/
    } else {
        sdo_usage(chp);
        return;
    }
}

/*===========================================================================*/
/* OreSat Power Domain Control                                               */
/*===========================================================================*/
void opd_usage(BaseSequentialStream *chp)
{
    chprintf(chp, "Usage: opd enable|disable|reset|reinit|status <opd_addr>\r\n");
}

void cmd_opd(BaseSequentialStream *chp, int argc, char *argv[])
{
    static opd_addr_t opd_addr = 0;
    opd_status_t status = {0};

    if (argc < 1) {
        opd_usage(chp);
        return;
    } else if (argc > 1) {
        opd_addr = strtoul(argv[1], NULL, 0);
        chprintf(chp, "Setting persistent board address to 0x%02X\r\n", opd_addr);
    } else if (opd_addr == 0) {
        chprintf(chp, "Please specify an OPD address at least once (it will persist)\r\n");
        opd_usage(chp);
        return;
    }

    if (!strcmp(argv[0], "enable")) {
        chprintf(chp, "Enabling board 0x%02X\r\n", opd_addr);
        opd_enable(opd_addr);
    } else if (!strcmp(argv[0], "disable")) {
        chprintf(chp, "Disabling board 0x%02X\r\n", opd_addr);
        opd_disable(opd_addr);
    } else if (!strcmp(argv[0], "reset")) {
        chprintf(chp, "Resetting board 0x%02X\r\n", opd_addr);
        opd_reset(opd_addr);
    } else if (!strcmp(argv[0], "reinit")) {
        chprintf(chp, "Reinitializing OPD\r\n", opd_addr);
        opd_stop();
        opd_init();
        opd_start();
    } else if (!strcmp(argv[0], "status")) {
        chprintf(chp, "Status of board 0x%02X: ", opd_addr);
        if (!opd_status(opd_addr, &status)) {
            chprintf(chp, "CONNECTED\r\n");
            chprintf(chp, "State: %s-%s\r\n",
                    (status.odr & MAX7310_PIN_MASK(OPD_LED) ? "ENABLED" : "DISABLED"),
                    (status.input & MAX7310_PIN_MASK(OPD_FAULT) ? "TRIPPED" : "NOT TRIPPED"));
            chprintf(chp, "Raw register values:\r\n");
            chprintf(chp, "Input:       %02X\r\n", status.input);
            chprintf(chp, "Output:      %02X\r\n", status.odr);
            chprintf(chp, "Polarity:    %02X\r\n", status.pol);
            chprintf(chp, "Mode:        %02X\r\n", status.mode);
            chprintf(chp, "Timeout:     %02X\r\n", status.timeout);
        } else {
            chprintf(chp, "NOT CONNECTED\r\n");
        }
    } else {
        opd_usage(chp);
        return;
    }
}

/*===========================================================================*/
/* Shell                                                                     */
/*===========================================================================*/
static const ShellCommand commands[] = {
    {"nmt", cmd_nmt},
    {"sdo", cmd_sdo},
    {"opd", cmd_opd},
    {"sdc", cmd_sdc},
    {NULL, NULL}
};

static char histbuf[SHELL_MAX_HIST_BUFF];

static const ShellConfig shell_cfg = {
    (BaseSequentialStream *)&SD2,
    commands,
    histbuf,
    sizeof(histbuf),
};

THD_WORKING_AREA(shell_wa, 0x200);
THD_WORKING_AREA(cmd_wa, 0x200);
THD_FUNCTION(cmd, arg)
{
    (void)arg;

    while (!chThdShouldTerminateX()) {
        thread_t *shell_tp = chThdCreateStatic(shell_wa, sizeof(shell_wa), NORMALPRIO, shellThread, (void *)&shell_cfg);
        chThdWait(shell_tp);
        chThdSleepMilliseconds(500);
    }

    chThdExit(MSG_OK);
}
