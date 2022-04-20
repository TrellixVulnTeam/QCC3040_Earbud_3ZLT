/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      UCQ string descriptor based on Teams_DevicesGeneralSpecification_4_0_final.pdf.
*/

#include "usb_app_ucq_descriptor.h"

/* According to Version 4.0 of the "Microsoft Teams Devices General Specification",
 * Unified Communications Qualification (UCQ) descriptor, the set of fields sent to
 * Skype for Business by a telephony HID device indicating the capabilities supported
 * by the device. The string is requested by Skype for Business through a standard
 * USB string descriptor request at index 33.
 *
 * The full UCQ string is 17 characters long (including the characters ‘UCQ’).
 * ucq[0-2]  : UCQ
 * ucq[3]    : Display Supported - "1":YES; "0":NO
 * ucq[4]    : Speakerphone      - "1":YES; "0":NO
 * ucq[5]    : Handset           - "1":YES; "0":NO
 * ucq[6]    : Headset           - "1":YES; "0":NO
 * ucq[7]    : AEC               - "1":YES; "0":NO
 * ucq[8-10] : Reserved          - "000"
 * ucq[11]   : Wireless          - "1":YES; "0":NO
 * ucq[12-13]: Skype for Business HID Version Major - "01":2007 R2
 * ucq[14-15]: Skype for Business HID Version Minor - "00":2007 R2
 * ucq[16]   : SIP endpoint - Always "0"
*/
static const uint16 ucq_descriptor[] =
{
    0x0055, /* U */
    0x0043, /* C */
    0x0051, /* Q */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0031, /* 1 */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0031, /* 1 */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0030, /* 0 */
    0x0000
};


const uint16 * UsbApplication_GetDefaultUcqDescriptors(void)
{
    return ucq_descriptor;
}
