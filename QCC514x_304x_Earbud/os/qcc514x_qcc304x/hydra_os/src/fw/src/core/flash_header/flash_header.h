/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * \defgroup flash_header flash_header
 * \ingroup core
 * \section flash_header_introduction Introduction
 * This module reads information from the flash header so that locations
 * of sections within the flash device can be found by the modules
 * that need them.
 */
#ifndef FLASH_HEADER_H
#define FLASH_HEADER_H
#include "hydra/hydra_types.h"
#include "buffer/buffer_msg.h"
#define FLASH_HEADER_BOOT_IMAGE_DFU_STATUS_0_INDEX        16
#define FLASH_HEADER_BOOT_IMAGE_MAX_DFU_INDEX            256
#define FLASH_HEADER_BOOT_IMAGE_DFU_STATUS_MAX_INDEX \
                         (FLASH_HEADER_BOOT_IMAGE_DFU_STATUS_0_INDEX + \
                                 FLASH_HEADER_BOOT_IMAGE_MAX_DFU_INDEX)

typedef enum
{
    FLASH_SECTION_ID_CFG_RO_FS,
    FLASH_SECTION_ID_DEVICE_RO_FS,
    FLASH_SECTION_ID_RO_FS,
    FLASH_SECTION_ID_RW_CONFIG,
    FLASH_SECTION_ID_RW_FS,
    FLASH_SECTION_ID_RA_PARTITION,
    FLASH_SECTION_ID_DEBUG_PARTITION,
    FLASH_SECTION_ID_APPS_P1,
    FLASH_SECTION_ID_CURATOR_RO_FS,
    FLASH_SECTION_ID_VMODEL_PARTITION, /* Virtual partition */
    FLASH_SECTION_MAX_ID,
    FLASH_SECTION_ID_INVALID
} FLASH_SECTION_ID;


/* The QSPI device to use for Apps P0 boot */
#define FLASH_HEADER_QSPI_DEVICE_0          0

/**
 * Size of the encryption nonce in bytes
 */
#define FLASH_HEADER_NONCE_SIZE_BYTES                (128/8)

/**
 * Size of the encryption nonce in words
 */
#define FLASH_HEADER_NONCE_SIZE_DWORDS \
                               (FLASH_HEADER_NONCE_SIZE_BYTES / sizeof(uint32))

/**
 * Size of the authentication hash in bytes
 */
#define FLASH_HEADER_AUTH_HASH_SIZE_BYTES            (128/8)

/**
 * Size of the authentication hash in words
 */
#define FLASH_HEADER_AUTH_HASH_SIZE_DWORDS \
                           (FLASH_HEADER_AUTH_HASH_SIZE_BYTES / sizeof(uint32))

/**
  * The size of a combined nonce and authentication hash
  * in bytes. The nonce and hash are 128 bits each.
  */
#define FLASH_HEADER_AUTH_HASH_NONCE_SIZE_BYTES \
            (FLASH_HEADER_AUTH_HASH_SIZE_BYTES + FLASH_HEADER_NONCE_SIZE_BYTES)

/**
 * The magic number at the start of the image header is
 * a single dword.
 */
#define FLASH_HEADER_IMAGE_TABLE_MAGIC_SIZE_BYTES    4

/**
 * The image table consists of key,value pairs with the
 * key and value being one dword each.
 */
#define FLASH_HEADER_IMAGE_TABLE_ENTRY_SIZE_BYTES    8

/**
 * The maximum size of the image header table. Used to prevent
 * indefinitely looking through memory if the table gets
 * corrupted.
 */
#define FLASH_HEADER_MAX_IMAGE_TABLE_SIZE            0x1000


/**
 * @brief init_flash_header
 * Initialise the module by reading from the headers in the flash device
 */
void init_flash_header(void);

/**
 * Get the image pointers from the boot image header of the given SQIF
 * device.
 * @param qspi_device The flash device to read from - 0 or 1.
 * @param boot_image TRUE for the boot image or FALSE for the other image
 * @return The address in the SQIF where the nonce and image header
 * are to be found.
 */
uint32 flash_header_get_image_location(uint8 qspi_device, bool boot_image);

/**
 * @brief flash_header_get_p0_code_offset
 * @param offset Return the offset into the flash device (SQIF0)
 * where the P0 code is located.
 * @return TRUE if the offset was present.
 */
bool flash_header_get_p0_code_offset(uint32 * offset);

/**
 * Get the offset of the P1 code from the flash image header
 * @param offset Return the offset into the flash device (either SQIF0
 * or SQIF1) where the P1 image is located.
 * @param flash_device Returns zero if the P1 image is in the main
 * flash memory device (SQIF0) or 1 if it is in the second flash
 * (SQIF1).
 * @return TRUE if the offset was present.
 */
bool flash_header_get_p1_code_offset(uint32 * offset, uint32 * flash_device);

/**
 * Get the offset of the given section within flash device.
 * @param section_id Id of the section.
 * @param offset Return the absolute address of the section.
 * @return TRUE if the section was present in the flash header.
 */
bool flash_header_get_device_section_offset(FLASH_SECTION_ID section_id,
                                            uint32 *offset);

/**
 * Get the offset and size for a given section within the flash device.
 * @param [in]  section_id  Id of the section.
 * @param [out] offset      Returns the absolute address of the section.
 * @param [out] size        Returns the size of the section from the header.
 * @return TRUE if the section was in the flash header, FALSE otherwise.
 */
bool flash_header_get_section_offset_and_size(FLASH_SECTION_ID section_id,
                                              uint32 *offset, uint32 *size);

/**
 * Get offset and size of read only file system.
 * @param section_id Id of the section where the filesystem is stored.
 * @param offset Return the offset of the file system into program memory
 * @param size_bytes Return the size of the file system memory
 * @return TRUE if the offset was present
 */
bool flash_header_get_fs_memory(FLASH_SECTION_ID section_id, uint32 * offset,
                                uint32 * size_bytes);

/**
 * Get details of the flash memory blocks allocated for use by the read/write
 * configuration data. The returned offsets are relative to the start of the
 * flash memory device.
 * @param flash_block_offset Offset to the first block allocated for config
 * @param block_length_bytes Length of the memory allocated to the config
 * @return TRUE if the values were present in the flash image header. If
 * FALSE then the rewritable config is not supported.
 */
bool flash_header_get_psflash_info(uint32 * flash_block_offset,
                                   uint32 * block_length_bytes);

/**
 * @brief flash_header_read_qspi_data
 * Read data from the QSPI using the read decrypt block if it is enabled.
 * @param address Absolute address in the QSPI device to read from.
 * @param length_bytes Number of bytes to read.
 * @param data Address to copy the read data.
 * @param qspi QSPI device from where the data is to be read.
 */
void flash_header_read_qspi_data(uint32 address,
                                     uint16 length_bytes,
                                     uint8 *data,
                                     uint8 qspi);

/**
 * Read raw data from the input QSPI device bypassing the read decrypt
 * block.
 * NOTE: The interrupts are blocked whilst the data is copied from the
 * QSPI device so it is recommended not to read a large number of bytes
 * using this function.
 * This function has to wait for the DMA to be idle so may take
 * some hundreds of microseconds to complete.
 *
 * @param address Absolute address in the QSPI device to read from.
 * @param length_bytes Number of bytes to read.
 * @param data Address to copy the read data.
 * @param qspi QSPI device from where the data is to be read.
 */
extern void flash_header_read_qspi_raw_data(uint32 address,
                                            uint16 length_bytes,
                                            uint8 *data,
                                            uint8 qspi);

/**
 * Read AES decrypted data from the input QSPI device.
 *
 * NOTE: The interrupts are blocked whilst the data is copied from the
 * QSPI device so it is recommended not to read a large number of bytes
 * using this function.
 * This function has to wait for the DMA to be idle so may take
 * some hundreds of microseconds to complete.
 *
 * @param boot_image TRUE if it is the boot image (which must be the
 * currently running image) we are reading. In that case this
 * function is equivalent to \c flash_header_read_qspi_data().
 * Otherwise the DMA is used to decrypt the data using the nonce of
 * the other bank.
 * @param address Absolute address in the QSPI device to read from.
 * @param length_bytes Number of bytes to read.
 * @param data Address to copy the read data.
 * @param qspi_device QSPI device from where the data is to be read.
 */
void flash_header_read_qspi_decrypted_data(bool boot_image,
                                           uint32 address,
                                           uint16 length_bytes,
                                           uint8 *data,
                                           uint8 qspi_device);

/**
 * Read AES decrypted data from the input QSPI device and re-encrypt it.
 *
 * This function re-encrypts the data from the input QSPI device using nonce
 * from the other image bank of the QSPI device.
 *
 * NOTE: The interrupts are blocked whilst the data is copied from the
 * QSPI device so it is recommended not to read a large number of bytes
 * using this function.
 * This function has to wait for the DMA to be idle so may take
 * some hundreds of microseconds to complete.
 *
 * @param qspi_device QSPI device from where the data is to be read.
 * @param address Absolute address in the QSPI device to read from.
 * @param length_bytes Number of bytes to read.
 * @param data Address to re-encrypt and copy the read data.
 * @param enc_address Absolute address in the SQIF device to be
 * used for re-encrypt operation. Should be the final destination
 * SQIF address where re-encrypted data \c data would be written.
 */
void flash_header_read_qspi_boot_image_and_encrypt(uint8 qspi_device,
                                                   uint32 address,
                                                   uint16 length_bytes,
                                                   uint8 *data,
                                                   uint32 enc_address);

/**
 * @brief flash_header_security_is_enabled
 * @return TRUE if security is enabled in the SQIF registers
 */
bool flash_header_security_is_enabled(void);

/**
 * Clear the cached information for the given SQIF device
 * @param qspi_device SQIF device number - 0 or 1
 */
void flash_header_clear_cached_data_for_other_image(uint8 qspi_device);

/**
 * Read the flash boot image table and update the internal state with the
 * locations of the primary and secondary flash images.
 * @param qspi_device Which QSPI to use - 0 or 1.
 * @return TRUE if the flash image table was found and read successfully
 */
bool flash_header_read_flash_image_locations(uint8 qspi_device);

/**
 * Check that the image header table has valid start and end markers
 * @param qspi_device QSPI device from where the data is to be read.
 * @param boot_image TRUE to read from the boot image, FALSE for
 * the other image
 * @return TRUE if the image table has a valid start and end marker
 */
bool flash_header_validate_table(uint8 qspi_device, bool boot_image);

/**
 * @brief flash_header_read_section_params
 * Reads flash header of the input image bank searching for image section keys.
 *
 * NOTE: The section element is not searched in the flash header if the input 
 * address (\c section_offset, \c section_size or \c section_capacity) for 
 * returning the value of the element is null.
 *
 * This function can be also used to validate (search for the expected start 
 * and the end signature) the image header in the QSPI device by passing 
 * a null \c section_id.
 *
 * @param qspi_device QSPI device from where the data is to be read.
 * @param boot_image TRUE to read from the boot image, FALSE for
 * the other image
 * @param section_id ID of the section from the flash_image_defs.h file
 * @param section_offset Returns the absolute SQIF address of the
 * section
 * @param section_size Returns the size of the section as stored in
 * the header 
 * @param section_capacity Returns the capacity of the section as stored in
 * the header
 * @return TRUE if the section information was found and returned
 */
bool flash_header_read_section_params(uint8 qspi_device,
                                      bool boot_image,
                                      uint32 section_id,
                                      uint32 * section_offset,
                                      uint32 * section_size,
                                      uint32 * section_capacity);

/**
 * Read the current DFU_STATUS byte by reading the boot image section of the
 * QSPI 0 device.
 * \param qspi_device The SQIF device to read the status bits from
 * \param dfu_status Address to return the current value of the DFU_STATUS.
 * Can be NULL if not required.
 * \param dfu_status_index Address to return the current DFU_STATUS index.
 * Can be NULL if not required.
 *
 * \return The boot image bank number - 0 or 1.
 *
 */
uint8 flash_header_read_dfu_status(uint8 qspi_device, uint8 *dfu_status,
                                                    uint16 *dfu_status_index);

/**
 * Returns the boot image bank for the input QSPI device.
 *
 * \param qspi_device The SQIF device to return info for.
 * \return The boot image bank number - 0 or 1.
 */
extern uint8 flash_header_get_boot_image(uint8 qspi_device);

/**
 * Returns whether we are running from the boot image bank or the other one.
 * Compares the location the code is running from with the DFU status bits.
 * \param qspi_device The SQIF device to return info for - 0 or 1.
 */
extern bool flash_header_running_from_boot_bank(uint8 qspi_device);

/**
 * Encrypt the given data for the device read only filesystem.
 * If encryption is not enabled then this function does nothing.
 * @param filesystem_id  The ID of the filesystem to potentially encrypt.
 * @param filesystem     A pointer to the filesystem to potentially encrypt.
 * @param length_bytes   The length to potentially encrypt.
 */
void flash_header_encrypt_filesystem(FLASH_SECTION_ID filesystem_id,
                                     uint8 *filesystem, uint32 length_bytes);

/**
 * Authenticate (AES-CBC) the input flash section with either the key provided
 * or from the e-fuse.
 *
 * This function reads the contents of the input flash section from QSPI and 
 * authenticate (AES-CBC) the section.
 *
 * NOTE: The interrupts are blocked whilst the data is read from the QSPI device
 * and this this function has to wait for the DMA to be idle so may take
 * some hundreds of microseconds to complete.
 *
 * @param section Input section for AES-CBC authentication.
 * @param key Address of the key to be used for authentication or NULL to
 *            use e-fuse key.
 * @return TRUE if the section authenticated successfully, else FALSE.
 *
 */
extern bool flash_header_authenticate_section_key(FLASH_SECTION_ID section, uint32 *key);

/**
 * Authenticate (AES-CBC) the input flash section using efuse key from 
 * the hardware.
 *
 * This function reads the contents of the input flash section from QSPI and 
 * authenticate (AES-CBC) the section.
 *
 * NOTE: The interrupts are blocked whilst the data is read from the QSPI device
 * and this this function has to wait for the DMA to be idle so may take
 * some hundreds of microseconds to complete.
 *
 * @param section Input section for AES-CBC authentication.
 * @return TRUE if the section authenticated successfully, else FALSE.
 *
 */
#define flash_header_authenticate_section(section) \
        (flash_header_authenticate_section_key(section, NULL))

/**
 * Read authentication values from the flash header.
 * @param [in]  section      The section ID.
 * @param [out] auth_addr    Returns the absolute flash address the
 *                           authentication starts from.
 * @param [out] auth_len     Returns the number of authenticated bytes starting
 *                           from @p auth_addr in @p section.
 * @param [out] hash_addr    Returns the absolute flash address of the
 *                           nonce/hash tuple.
 * @param [out] section_len  Returns the entire length of @p section in bytes.
 * @return TRUE if each value was successfully read, FALSE otherwise.
 */
bool flash_header_read_auth_values(FLASH_SECTION_ID section, uint32 *auth_addr,
                                   uint32 *auth_len, uint32 *hash_addr,
                                   uint32 *section_len);


/**
 * Utility function to translate P1 direct sqif memory-mapped address to
 * a P0 direct sqif memory-mapped address.
 * @param [in] p1_ptr       Pointer into P1 direct sqif memory space
 * @return Pointer to same data in P0 direct sqif memory space, or NULL
 *         if the data cannot be accessed from P0
 */
void *flash_header_get_p0dsqifptr_from_p1dsqifptr(const void *p1_ptr);

/**
 * Utility function to translate P1 direct sqif memory-mapped address to
 * an absolute sqif address.
 * @param [in] p1_ptr       Pointer into P1 direct sqif memory space
 * @return Absolute sqif pointer
 */
void *flash_header_get_sqifptr_from_p1dsqifptr(const void *p1_ptr);

#ifdef INSTALL_AUDIO_QSPI_UPDATE

/**
 * Platforms on which the audio image running from QSPI (external flash)
 * can be updated.
 */

/* DFU_STATUS in the boot sector of Apps QSPI 0 is used 
 * for Audio QSPI as well. 
 */
#define APPS_QSPI_DEVICE_CONTAINING_AUDIO_IMAGE (FLASH_HEADER_QSPI_DEVICE_0)

/**
 * Start address of the first audio image in the Audio QSPI.
 * It's set to start from address 0.
 */
#define AUDIO_BANK_A_START_ADDR ((uint32)0ul)


/**
 * Initialise the Audio QSPI boot settings if the audio image is running from
 * the QSPI device and can be updated.
 *
 * @return None.
 */
void flash_header_initialise_audio_qspi_boot_settings(void);

/**
 * Get the current running image bank for the image in the Audio QSPI.
 *
 * @return Bank from where the audio is running, 0 - bank A, 1 - bank B.
 */
uint8 flash_header_get_audio_qspi_running_image(void);

#else
#define flash_header_initialise_audio_qspi_boot_settings() ((void)0)
#define flash_header_get_audio_qspi_running_image() ((void)0)
#endif /* INSTALL_AUDIO_QSPI_UPDATE */

#endif /* FLASH_HEADER_H */


