// ========================================================================================
// Licence: MIT                                                                           #
// File: multipart.c                                                                      #
// This file implements a finite state machine for parsing multipart/form-data            #
// from http request body.                                                                #
// I implemented this as part of my project called epollix for a web server with epoll.   #
// Find it on github at https://github.com/abiiranathan/epollix.git                       #
//                                                                                        #
// Author: Dr. Abiira Nathan                                                              #
// Date: 17 June 2024                                                                     #
//=========================================================================================

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "multipart.h"

// Helper function to double the capacity of files allocated in the form.
static FileHeader* realloc_files(MultipartForm* form);

// Helper function to double the capacity of fields allocated in the form.
static FormField* realloc_fields(MultipartForm* form);

/**
 * Parse a multipart form from the request body.
 * @param data: Request body (with out headers)
 * @param size: Content-Length(size of data in bytes)
 * @param boundary: Null-terminated string for the form boundary.
 * @param form: Pointer to MultipartForm struct to store the parsed form data. It is assumed
 * to be initialized well and not NULL.
 * You can use the function parse_multipart_boundary or parse_multipart_boundary_from_header helpers
 * to get the boundary.
 * 
 * @returns: MultipartCode enum value indicating the success or failure of the operation.
 * Use the multipart_error_message function to get the error message if the code
 * is not MULTIPART_OK.
 * */
MultipartCode multipart_parse_form(const char* data, size_t size, char* boundary, MultipartForm* form) {
    size_t boundary_length = strlen(boundary);

    // Temporary variables to store state of the FSM.
    const char* ptr = data;

    const char* key_start = NULL;
    const char* value_start = NULL;
    char* key = NULL;
    char* value = NULL;

    char* filename = NULL;
    char* mimetype = NULL;

    // Current file in State transitions
    FileHeader header = {0};

    // Initial state
    State state = STATE_BOUNDARY;

    // Allocate initial memory for files and fields.
    form->files = (FileHeader*)malloc(INITIAL_FILE_CAPACITY * sizeof(FileHeader));
    if (!form->files) {
        fprintf(stderr, "Failed to allocate memory for files\n");
        return MEMORY_ALLOC_ERROR;
    }

    // Initialize the number of files and fields to 0.
    form->num_files = 0;

    // zero out the memory
    memset(form->files, 0, INITIAL_FILE_CAPACITY * sizeof(FileHeader));

    // Allocate memory for fields
    form->fields = (FormField*)malloc(INITIAL_FIELD_CAPACITY * sizeof(FormField));
    if (!form->fields) {
        fprintf(stderr, "Failed to allocate memory for fields\n");
        return MEMORY_ALLOC_ERROR;
    }
    form->num_fields = 0;
    memset(form->fields, 0, INITIAL_FIELD_CAPACITY * sizeof(FormField));

    // Start parsing the form data
    while (*ptr != '\0') {
        switch (state) {
            case STATE_BOUNDARY:
                if (strncmp(ptr, boundary, boundary_length) == 0) {
                    state = STATE_HEADER;
                    ptr += boundary_length;
                    while (*ptr == '-' || *ptr == '\r' || *ptr == '\n')
                        ptr++;  // Skip extra characters after boundary
                } else {
                    ptr++;
                }
                break;
            case STATE_HEADER:
                if (strncmp(ptr, "Content-Disposition:", 20) == 0) {
                    ptr = strstr(ptr, "name=\"") + 6;
                    key_start = ptr;
                    state = STATE_KEY;
                } else {
                    ptr++;
                }
                break;
            case STATE_KEY:
                if (*ptr == '"' && key_start != NULL) {
                    size_t key_length = ptr - key_start;
                    key = (char*)malloc(key_length + 1);
                    if (!key) {
                        fprintf(stderr, "Failed to allocate memory for key\n");
                        multipart_free_form(form);
                        return MEMORY_ALLOC_ERROR;
                    }

                    strncpy(key, key_start, key_length);
                    key[key_length] = '\0';

                    // Check if we have a filename="name" next in case its a file.
                    if (strncmp(ptr, "\"; filename=\"", 13) == 0) {
                        // Store the field name in header
                        header.field_name = key;

                        // Switch state to process filename
                        ptr = strstr(ptr, "; filename=\"") + 12;
                        key_start = ptr;
                        state = STATE_FILENAME;
                    } else {
                        // Move to the end of the line
                        while (*ptr != '\n')
                            ptr++;

                        ptr++;  // Skip the newline character

                        // consume the leading CRLF before value
                        if (*ptr == '\r' && *(ptr + 1) == '\n')
                            ptr += 2;

                        value_start = ptr;
                        state = STATE_VALUE;
                    }
                } else {
                    ptr++;
                }
                break;
            case STATE_VALUE:
                if ((strncmp(ptr, "\r\n--", 4) == 0 || strncmp(ptr, boundary, boundary_length) == 0) &&
                    value_start != NULL) {
                    size_t value_length = ptr - value_start;
                    value = (char*)malloc(value_length + 1);
                    if (!value) {
                        fprintf(stderr, "Failed to allocate memory for value\n");

                        // Free previously allocated key(we check just in case)
                        if (key)
                            free(key);
                        multipart_free_form(form);
                        return MEMORY_ALLOC_ERROR;
                    }

                    strncpy(value, value_start, value_length);
                    value[value_length] = '\0';

                    // Save the key-value pair
                    char* key_copy = strdup(key);
                    char* value_copy = strdup(value);
                    if (!key_copy || !value_copy) {
                        fprintf(stderr, "Failed to copy key or value\n");
                        if (key)
                            free(key);
                        if (value)
                            free(value);
                        multipart_free_form(form);
                        return MEMORY_ALLOC_ERROR;
                    }

                    // Check if we have enough capacity for fields
                    if (form->num_fields >= INITIAL_FIELD_CAPACITY) {
                        if (!realloc_fields(form)) {
                            fprintf(stderr, "Failed to reallocate fields\n");
                            free(key);
                            free(value);
                            multipart_free_form(form);
                            return MEMORY_ALLOC_ERROR;
                        }
                    }

                    // Copy the key-value pair
                    form->fields[form->num_fields].name = key_copy;
                    form->fields[form->num_fields].value = value_copy;
                    form->num_fields++;

                    // Free the key and value
                    free(key);
                    free(value);

                    // Reset key and value pointers
                    key = NULL;
                    value = NULL;

                    while (*ptr == '\r' || *ptr == '\n')
                        ptr++;  // Skip CRLF characters

                    // Reset state and process the next field if any
                    state = STATE_BOUNDARY;
                } else {
                    ptr++;
                }
                break;
            case STATE_FILENAME: {
                if (*ptr == '"' && key_start != NULL) {
                    size_t filename_length = ptr - key_start;
                    filename = (char*)malloc(filename_length + 1);
                    if (!filename) {
                        fprintf(stderr, "Failed to allocate memory for filename\n");
                        // Free previously allocated key(field name)
                        // We expect it to be allocated but we check anyway!
                        if (key)
                            free(key);
                        multipart_free_form(form);
                        return MEMORY_ALLOC_ERROR;
                    }

                    strncpy(filename, key_start, filename_length);
                    filename[filename_length] = '\0';

                    header.filename = strdup(filename);
                    if (!header.filename) {
                        free(filename);
                        if (key)
                            free(key);
                        multipart_free_form(form);
                        return MEMORY_ALLOC_ERROR;
                    }

                    // Move to the end of the line
                    while (*ptr != '\n')
                        ptr++;

                    ptr++;  // Skip the newline character

                    // consume the leading CRLF before value if available
                    // avoid dereferencing null pointer if ptr+1 is null
                    if (*ptr == '\r' && *(ptr + 1) == '\n')
                        ptr += 2;

                    // We expect the next line to be Content-Type
                    state = STATE_FILE_MIME_HEADER;
                } else {
                    ptr++;
                }
            } break;
            case STATE_FILE_MIME_HEADER: {
                if (strncmp(ptr, "Content-Type: ", 14) == 0) {
                    ptr = strstr(ptr, "Content-Type: ") + 14;
                    state = STATE_MIMETYPE;
                } else {
                    ptr++;
                }
            } break;
            case STATE_MIMETYPE: {
                size_t mimetype_len = 0;  // Length of the mimetype
                value_start = ptr;        // store the start of the mimetype

                // Compute the length of the mimetype as we advance the pointer
                while (*ptr != '\r' && *ptr != '\n') {
                    mimetype_len++;
                    ptr++;
                }

                // Allocate memory for the mimetype, if len is 0, we shall have an empty string
                mimetype = (char*)malloc(mimetype_len + 1);
                if (!mimetype) {
                    fprintf(stderr, "Failed to allocate memory for mimetype\n");
                    if (key)
                        free(key);
                    if (filename)
                        free(filename);
                    multipart_free_form(form);
                    return MEMORY_ALLOC_ERROR;
                }

                // Copy the mimetype into the allocated memory
                strncpy(mimetype, value_start, mimetype_len);
                mimetype[mimetype_len] = '\0';
                header.mimetype = mimetype;

                // Move to the end of the line
                while (*ptr != '\n')
                    ptr++;

                ptr++;  // Skip the newline character

                // consume the leading CRLF before bytes of the file
                while (((*ptr == '\r' && *(ptr + 1) == '\n'))) {
                    ptr += 2;
                }

                // We expect the body of the file next, but we check for boundary
                // Just in case the file is empty.
                if (memcmp(ptr, boundary, boundary_length) == 0) {
                    // No file content, free everyting
                    if (key)
                        free(key);
                    if (filename)
                        free(filename);
                    if (mimetype)
                        free(mimetype);

                    // We don't report this as an error, we just ignore the file.
                    state = STATE_BOUNDARY;
                } else {
                    // We have a file body
                    state = STATE_FILE_BODY;
                }
            } break;
            case STATE_FILE_BODY:
                header.offset = ptr - data;
                size_t endpos = 0;
                size_t haystack_len = size - header.offset;

                // Apparently strstr can't be used with binary data!!
                // I spen't days here trying to figgit with binary files :)
                char* endptr = memmem(ptr, haystack_len, boundary, boundary_length);
                if (endptr == NULL) {
                    if (key)
                        free(key);
                    if (filename)
                        free(filename);
                    if (mimetype)
                        free(mimetype);
                    multipart_free_form(form);
                    // We have a problem with boundary
                    fprintf(stderr, "Unterminated boundary after file body\n");
                    return INVALID_FORM_BOUNDARY;
                }

                // Compute the end of file contents so we determine file size.
                endpos = endptr - data;

                // Let's validate the file size to avoid overflow and wrapping
                // if endpos is less than offset, we have a problem
                if (endpos < header.offset) {
                    if (key)
                        free(key);
                    if (filename)
                        free(filename);
                    if (mimetype)
                        free(mimetype);
                    multipart_free_form(form);
                    fprintf(stderr, "Unexpected file size. failed assertion on file size\n");
                    return INVALID_FORM_BOUNDARY;
                }

                // Compute the file size
                size_t file_size = endpos - header.offset;

                // Validate the file size
                if (file_size > MAX_FILE_SIZE) {
                    if (key)
                        free(key);
                    if (filename)
                        free(filename);
                    if (mimetype)
                        free(mimetype);

                    multipart_free_form(form);
                    fprintf(stderr, "File size exceeds maximum file size\n");
                    return MAX_FILE_SIZE_EXCEEDED;
                }

                // Set the file size.
                header.size = file_size;

                // set the label
                header.field_name = strdup(key);
                if (!header.field_name) {
                    if (key)
                        free(key);
                    if (filename)
                        free(filename);
                    if (mimetype)
                        free(mimetype);
                    multipart_free_form(form);
                    return MEMORY_ALLOC_ERROR;
                }

                // Check if we have enough capacity for files
                if (form->num_files >= INITIAL_FILE_CAPACITY) {
                    if (!realloc_files(form)) {
                        fprintf(stderr, "Failed to reallocate files\n");
                        free(key);
                        free(filename);
                        free(mimetype);
                        free(header.field_name);
                        multipart_free_form(form);
                        return MEMORY_ALLOC_ERROR;
                    }
                }

                // Copy the file header into the files array
                form->files[form->num_files] = header;
                form->num_files++;

                header = (FileHeader){0};

                // consume the trailing CRLF before the next boundary
                while (((*ptr == '\r' && *(ptr + 1) == '\n'))) {
                    ptr += 2;
                }
                state = STATE_BOUNDARY;
                break;
            default:
                // This is unreachable but just in case, we don't want an infinite-loop
                // Crash and burn!!
                fprintf(stderr, "UNREACHABLE\n");
                exit(EXIT_FAILURE);
        }
    }

    if (key)
        free(key);
    if (value)
        free(value);
    if (filename)
        free(filename);

    return MULTIPART_OK;
}

// Parses the form boundary from the request body and copies it into the boundary buffer.
// size if the sizeof(boundary) buffer.
// Returns: true on success, false if the size is small or no boundary found.
bool multipart_parse_boundary(const char* body, char* boundary, size_t size) {
    char* boundary_end = strstr(body, "\r\n");  // should end at first CRLF
    if (!boundary_end) {
        fprintf(stderr, "Unable to determine the boundary in body: %s\n", body);
        return false;
    }

    size_t length = boundary_end - body;
    size_t total_capacity = length + 1;
    if (size <= total_capacity) {
        fprintf(stderr, "boundary buffer is smaller than %ld bytes\n", length + 1);
        return false;
    }

    strncpy(boundary, body, length);
    boundary[length] = '\0';
    return true;
}

// Parses the form boundary from the content-type header.
// Note that this boundary must always be -- shorter than what's in the body, so it's prefixed for you.
// Returns true if successful, otherwise false(Invalid Content-Type, no boundary).
bool multipart_parse_boundary_from_header(const char* content_type, char* boundary, size_t size) {
    const char* prefix = "--";
    size_t prefix_len = strlen(prefix);
    size_t total_length = strlen(content_type);

    if (strncasecmp(content_type, "multipart/form-data", 19) != 0) {
        fprintf(stderr, "content type is missing multipart/form-data in header\n");
        return false;
    }

    char* start = strstr(content_type, "boundary=");
    size_t length = total_length - ((start + 9) - content_type);

    // Account for prefix and null terminater
    if (size <= length + prefix_len + 1) {
        fprintf(stderr, "buffer size for boundary is too small\n");
        return false;
    }

    memcpy(boundary, prefix, prefix_len);  // ignore null terminator
    strncpy(boundary + prefix_len, (start + 9), length);
    boundary[length + prefix_len] = '\0';
    return true;
}

void multipart_free_form(MultipartForm* form) {
    if (!form)
        return;

    for (size_t i = 0; i < form->num_files; i++) {
        if (form->files[i].filename) {
            free(form->files[i].filename);
            form->files[i].filename = NULL;
        }

        if (form->files[i].mimetype) {
            free(form->files[i].mimetype);
            form->files[i].mimetype = NULL;
        }

        if (form->files[i].field_name) {
            free(form->files[i].field_name);
            form->files[i].field_name = NULL;
        }
    }

    for (size_t i = 0; i < form->num_fields; i++) {
        if (form->fields[i].name) {
            free(form->fields[i].name);
            form->fields[i].name = NULL;
        }
        if (form->fields[i].value) {
            free(form->fields[i].value);
            form->fields[i].value = NULL;
        }
    }
    if (form->files) {
        free(form->files);
        form->files = NULL;
    }
    if (form->fields) {
        free(form->fields);
        form->fields = NULL;
    }

    form->num_files = 0;
    form->num_fields = 0;
    form = NULL;
}

// =============== Fields API ========================
// Get the value of a field by name.
// Returns NULL if the field is not found.
const char* multipart_get_field_value(const MultipartForm* form, const char* name) {
    for (size_t i = 0; i < form->num_fields; i++) {
        if (strcmp(form->fields[i].name, name) == 0) {
            return form->fields[i].value;
        }
    }
    return NULL;
}

// =============== File API ==========================

// Get the first file matching the field name.
FileHeader* multipart_get_file(const MultipartForm* form, const char* field_name) {
    for (size_t i = 0; i < form->num_files; i++) {
        if (strcmp(form->files[i].field_name, field_name) == 0) {
            return &form->files[i];
        }
    }
    return NULL;
}

// Get all files matching the field name.
// @params: count is the number of files found and will be updated.
// Not that the array will be allocates and must be freed by the caller with
// the normal `free` call on the array.
FileHeader* multipart_get_files(const MultipartForm* form, const char* field_name, size_t* count) {
    size_t num_files = 0;
    for (size_t i = 0; i < form->num_files; i++) {
        if (strcmp(form->files[i].field_name, field_name) == 0) {
            num_files++;
        }
    }

    if (num_files == 0 || count == NULL) {
        *count = 0;
        return NULL;
    }

    FileHeader* files = (FileHeader*)malloc(num_files * sizeof(FileHeader));
    if (!files) {
        perror("Failed to allocate memory for files");
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < form->num_files; i++) {
        if (strcmp(form->files[i].field_name, field_name) == 0) {
            files[j] = form->files[i];
            j++;
        }
    }

    *count = num_files;
    return files;
}

// Save file writes the file to the file system.
// @param: file is the FileHeader that has the correct offset and file size.
// @param:  body is the request body. (Must not have been modified) since the file offset is relative to it.
// @param: path is the path to save the file to.
//
// Returns: true on success, false on failure.
bool multipart_save_file(const FileHeader* file, const char* body, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        perror("Failed to open file for writing");
        return false;
    }

    size_t n = fwrite(body + file->offset, 1, file->size, f);
    if (n != file->size) {
        perror("Failed to write file to disk");
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

// Returns the const char* representing the error message.
const char* multipart_error_message(MultipartCode error) {
    switch (error) {
        case MEMORY_ALLOC_ERROR:
            return "Memory allocation failed";
        case INVALID_FORM_BOUNDARY:
            return "Invalid form boundary";
        case MAX_FILE_SIZE_EXCEEDED:
            return "Maximum file size exceeded";
        case NO_FILE_CONTENT_TYPE:
            return "No file content type";
        case NO_FILE_CONTENT_DISPOSITION:
            return "No file content disposition";
        case NO_FILE_NAME:
            return "No file name";
        case NO_FILE_DATA:
            return "No file data";
        default:
            return "Multipart OK";
    }
}

static FileHeader* realloc_files(MultipartForm* form) {
    size_t new_capacity = form->num_files * 2;
    FileHeader* new_files = (FileHeader*)realloc(form->files, new_capacity * sizeof(FileHeader));
    if (!new_files) {
        perror("Failed to reallocate memory for files");
        multipart_free_form(form);
        return NULL;
    }
    form->files = new_files;
    return form->files;
}

static FormField* realloc_fields(MultipartForm* form) {
    size_t new_capacity = form->num_fields * 2;
    FormField* new_fields = (FormField*)realloc(form->fields, new_capacity * sizeof(FormField));
    if (!new_fields) {
        perror("Failed to reallocate memory for fields");
        multipart_free_form(form);
        return NULL;
    }
    form->fields = new_fields;
    return form->fields;
}

// Works but still has memory leaks and needs tidying.