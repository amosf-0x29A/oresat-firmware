/*
 * CAN Subsystem header file
 */
#ifndef _CAN_H_
#define _CAN_H_

/*
 * CAN Register configuration
 * See section 22.7.7 on the STM32 reference manual.
 * Timing calculator:
 * http://www.bittiming.can-wiki.info/
 */
static const CANConfig cancfg = {
    // MCR (Master Control Register)
    CAN_MCR_ABOM      |     //Automatic Bus-Off Management
    CAN_MCR_AWUM      |     //Automatic Wakeup Mode
    CAN_MCR_TXFP      ,     //Transmit FIFO Priority
    // BTR (Bit Timing Register)
    // Note: Convert to zero based values here when using the calculator
    // CAN_BTR_LBKM     |     //Loopback Mode (Debug)
    CAN_BTR_SJW(0)    |     //Synchronization Jump Width
    CAN_BTR_TS1(12)   |     //Time Segment 1
    CAN_BTR_TS2(1)    |     //Time Segment 2
    CAN_BTR_BRP(5)          //Bit Rate Prescaler
};

void can_init(void);

void can_start(void);

#endif
