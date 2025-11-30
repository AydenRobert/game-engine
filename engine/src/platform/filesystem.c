#include "platform/filesystem.h"

#include "core/kmemory.h"
#include "core/logger.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

b8 filesystem_exists(const char *path) {
#ifdef _MSC_VER
    struct _stat buffer;
    return _stat(path, &buffer);
#else
    struct stat buffer;
    return stat(path, &buffer) == 0;
#endif
}

b8 filesystem_open(const char *path, file_modes mode, b8 binary,
                   file_handle *out_handle) {
    out_handle->is_valid = false;
    out_handle->handle = 0;
    const char *mode_str;

    if ((mode & FILE_MODE_READ) != 0 && (mode & FILE_MODE_WRITE) != 0) {
        mode_str = binary ? "w+b" : "w+";
    } else if ((mode & FILE_MODE_READ) != 0 && (mode & FILE_MODE_WRITE) == 0) {
        mode_str = binary ? "rb" : "r";
    } else if ((mode & FILE_MODE_READ) == 0 && (mode & FILE_MODE_WRITE) != 0) {
        mode_str = binary ? "wb" : "w";
    } else {
        KERROR("Invalid mode while trying to open file: '%s'.", path);
        return false;
    }

    FILE *file = fopen(path, mode_str);
    if (!file) {
        KERROR("Error opening file: '%s'.", path);
        return false;
    }

    out_handle->handle = file;
    out_handle->is_valid = true;
    return true;
}

void filesystem_close(file_handle *handle) {
    if (!handle->handle) {
        return;
    }

    fclose((FILE *)handle->handle);
    handle->handle = 0;
    handle->is_valid = false;
}

b8 filesystem_size(file_handle *handle, u64 *out_size) {
    if (!handle->handle) {
        return false;
    }
    fseek((FILE *)handle->handle, 0, SEEK_END);
    *out_size = ftell((FILE *)handle->handle);
    rewind((FILE *)handle->handle);
    return true;
}

KAPI b8 filesystem_read_line(file_handle *handle, u64 max_length,
                             char **line_buf, u64 *out_line_length) {
    if (!(handle->handle && line_buf && out_line_length && max_length > 0)) {
        return false;
    }

    char *buf = *line_buf;
    if (fgets(buf, max_length, (FILE *)handle->handle) == 0) {
        return false;
    }

    *out_line_length = strlen(*line_buf);
    return true;
}

b8 filesystem_write_line(file_handle *handle, const char *text) {
    if (!handle->handle) {
        return false;
    }

    i32 result = fputs(text, (FILE *)handle->handle);
    if (result != EOF) {
        result = fputc('\n', (FILE *)handle->handle);
    }

    fflush((FILE *)handle->handle);
    return result != EOF;
}

b8 filesystem_read(file_handle *handle, u64 data_size, void *out_data,
                   u64 *out_bytes_read) {
    if (!handle->handle) {
        return false;
    }

    *out_bytes_read = fread(out_data, 1, data_size, (FILE *)handle->handle);
    if (*out_bytes_read != data_size) {
        return false;
    }

    return true;
}

b8 filesystem_read_all_bytes(file_handle *handle, u8 *out_bytes,
                             u64 *out_bytes_read) {
    if (!handle->handle) {
        return false;
    }

    u64 size = 0;
    if (!filesystem_size(handle, &size)) {
        return false;
    }

    *out_bytes_read = fread(out_bytes, 1, size, (FILE *)handle->handle);
    return *out_bytes_read == size;
}

b8 filesystem_read_all_text(file_handle *handle, char *out_text,
                            u64 *out_bytes_read) {
    if (!handle->handle) {
        return false;
    }

    u64 size = 0;
    if (!filesystem_size(handle, &size)) {
        return false;
    }

    *out_bytes_read = fread(out_text, 1, size, (FILE *)handle->handle);
    return *out_bytes_read == size;
}

b8 filesystem_write(file_handle *handle, u64 data_size, const void *data,
                    u64 *out_bytes_written) {
    if (!handle->handle) {
        return false;
    }

    *out_bytes_written = fwrite(data, 1, data_size, (FILE *)handle->handle);
    if (*out_bytes_written != data_size) {
        return false;
    }

    fflush((FILE *)handle->handle);
    return true;
}
