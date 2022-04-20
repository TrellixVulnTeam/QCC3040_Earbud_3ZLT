/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       rafs_file.h
\brief      Implements operations acting on files in rafs

*/

#ifndef RAFS_FILE_H
#define RAFS_FILE_H

/*!
 * \brief Perform some sanity tests on the file open parameters.
 * \param filename  The filename to open
 * \param flags     The flags (read or write)
 * \param file_id   A pointer to where the opened id will be stored.
 * \return RAFS_OK or an error status
 */
rafs_errors_t Rafs_ValidateOpen(const char *filename, rafs_mode_t flags, rafs_file_t *file_id);

/*!
 * \brief Open a file for reading or writing
 * \param task      The application task to receive events relating to this file open
 * \param filename  The filename to open
 * \param flags     The flags (read or write)
 * \param file_id   A pointer to where the opened id will be stored.
 * \return RAFS_OK or an error status
 */
rafs_errors_t Rafs_DoOpen(Task task, const char *filename, rafs_mode_t flags, rafs_file_t *file_id);

/*!
 * \brief Determine if the file id is one we expect.
 * \param file_id        The id
 * \return RAFS_OK or an error status
 */
rafs_errors_t Rafs_ValidateFileIdentifier(rafs_file_t file_id);

/*!
 * \brief Close a file.
 * \param file_id   The id
 * \return RAFS_OK or an error status
 */
rafs_errors_t Rafs_DoClose(rafs_file_t file_id);

/*!
 * \brief Read some data from a file.
 * \param file_id   The id
 * \param buf       A pointer to a buffer to receive data
 * \param num_bytes_to_read     The number of bytes to read
 * \param num_bytes_read        a pointer to actual number of bytes read
 * \return RAFS_OK or an error status
 */
rafs_errors_t Rafs_DoRead(rafs_file_t file_id, void *buf,
                          rafs_size_t num_bytes_to_read, rafs_size_t *num_bytes_read);

/*!
 * \brief Write some data to a file.
 * \param file_id   The id
 * \param buf       A pointer to a buffer for data to be saved
 * \param num_bytes_to_write     The number of bytes to write
 * \param num_bytes_written      a pointer to actual number of bytes written
 * \return RAFS_OK or an error status
 */
rafs_errors_t Rafs_DoWrite(rafs_file_t file_id, const void *buf,
                           rafs_size_t num_bytes_to_write, rafs_size_t *num_bytes_written);

/*!
 * \brief Set the location for the next \see Rafs_Read
 * \param[in] file_id       the file id
 * \param[in] position      the next position in the file to read from
 * \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoSetPosition(rafs_file_t file_id, rafs_size_t position);

/*!
 * \brief Get the location for the next \see Rafs_Read
 * \param[in] file_id       the file id
 * \param[out] position     the next position in the file that would be read from
 * \return RAFS_OK or an error code.
 */
rafs_errors_t Rafs_DoGetPosition(rafs_file_t file_id, rafs_size_t *position);

#endif /* RAFS_FILE_H */
