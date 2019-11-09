#include <stdlib.h>
#include <string.h>

#include "mmc.h"
#include "chprintf.h"

#define MMC_BURST_SIZE      2

extern MMCDriver MMCD1;

/* Buffer for block read/write operations, note that extra bytes are
   allocated in order to support unaligned operations.*/
static uint8_t buf[MMCSD_BLOCK_SIZE * MMC_BURST_SIZE + 4];

/* Additional buffer for sdcErase() test */
static uint8_t buf2[MMCSD_BLOCK_SIZE * MMC_BURST_SIZE ];

/*===========================================================================*/
/* SD Card Control                                                           */
/*===========================================================================*/
void cmd_mmc(BaseSequentialStream *chp, int argc, char *argv[]) {
    systime_t start, end;
    uint32_t n, startblk;

    if (argc != 1) {
        chprintf(chp, "Usage: mmc read|write|erase|all\r\n");
        return;
    }

    /* Card presence check.*/
    if (!blkIsInserted(&MMCD1)) {
        chprintf(chp, "Card not inserted, aborting.\r\n");
        return;
    }

    /* Connection to the card.*/
    chprintf(chp, "Connecting... ");
    if (mmcConnect(&MMCD1)) {
        chprintf(chp, "failed\r\n");
        return;
    }

    chprintf(chp, "OK\r\n\r\nCard Info\r\n");
    chprintf(chp, "CSD      : %08X %8X %08X %08X \r\n",
            MMCD1.csd[3], MMCD1.csd[2], MMCD1.csd[1], MMCD1.csd[0]);
    chprintf(chp, "CID      : %08X %8X %08X %08X \r\n",
            MMCD1.cid[3], MMCD1.cid[2], MMCD1.cid[1], MMCD1.cid[0]);
    chprintf(chp, "Capacity : %DMB\r\n", MMCD1.capacity / 2048);

    /* The test is performed in the middle of the flash area.*/
    startblk = (MMCD1.capacity / MMCSD_BLOCK_SIZE) / 2;

    if ((strcmp(argv[0], "read") == 0) ||
            (strcmp(argv[0], "all") == 0)) {

        /* Single block read performance, aligned.*/
        chprintf(chp, "Single block aligned read performance:           ");
        start = chVTGetSystemTime();
        end = chTimeAddX(start, TIME_MS2I(1000));
        n = 0;
        do {
            if (blkRead(&MMCD1, startblk, buf, 1)) {
                chprintf(chp, "failed\r\n");
                goto exittest;
            }
            n++;
        } while (chVTIsSystemTimeWithin(start, end));
        chprintf(chp, "%D blocks/S, %D bytes/S\r\n", n, n * MMCSD_BLOCK_SIZE);

        /* Multiple sequential blocks read performance, aligned.*/
        chprintf(chp, "16 sequential blocks aligned read performance:   ");
        start = chVTGetSystemTime();
        end = chTimeAddX(start, TIME_MS2I(1000));
        n = 0;
        do {
            if (blkRead(&MMCD1, startblk, buf, MMC_BURST_SIZE)) {
                chprintf(chp, "failed\r\n");
                goto exittest;
            }
            n += MMC_BURST_SIZE;
        } while (chVTIsSystemTimeWithin(start, end));
        chprintf(chp, "%D blocks/S, %D bytes/S\r\n", n, n * MMCSD_BLOCK_SIZE);

        /* Single block read performance, unaligned.*/
        chprintf(chp, "Single block unaligned read performance:         ");
        start = chVTGetSystemTime();
        end = chTimeAddX(start, TIME_MS2I(1000));
        n = 0;
        do {
            if (blkRead(&MMCD1, startblk, buf + 1, 1)) {
                chprintf(chp, "failed\r\n");
                goto exittest;
            }
            n++;
        } while (chVTIsSystemTimeWithin(start, end));
        chprintf(chp, "%D blocks/S, %D bytes/S\r\n", n, n * MMCSD_BLOCK_SIZE);

        /* Multiple sequential blocks read performance, unaligned.*/
        chprintf(chp, "16 sequential blocks unaligned read performance: ");
        start = chVTGetSystemTime();
        end = chTimeAddX(start, TIME_MS2I(1000));
        n = 0;
        do {
            if (blkRead(&MMCD1, startblk, buf + 1, MMC_BURST_SIZE)) {
                chprintf(chp, "failed\r\n");
                goto exittest;
            }
            n += MMC_BURST_SIZE;
        } while (chVTIsSystemTimeWithin(start, end));
        chprintf(chp, "%D blocks/S, %D bytes/S\r\n", n, n * MMCSD_BLOCK_SIZE);
    }

    if ((strcmp(argv[0], "write") == 0) ||
            (strcmp(argv[0], "all") == 0)) {
        unsigned i;

        memset(buf, 0xAA, MMCSD_BLOCK_SIZE * 2);
        chprintf(chp, "Writing...");
        if(blkWrite(&MMCD1, startblk, buf, 2)) {
            chprintf(chp, "failed\r\n");
            goto exittest;
        }
        chprintf(chp, "OK\r\n");

        memset(buf, 0x55, MMCSD_BLOCK_SIZE * 2);
        chprintf(chp, "Reading...");
        if (blkRead(&MMCD1, startblk, buf, 1)) {
            chprintf(chp, "failed\r\n");
            goto exittest;
        }
        chprintf(chp, "OK\r\n");

        for (i = 0; i < MMCSD_BLOCK_SIZE; i++)
            buf[i] = i + 8;
        chprintf(chp, "Writing...");
        if(blkWrite(&MMCD1, startblk, buf, 2)) {
            chprintf(chp, "failed\r\n");
            goto exittest;
        }
        chprintf(chp, "OK\r\n");

        memset(buf, 0, MMCSD_BLOCK_SIZE * 2);
        chprintf(chp, "Reading...");
        if (blkRead(&MMCD1, startblk, buf, 1)) {
            chprintf(chp, "failed\r\n");
            goto exittest;
        }
        chprintf(chp, "OK\r\n");
    }

    if ((strcmp(argv[0], "erase") == 0) ||
            (strcmp(argv[0], "all") == 0)) {
        /**
         * Test sdcErase()
         * Strategy:
         *   1. Fill two blocks with non-constant data
         *   2. Write two blocks starting at startblk
         *   3. Erase the second of the two blocks
         *      3.1. First block should be equal to the data written
         *      3.2. Second block should NOT be equal too the data written (i.e. erased).
         *   4. Erase both first and second block
         *      4.1 Both blocks should not be equal to the data initially written
         * Precondition: MMC_BURST_SIZE >= 2
         */
        memset(buf, 0, MMCSD_BLOCK_SIZE * 2);
        memset(buf2, 0, MMCSD_BLOCK_SIZE * 2);
        /* 1. */
        unsigned int i = 0;
        for (; i < MMCSD_BLOCK_SIZE * 2; ++i) {
            buf[i] = (i + 7) % 'T'; //Ensure block 1/2 are not equal
        }
        /* 2. */
        if(blkWrite(&MMCD1, startblk, buf, 2)) {
            chprintf(chp, "mmcErase() test write failed\r\n");
            goto exittest;
        }
        /* 3. (erase) */
        if(mmcErase(&MMCD1, startblk + 1, startblk + 2)) {
            chprintf(chp, "mmcErase() failed\r\n");
            goto exittest;
        }
        if(blkRead(&MMCD1, startblk, buf2, 2)) {
            chprintf(chp, "single-block mmcErase() failed\r\n");
            goto exittest;
        }
        /* 3.1. */
        if(memcmp(buf, buf2, MMCSD_BLOCK_SIZE) != 0) {
            chprintf(chp, "mmcErase() non-erased block compare failed\r\n");
            goto exittest;
        }
        /* 3.2. */
        if(memcmp(buf + MMCSD_BLOCK_SIZE,
                    buf2 + MMCSD_BLOCK_SIZE, MMCSD_BLOCK_SIZE) == 0) {
            chprintf(chp, "mmcErase() erased block compare failed\r\n");
            goto exittest;
        }
        /* 4. */
        if(mmcErase(&MMCD1, startblk, startblk + 2)) {
            chprintf(chp, "multi-block mmcErase() failed\r\n");
            goto exittest;
        }
        if(blkRead(&MMCD1, startblk, buf2, 2)) {
            chprintf(chp, "single-block mmcErase() failed\r\n");
            goto exittest;
        }
        /* 4.1 */
        if(memcmp(buf, buf2, MMCSD_BLOCK_SIZE) == 0) {
            chprintf(chp, "multi-block mmcErase() erased block compare failed\r\n");
            goto exittest;
        }
        if(memcmp(buf + MMCSD_BLOCK_SIZE,
                    buf2 + MMCSD_BLOCK_SIZE, MMCSD_BLOCK_SIZE) == 0) {
            chprintf(chp, "multi-block mmcErase() erased block compare failed\r\n");
            goto exittest;
        }
        /* END of mmcErase() test */
    }

    /* Card disconnect and command end.*/
exittest:
    mmcDisconnect(&MMCD1);
}
