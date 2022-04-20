/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs.h
\defgroup   rafs
\brief      Provides a very simple read/write file system interface to store, retrieve and remove user content.

*/

#ifndef RAFS_H
#define RAFS_H

#include <domain_message.h>

/*\{*/

/*!
 * \brief The version number of RAFS
 * This should be incremented whenever there is a non compatible change
 * to any persistent RAFS data stored on the flash, such as the dir_entry structure.
 *
 * \note
 * This potentially also means changes in the toolchain used to build the module.
*/
#define RAFS_VERSION        "001"

#define RAFS_MAX_OPEN_FILES 4   /*!< The number of files that can be opened at once */

#define RAFS_MAX_OPEN_DIRS  1   /*!< The number of directories that can be opened at once */

#define RAFS_MAX_FILELEN    16  /*!< Maximum length of a filename, INCLUDING the \0 */

#define RAFS_INVALID_FD     -1  /*!< The invalid file id */

#define RAFS_MOUNTPOINT_LEN 8   /*!< Maximum length of a mount point name, INCLUDING the \0 */

#define RAFS_PATH_SEPARATOR "/" /*!< The path and directory separator */

/*! \brief The possible errors that can result from various RAFS operations */
typedef enum {
    RAFS_OK,
    RAFS_MAX_FILES_OPENED,  /*!< Maximum number of open files */
    RAFS_BAD_ID,            /*!< Bad file or directory id */
    RAFS_FILE_NOT_FOUND,    /*!< No such filename */
    RAFS_FILE_SYSTEM_FULL,  /*!< No space left or FAT is full */
    RAFS_FILE_FULL,         /*!< There is no more space for inodes in the current file */
    RAFS_FAT_WRITE_FAILED,  /*!< Failure to update the FAT when closing a file for writing */
    RAFS_FILE_EXISTS,       /*!< File exists (opening for writing a 2nd time) */
    RAFS_INVALID_LENGTH,    /*!< Filename too long or too short */
    RAFS_INVALID_CHARACTER, /*!< A character not in [A-Za-z0-9._] */
    RAFS_INVALID_PATH,      /*!< The absolute pathname is invalid */
    RAFS_BUSY,              /*!< Waiting for previous background activity to complete */
    RAFS_INVALID_FAT,       /*!< Invalid FAT table */
    RAFS_INVALID_PARTITION, /*!< Invalid partition name */
    RAFS_NO_PARTITION,      /*!< No partition / partition manager */
    RAFS_NOT_MOUNTED,       /*!< No partition has been mounted */
    RAFS_ALREADY_MOUNTED,   /*!< Partition is already mounted */
    RAFS_STILL_MOUNTED,     /*!< Partition is still mounted */
    RAFS_FILE_STILL_OPEN,   /*!< File(s) is/are still open */
    RAFS_DIRECTORY_OPEN,    /*!< An OpenDirectory is already active */
    RAFS_NO_MORE_FILES,     /*!< ReadDirectory reached the end of the directory */
    RAFS_NO_MORE_DATA,      /*!< A file read returned zero bytes of data - end of file */
    RAFS_INVALID_SEEK,      /*!< The seek position is outside the bounds of the file */
    RAFS_NOT_SEEKABLE,      /*!< Files opened for writing are not seekable in the current RAFS */
    RAFS_UNSUPPORTED_IOC,   /*!< The parameter set to \see Rafs_IoControl is not supported */
    RAFS_LOW_POWER,         /*!< There is insufficient power to start a new write operation */
    RAFS_FILE_SYSTEM_CLEAN, /*!< There is no work for \see Rafs_Compact to do - it's clean */
    RAFS_LAST
} rafs_errors_t;

/*! \brief The mode for opening the file */
typedef enum {
    RAFS_RDONLY,            /*!< The file is opened for reading */
    RAFS_WRONLY             /*!< The file is opened for writing */
} rafs_mode_t;

/*! \brief Message responses to various background processes of RAFS */
typedef enum {
    MESSAGE_RAFS_MOUNT_COMPLETE = INTERNAL_MESSAGE_BASE,
    MESSAGE_RAFS_COMPACT_COMPLETE,
    MESSAGE_RAFS_WRITE_COMPLETE,
    MESSAGE_RAFS_REMOVE_COMPLETE,
    MESSAGE_RAFS_LAST
} rafs_message_id_t;

typedef uint32  rafs_size_t;    /*!< Used to store the sizes */

typedef int32   rafs_file_t;    /*!< The open file id */

typedef enum {
    INVALID_DIR,
    VALID_DIR,
} rafs_dir_t;                   /*!< The open directory id */

typedef struct {
    rafs_errors_t   status;
} rafs_mount_complete_cfm;

typedef struct {
    rafs_errors_t   status;
    uint32          num_free_slots; /*!< Number of free directory slots */
} rafs_compact_complete_cfm;

typedef struct {
    rafs_errors_t   result;         /*!< The result of the write operation */
    rafs_file_t     file_id;        /*!< The file id this write confirmation relates to */
    const void      *buf;           /*!< The original input buffer pointer */
    rafs_size_t     request_count;  /*!< The number of bytes requested to be written */
    rafs_size_t     write_count;    /*!< Number of bytes written in the request */
} rafs_write_complete_cfm;

typedef struct {
    rafs_errors_t   status;
} rafs_remove_complete_cfm;

/*!
  * \brief Record a sense of 'time' for each file.
  *
  * In a system without a real time clock, the timestamps are really just counters.
  * But these will at least provide the opportunity to compare one file's relative 'age'
  * with another to determine which is the more recent file.
  *
  * The 'created' field is a simple incrementing counter that increments with each
  * new file created.
  * Note that this counter is subject to being adjusted by \see Rafs_Compact(), so applications
  * should not make a persistent record of this value.
  *
  * The 'accessed' field is a shift counter; Internally, the initial value of all 1's is
  * successively reduced by clearing one of the bits each time the file is opened.
  * At the API level, the application sees a number increasing by the addition of powers
  * of two, with the sequence 0,1,3,7,15,31...
  * Note that once this counter reaches 65535(0xffff), it will stick at that value.
  */
typedef struct {
    uint16      sequence_count;         /*!< When the file was created, relative to other files */
    uint16      access_count;           /*!< How often the file has been read, since it was created */
} stat_counters_t;

/*!
  The structure returned by a call to the /see Rafs_ReadDirectory call.
 */
typedef struct {
    char        filename[RAFS_MAX_FILELEN]; /*!< Always \0 terminated */
    uint32      file_size;                  /*!< in bytes */
    stat_counters_t      stat_counters;     /*!< the relative ages of the file */
} rafs_stat_t;

/*! The result of the Rafs_Statfs
 *
 * \note the file_space is the sum of the N largest contiguous blocks,
 * where N is the internally configured number of inodes.
 */
typedef struct {
    uint32      sector_size;        /*!< The sector size of the underlying partition - in bytes */
    uint32      partition_size;     /*!< The partition size of the underlying partition - in bytes */
    uint32      block_size;         /*!< The block size of the underlying partition - in bytes */
    rafs_size_t available_space;    /*!< what would be available immediately after a format - in bytes */
    rafs_size_t free_space;         /*!< all the blocks not presently in use by the FAT and files - in bytes */
    rafs_size_t contiguous_space;   /*!< the largest file in a single extent - in bytes */
    rafs_size_t file_space;         /*!< the largest file that could be presently created - in bytes */
    uint16      max_dir_entries;    /*!< Maximum number of entries in the directory */
    uint16      num_dir_entries;    /*!< Current number of stored files */
} rafs_statfs_t;

/*! Determine the kind of \see Rafs_Format to be performed.
 */
typedef enum {
    format_normal,  /*!< Will only format if the file system is presently unmounted */
} rafs_format_type_t;

/*!
 * The IO control interface is a generalised ad-hoc mechanism for
 * interacting with RAFS to access 'out of band' information.
 *
 * This enumeration lists the possible types that can be used with the
 * \see Rafs_IoControl API.
 */
typedef enum {
    RAFS_IOC_GET_PARTITION_HANDLE,  /*!< Get the handle to the mounted ra_partition */
    /*!<
     * in -         Pointer to partition name.
     * in_size -    Length of that string.
     * out -        Pointer to a raPartition_handle.
     * out_size -   The value sizeof(raPartition_handle).
     * out_length - Pointer to a rafs_size_t for the actual written amount.
    */

    RAFS_IOC_GET_FILE_OFFSET,       /*!< Get the absolute offset of a given file within the ra_partition */
    /*!<
     * in -         Pointer to absolute path of the filename.
     * in_size -    Length of that string.
     * out -        Pointer to a uint32.
     * out_size -   The value sizeof(uint32).
     * out_length - Pointer to a rafs_size_t for the actual written amount.
    */

    RAFS_IOC_SET_APP_STATUS_IDLE_CB,/*!< Set an application state callback function */
    /*!<
     * in -         Pointer to a \see rafs_ioc_set_app_status_idle_cb_t structure.
     * in_size -    Size of the structure.
     * out -        NULL
     * out_size -   0
     * out_length - NULL
    */
} rafs_ioc_type_t;

typedef struct {
    bool    (*appIsIdle)(void*);/*!< An application function that returns true if the application is in an idle state */
    void    *context;           /*!< An application context pointer to pass to the appIsIdle function */
} rafs_ioc_set_app_status_idle_cb_t;

/* === */
/* API */
/* === */
/*! \brief Opens a file for reading or writing.
    \param[in]  task        The task that will receive messages relating to operations on this file.
                            This may be NULL if the application makes no use of \see Rafs_WriteBackground.
                            RAFS message IDs are not globally unique, a dedicated handler is required.
    \param[in]  pathname    The pathname (ie /partition/filename) of the file to open.
    \param[in]  flags       One of RAFS_RDONLY or RAFS_WRONLY.
    \param[out] file_id     A pointer to where to store the open file id.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_Open(Task task, const char *pathname, rafs_mode_t flags, rafs_file_t *file_id);


/*! \brief Close a file.
    \param[in]  file_id     The id to close.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_Close(rafs_file_t file_id);


/*! \brief Read from a file.
    \param[in]  file_id     The file id to read from.
    \param[out] buf         Where the read data will be stored.
    \param[in]  num_bytes_to_read   The number of bytes to read.
    \param[out] num_bytes_read      A pointer to actual number of bytes read.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_Read(rafs_file_t file_id, void *buf,
                        rafs_size_t num_bytes_to_read, rafs_size_t *num_bytes_read);


/*! \brief  Write to a file.
    \param[in]  file_id     The file id to write to.
    \param[in]  buf         The data to be written.
    \param[in]  num_bytes_to_write  The number of bytes to write.
    \param[out] num_bytes_written   A pointer to actual number of bytes written.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_Write(rafs_file_t file_id, const void *buf,
                         rafs_size_t num_bytes_to_write, rafs_size_t *num_bytes_written);


/*! \brief  Write to a file in the background.
    \param[in]  file_id     The file id to write to.
    \param[in]  buf         The data to be written.
    \param[in]  num_bytes_to_write  The number of bytes to write.
    \return RAFS_OK or an error code.
    \note MESSAGE_RAFS_WRITE_COMPLETE will be sent to the task registered with the \see Rafs_Open call.
    \note The application MUST ensure that buf remains valid until the complete message is received.
    \note If a result other than RAFS_OK is returned, no MESSAGE_RAFS_WRITE_COMPLETE will be sent.
 */
rafs_errors_t Rafs_WriteBackground(rafs_file_t file_id, const void *buf, rafs_size_t num_bytes_to_write);


/*!
 * \brief Set the location for the next \see Rafs_Read.
 * \param[in] file_id       The file id.
 * \param[in] position      The next position in the file to read from.
 * \return RAFS_OK or an error code.
 * \note This is currently only supported for files opened for reading.
 */
rafs_errors_t Rafs_SetPosition(rafs_file_t file_id, rafs_size_t position);


/*!
 * \brief Get the location for the next \see Rafs_Read
 * \param[in] file_id       The file id
 * \param[out] position     The next position in the file that would be read from
 * \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_GetPosition(rafs_file_t file_id, rafs_size_t *position);


/*! \brief Open a directory to read information about each file.
    \param[in] path         The path to the directory (ie "/partition/" ).
    \param[out] dir_id      A pointer to where to store the opened directory id.
    \param[out] num         A pointer to where to store the number of directory entries in the opened directory.
    \return RAFS_OK or an error code.

    \note In this implementation of RAFS, there is only a single 'root' directory which is specified
    by appending a single '/' to the end of the partition name.
    The implementation of sub-directories, along with the required 'MakeDirectory' and 'RemoveDirectory'
    API calls may be the subject of a future release of RAFS.
 */
rafs_errors_t Rafs_OpenDirectory(const char *path, rafs_dir_t *dir_id, uint16 *num);


/*! \brief Read the next directory entry.
    \param[in] dir_id       The directory id.
    \param[out] stat        A pointer to a stat structure to receive information.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_ReadDirectory(rafs_dir_t dir_id, rafs_stat_t *stat);


/*! \brief Close a directory.
    \param[in] dir_id       The directory id to close.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_CloseDirectory(rafs_dir_t dir_id);


/*! \brief  Removes a file.
    \param[in]  task        The task that will receive the MESSAGE_RAFS_REMOVE_COMPLETE message.
                            This may be NULL, but the application would have to deal with \see RAFS_BUSY errors in the meantime.
                            RAFS message IDs are not globally unique, a dedicated handler is required.
    \param[in]  pathname    The pathname (ie /partition/filename) of the file to open.
    \return RAFS_OK or an error code.
    \note If the result is an error, no MESSAGE_RAFS_REMOVE_COMPLETE is sent.
 */
rafs_errors_t Rafs_Remove(Task task, const char *pathname);


/*! \brief  Format the RA partition with the RAFS file system.
    \param[in]  partition_name  Identify the partition to be formatted.
    \param[in]  type            The type of format to be performed.
    \return RAFS_OK or an error code.
    \warning All existing data WILL be lost.
 */
rafs_errors_t Rafs_Format(const char *partition_name, rafs_format_type_t type);


/*! \brief  Trigger compaction of the FAT table and/or the files.
    \param[in]  task        The task that will receive the MESSAGE_RAFS_COMPACT_COMPLETE message.
                            This may be NULL, but the application would have to deal with \see RAFS_BUSY errors in the meantime.
                            RAFS message IDs are not globally unique, a dedicated handler is required.
    \param[in]  partition_name  Identify the partition to be compacted.
    \return RAFS_OK or an error code.
    \note If the result is an error, no MESSAGE_RAFS_COMPACT_COMPLETE is sent.
    \note This only compacts the FAT table in this implementation.
    \note This will modify the sequence count of files.
 */
rafs_errors_t Rafs_Compact(Task task, const char *partition_name);


/*! \brief Rafs_Mount is used to mount a file system for use
            The initial check is a quick basic validation to verify that an intact file system is present.
            The extended checks happen in the background and result in a message to the controlling task on completion.
    \param[in]  task        The task that will receive the MESSAGE_RAFS_MOUNT_COMPLETE message.
                            This may be NULL, but the application would have to deal with \see RAFS_BUSY errors in the meantime.
                            RAFS message IDs are not globally unique, a dedicated handler is required.
    \param[in] partition_name   Identify the partition to be mounted.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_Mount(Task task, const char *partition_name);


/*! \brief Rafs_Unmount is used to unmount a file system
    \param[in] partition_name   Identify the partition to be unmounted.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_Unmount(const char *partition_name);


/*! \brief  Obtain information about the file system.
        Total available space - what would be available immediately after a format.
        Total free space - all the blocks not presently in use by the FAT and files.
        Max file size - the largest file that could be presently created
    \param[in] partition_name   Identify the partition to be examined.
    \param[out] buf             A pointer to a buffer where file system info will be written.
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_Statfs(const char *partition_name, rafs_statfs_t *buf);


/*! \brief  This performs the initialisation of the RAFS.
    \param[in] task     The initialisation task (unused by RAFS)
    \return True if all is well, False otherwise.
 */
bool Rafs_Init(Task task);


/*! \brief  This shuts down the RAFS
    \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_Shutdown(void);


/*!
 * \brief Rafs_IoControl
 * This function allows applications to access specific bits of information in an ad-hoc way,
 * but through a consistent API.
 * \param[in] type          The type of operation to be performed.
 * \param[in] in            A pointer to operation specific input data.
 * \param[in] in_size       The size of the operation specific input data.
 * \param[out] out          A pointer to operation specific output data.
 * \param[in] out_size      The size of the operation specific output data.
 * \param[out] out_length   The actual amount of data written to out.
 * \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_IoControl(rafs_ioc_type_t type, const void *in, const rafs_size_t in_size,
                             void *out, const rafs_size_t out_size, rafs_size_t *out_length);

/*\}*/

#endif /* RAFS_H */
