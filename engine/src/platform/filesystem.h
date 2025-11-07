#pragma once

#include "defines.h"

typedef struct file_handle {
    void *handle;
    b8 is_valid;
} file_handle;

typedef enum file_modes {
    FILE_MODE_READ = 0x1,
    FILE_MODE_WRITE = 0x2
} file_modes;

/**
 * @brief Checks if a file within the given path exists.
 *
 * @param path The path to be checked.
 * @return True if it exists; otherwise False.
 */
KAPI b8 filesystem_exists(const char *path);

/**
 * @brief Attemp to open file located at this path.
 *
 * @param path The path of the file to be opened
 * @param mode Mode flags for the file when opened (read/write).
 * @param binary Indicates if the file should be opened in binary mode.
 * @param out_handle A pointer to a file_handle struct.
 * @return True if opened successfully; otherwise False.
 */
KAPI b8 filesystem_open(const char *path, file_modes mode, b8 binary,
                        file_handle *out_handle);

/**
 * @brief Closes the provided handle to a file.
 *
 * @param handle A pointer to a file_handle struct.
 */
KAPI void filesystem_close(file_handle *handle);

/**
 * @brief Reads up to a newline or EOF, allocated line_buf must be freed by the
 * caller.
 *
 * @param handle A pointer to a file_handle struct.
 * @param line_buf A pointer to a character array which will be allocated and
 * populated by this method.
 * @return True if successful; otherwise False.
 */
KAPI b8 filesystem_read_line(file_handle *handle, char **line_buf);

/**
 * @brief Writes text to a provided file, appending `\n` to the end.
 *
 * @param handle A pointer to a file_handle struct.
 * @param text The text to be written.
 * @return True if successful; otherwise False.
 */
KAPI b8 filesystem_write_line(file_handle *handle, const char *text);

/**
 * @brief Reads up to data_size bytes of data into out_data. Allocates
 * *out_data which must be freed by the caller.
 *
 * @param handle A pointer to a file_handle struct.
 * @param data_size The number of bytes to read.
 * @param out_data A pointer to a block of memory to be populated by this
 * method.
 * @param out_bytes_read A pointer to a number which will be populated with the
 * actually byte amount read.
 * @return True if successful; otherwise False.
 */
KAPI b8 filesystem_read(file_handle *handle, u64 data_size, void *out_data,
                        u64 *out_bytes_read);

/**
 * @brief Reads all bytes of a file.
 *
 * @param handle A pointer to a file_handle struct.
 * @param out_bytes A pointer to a byte array which will be allocated and
 * populated by this function. It must be freed by the caller.
 * @param out_bytes_read A pointer to a number which will be populated with the
 * actually byte amount read.
 * @return True if successful; otherwise False.
 */
KAPI b8 filesystem_read_all_bytes(file_handle *handle, u8 **out_bytes,
                                  u64 *out_bytes_read);

/**
 * @brief Writes provided data to a file.
 *
 * @param handle A pointer to a file_handle struct.
 * @param data_size The size of the data to be written in bytes.
 * @param data The data to be written.
 * @param out_bytes_written A pointer to a number which will be populated with
 * the actually byte amount written.
 * @return
 */
KAPI b8 filesystem_write(file_handle *handle, u64 data_size, const void *data,
                         u64 *out_bytes_written);
