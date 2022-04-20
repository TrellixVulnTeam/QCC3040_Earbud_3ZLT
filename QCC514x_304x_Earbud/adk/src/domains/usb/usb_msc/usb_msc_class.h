/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Class specific definitions for USB Mass Storage
*/

#ifndef USB_MASS_STORAGE_CLASS_H_
#define USB_MASS_STORAGE_CLASS_H_

/* Universal Serial Bus, Mass Storage Class, Specification Overview (MSCO),
 * Revision 1.4 */

/** Mass Storage Interface Descriptor */
#define B_INTERFACE_CLASS_MASS_STORAGE 0x08
/** SCSI transparent command set  */
#define B_INTERFACE_SUB_CLASS_MASS_STORAGE 0x06
/** USB Mass Storage Class Bulk-Only (BBB) Transport */
#define B_INTERFACE_PROTOCOL_MASS_STORAGE 0x50

/* Universal Serial Bus, Mass Storage Class, Bulk-Only Transport (MSBO),
 * Revision 1.0 */

/** MSBO, 3.1 Bulk-Only Mass Storage Reset */
#define MS_BULK_RESET 0xff
/** MSBO, 3.2 Get Max LUN */
#define MS_GET_MAX_LUN 0xfe

/** MSBO, 5.1 Command Block Wrapper (CBW) signature, byte 0 */
#define CBW_SIGNATURE_B1 0x55
/** MSBO, 5.1 Command Block Wrapper (CBW) signature, byte 1 */
#define CBW_SIGNATURE_B2 0x53
/** MSBO, 5.1 Command Block Wrapper (CBW) signature, byte 2 */
#define CBW_SIGNATURE_B3 0x42
/** MSBO, 5.1 Command Block Wrapper (CBW) signature, byte 3 */
#define CBW_SIGNATURE_B4 0x43
/** MSBO, 5.1 Command Block Wrapper (CBW) size */
#define CBW_SIZE 31

/* MSBO, 5.2 Command Status Wrapper (CSW) signature, byte 0 */
#define CSW_SIGNATURE_B1 0x55
/* MSBO, 5.2 Command Status Wrapper (CSW) signature, byte 1 */
#define CSW_SIGNATURE_B2 0x53
/* MSBO, 5.2 Command Status Wrapper (CSW) signature, byte 2 */
#define CSW_SIGNATURE_B3 0x42
/* MSBO, 5.2 Command Status Wrapper (CSW) signature, byte 3 */
#define CSW_SIGNATURE_B4 0x53
/* MSBO, 5.2 Command Status Wrapper (CSW) size */
#define CSW_SIZE 13


/* MSBO, 5.2 Command Status Wrapper (CSW),
 * Table 5.3 - Command Block Status Values */
typedef enum
{
    CSW_STATUS_PASSED = 0x00,
    CSW_STATUS_FAILED = 0x01,
    CSW_STATUS_PHASE_ERROR = 0x02
} csw_status_t;


/** SCSI Commands Reference Manual (SCSI_CRM), Rev. D, 3.0 Command Reference */
typedef enum
{
    SCSI_TEST_UNIT_READY = 0x00,
    SCSI_REQUEST_SENSE = 0x03,
    SCSI_INQUIRY = 0x12,
    SCSI_MODE_SENSE6 = 0x1a,
    SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL = 0x1e,
    SCSI_READ_FORMAT_CAPACITIES = 0x23,
    SCSI_READ_CAPACITY10 = 0x25,
    SCSI_READ10 = 0x28,
    SCSI_WRITE10 = 0x2a,
    SCSI_VERIFY10 = 0x2f,
    SCSI_MODE_SENSE10 = 0x5a,
    SCSI_READ_CAPACITY16 = 0x9e,
    SCSI_READ12 = 0xa8,
    SCSI_WRITE12 = 0xaa
} scsi_command_t;


/**  SCSI_CRM, 2.4.1.5, Sense key and sense code definitions */
typedef enum
{
    SENSE_ERROR_NO_SENSE = 0x0,
    SENSE_ERROR_RECOVERED_ERROR = 0x1,
    SENSE_ERROR_NOT_READY = 0x2,
    SENSE_ERROR_MEDIUM_ERROR = 0x3,
    SENSE_ERROR_HARDWARE_ERROR = 0x4,
    SENSE_ERROR_ILLEGAL_REQUEST = 0x5,
    SENSE_ERROR_UNIT_ATTENTION = 0x6,
    SENSE_ERROR_DATA_PROTECT = 0x7,
    SENSE_ERROR_BLANK_CHECK = 0x8,
    SENSE_ERROR_VENDOR_SPECIFIC = 0x9,
    SENSE_ERROR_COPY_ABORTED = 0xa,
    SENSE_ERROR_ABORTED_COMMAND = 0xb,
    SENSE_ERROR_OBSOLETE = 0xc,
    SENSE_ERROR_VOLUME_OVERFLOW = 0xd,
    SENSE_ERROR_MISCOMPARE = 0xe
} scsi_sense_key_t;


/** SCSI_CRM, 2.4.1.6, Additional Sense and Additional Sense Qualifier codes */
typedef enum
{
    SENSE_ASC_INVALID_COMMAND_OPCODE = 0x20,
    SENSE_ASCQ_INVALID_COMMAND_OPCODE = 0x00,
    SENSE_ASC_INVALID_FIELD_IN_PARAMETER_LIST = 0x26,
    SENSE_ASCQ_INVALID_FIELD_IN_PARAMETER_LIST = 0x00,
    SENSE_ASC_INVALID_FIELD_IN_CDB = 0x24,
    SENSE_ASCQ_INVALID_FIELD_IN_CDB = 0x00
} scsi_additional_sense_codes_t;


/* SCSI_CRM, 2.4.1 Sense data introduction, Table 12. Sense data response codes */
typedef enum
{
    SENSE_RESPONSE_CURRENT = 0x70,
    SENSE_RESPONSE_DEFERRED = 0x71
} scsi_sense_response_t;


/** SCSI_CRM, Table 279. Mode page codes and subpage codes. */
typedef enum
{
    PAGE_CODE_TIMER_AND_PROTECT_PAGE = 0x1c,
    PAGE_FLEXIBLE_DISK = 0x05,
    PAGE_CODE_CACHING = 0x08,
    PAGE_CODE_ALL_PAGES = 0x3f
} scsi_page_code_t;

#define CBW_FLAG_DIRECTION_TO_HOST 0x80


/** MSBO, 5.1 Command Block Wrapper (CBW)
 *
 * The CBW shall start on a packet boundary and shall end as a
 * short packet with exactly 31 (1Fh) bytes transferred. */
typedef struct
{
    /** Signature that helps identify this data packet as a CBW,
     * shall contain the value 43425355h (little endian). */
    uint8 dCBWSignature[4];
    /** A Command Block Tag sent by the host. The device shall echo the contents
     * of this field back to the host in the dCSWTag field of the associated CSW */
    uint8 dCBWTag[4];
    /** The number of bytes of data that the host expects to transfer
     * on the Bulk-In or Bulk-Out endpoint */
    uint8 dCBWDataTransferLength[4];
    /** The bits of this field are defined as follows:
     * Bit 7 Direction - ignored if the dCBWDataTransferLength field is zero,
     * otherwise: 0 = host-to-device, 1 = device-to-host.
     * Bit 6 Obsolete. The host shall set this bit to zero.
     * Bits 5..0 Reserved - the host shall set these bits to zero. */
    uint8 bCBWFlags[1];
    /** The device Logical Unit Number (LUN) to which the command block
     * is being sent. */
    uint8 bCBWLUN[1];
    /** The valid length of the CBWCB in bytes */
    uint8 bCBWCBLength[1];
    /** The command block to be executed by the device */
    uint8 CBWCB[16];
} usb_msc_cbw_t;


/* MSBO, 5.2 Command Status Wrapper (CSW)
 *
 * The CSW shall start on a packet boundary and shall end as a short packet
 * with exactly 13 (0Dh) bytes transferred. */
typedef struct
{
    /** Signature that helps identify this data packet as a CBW, shall
     * contain the value 43425355h (little endian). */
    uint8 dCSWSignature[4];
    /** The device shall set this field to the value received in the dCBWTag
     * of the associated CBW. */
    uint8 dCSWTag[4];
    /** The difference between dCBWDataTransferLength and the actual amount
     * of data sent/processed by the device. */
    uint8 dCSWDataResidue[4];
    /** Indicates the success or failure of the command. */
    uint8 bCSWStatus[1];
} usb_msc_csw_t;


/* SCSI_CRM, 3.6.2 Standard INQUIRY data,
 * Table 48. Standard INQUIRY data format */
typedef struct
{
    /** b7-b5: Peripheral Qualifier; b4-b0: Peripheral Device Type */
    uint8 Peripheral[1];
    /** b7: removable medium; b6-b0: reserved */
    uint8 Removble[1];
    /** Implemented version of the SPC standard and is defined in SCSI_RMB, table 51. */
    uint8 Version[1];
    /** b7-b6: Obsolete; b5: NORMACA (Access control co-ordinator);
     * b4: HISUP (hierarchical addressing support);
     * b3-b0:2 indicates response is in format defined by spec */
    uint8 Response_Data_Format[1];
    /** Length in bytes of remaining in standard inquiry data */
    uint8 AdditionalLength[1];
    /** b7:SCCS; b6:ACC; b5-b4:TGPS; b3:3PC; b2-b1:Reserved, b0:Protected */
    uint8 Sccstp[1];
    /** b7:bque; b6:EncServ; b5:VS; b4:MultiP; b3:MChngr; b2-b1:Obsolete; b0:Addr16    */
    uint8 bqueetc[1];
    /** b7-b6:Obsolete; b5:WBUS; b4:Sync; b3:Linked; b2:Obsolete; b1:Cmdque; b0:VS */
    uint8 CmdQue[1];
    /** Eight bytes of left-aligned ASCII data identifying the vendor of the product. */
    uint8 vendorID[8];
    /** Sixteen bytes of left-aligned ASCII data. */
    uint8 productID[16];
    /** Four bytes of left-aligned ASCII data */
    uint8 productRev[4];
} usb_msc_inquiry_response_t;

#define SIZE_INQUIRY_RESPONSE sizeof(usb_msc_inquiry_response_t)


/* SCSI_CRM, 2.4.1.2 Fixed format sense data */
typedef struct
{
    /** b7:Valid; b6-b0:Response Code */
    uint8 Valid_ResponseCode[1];
    /** Always set to 0 */
    uint8 Obsolete[1];
    /* b7:Filemark; b6:EOM; b5:ILI; b4:Reserved; b3-b0:Sense Key */
    uint8 SenseKey[1];
    /* Device type or command specific */
    uint8 Information[4];
    /* Number of additional sense bytes */
    uint8 AddSenseLen[1];
    /* Command specific */
    uint8 CmdSpecificInfo[4];
    /* Additional sense code */
    uint8 ASC[1];
    /* Additional sense code qualifier */
    uint8 ASCQ[1];
    /* Field replaceable unit code */
    uint8 FRUC[1];
    /* MSB is SKSV, rest sense key specific */
    uint8 SenseKeySpecific[3];
    /* bytes 18 - n are additional sense bytes, but not defined here */
} usb_msc_request_sense_response_t;

#define SIZE_REQUEST_SENSE_RESPONSE sizeof(usb_msc_request_sense_response_t)


/* SCSI_CRM, 3.25.2 READ CAPACITY (10) parameter data */
typedef struct
{
    /** Logical Block Address (LBA) */
    uint8 LBA[4];
    /** Block length in bytes */
    uint8 BlockLength[4];
} usb_msc_read_capacity10_response_t;

#define SIZE_READ_CAPACITY10_RESPONSE sizeof(usb_msc_read_capacity10_response_t)


/* SCSI_CRM, 3.26.2 READ CAPACITY (16) parameter data */
typedef struct
{
    /** Logical Block Address (LBA) */
    uint8 LBA[8];
    /** Block length in bytes */
    uint8 BlockLength[4];
    /** b7-b4 Reserved; b3-b1 P_TYPE; b1 PROT_EN */
    uint8 ProtPType[1];
    /** Reserved */
    uint8 Reserved[19];
} usb_msc_read_capacity16_response_t;

#define SIZE_READ_CAPACITY16_RESPONSE sizeof(usb_msc_read_capacity16_response_t)


/** INCITS/T10, SCSI Multi-Media Commands, 6.23 READ FORMAT CAPACITIES Command
 * Table 461 — READ FORMAT CAPACITIES Data Format */
typedef struct
{
    uint8 CapacityListHeader[4];
    uint8 CurrentMaximumCapacityHeader[8];
    uint8 FormattableCapacityDescriptor[8];
} usb_msc_read_format_capacities_response_t;

#define SIZE_READ_FORMAT_CAPACITIES_RESPONSE sizeof(usb_msc_read_format_capacities_response_t)


/* SCSI_CRM, 4.3.3Mode parameter header formats,
 * Table 273. Mode parameter header(6) */
typedef struct
{
    uint8 ModeDataLength[1];
    uint8 MediumType[1];
    uint8 DeviceSpecificParam[1];
    uint8 BlockDescriptorLength[1];
} usb_msc_mode_parameter_header_t;

#define SIZE_MODE_PARAM_HEADER sizeof(usb_msc_mode_parameter_header_t)


/* Timer Protect Page Response */
typedef struct
{
    uint8 PageCode[1];
    uint8 PageLength[1];
    uint8 Reserved1[1];
    uint8 InactivityTimeMult[1];
    uint8 Reserved2[4];
} usb_msc_page_timer_protect_response_t;

#define SIZE_PAGE_TIMER_PROTECT_RESPONSE sizeof(usb_msc_page_timer_protect_response_t)

#endif /* USB_MASS_STORAGE_CLASS_H_ */
