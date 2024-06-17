#ifndef __MULTIPART_H__
#define __MULTIPART_H__

#include <stddef.h>
#include <stdbool.h>

// Constants that can be overriden
#ifndef INITIAL_FIELD_CAPACITY
#define INITIAL_FIELD_CAPACITY 16
#endif

#ifndef INITIAL_FILE_CAPACITY
#define INITIAL_FILE_CAPACITY 2
#endif

#ifndef MAX_FILE_SIZE
#define MAX_FILE_SIZE 10 * 1024 * 1024 // 10MB
#endif


typedef enum {
    STATE_BOUNDARY, 
    STATE_HEADER,
    STATE_KEY,
    STATE_VALUE,
    STATE_FILENAME,
    STATE_FILE_MIME_HEADER,
    STATE_MIMETYPE,
    STATE_FILE_BODY,
} State;


// FileHeader is a representation of a file parsed from the form.
// It helps us avoid copying file contents but can save the file from
// it's offset and size.
typedef struct FileHeader {
    size_t offset; // Offset from the body of request as passed to parse_multipart.
    size_t size;   // Computed file size.

    char* filename; // Value of filename in Content-Disposition
    char* mimetype; // Content-Type of the file.
    char* field_name; // Name of the field the file is associated with.
} FileHeader;


// Represents a field with its value in a form.
typedef struct FormField{
    char *name;  // Field name
    char *value; // Value associated with the field.
} FormField;

typedef struct MultipartForm {
    FileHeader* files;  // The array of file headers
    size_t num_files;   // The number of files processed.

    FormField*fields;  // Array of form field structs.
    size_t num_fields; // The number of fields.
} MultipartForm;

typedef  enum {
	MULTIPART_OK,
	MEMORY_ALLOC_ERROR,
	INVALID_FORM_BOUNDARY,
	MAX_FILE_SIZE_EXCEEDED,
	NO_FILE_CONTENT_TYPE,
	NO_FILE_CONTENT_DISPOSITION,
	NO_FILE_NAME,
	NO_FILE_DATA,     
}MultipartCode;

/**
 * Parse a multipart form from the request body.
 * @param data: Request body (with out headers)
 * @param size: Content-Length(size of data in bytes)
 * @param boundary: Null-terminated string for the form boundary.
 * @param form: Pointer to MultipartForm struct to store the parsed form data. It is assumed
 * to be initialized well and not NULL.
 * You can use the function multipart_parse_boundary or multipart_parse_boundary_from_header helpers
 * to get the boundary.
 * 
 * @returns: MultipartCode enum value indicating the success or failure of the operation.
 * Use the multipart_error_message function to get the error message if the code
 * is not MULTIPART_OK.
 * */
MultipartCode multipart_parse_form(const char* data, size_t size, char* boundary, MultipartForm *form);

// Free memory allocated by parse_multipart_form
void multipart_free_form(MultipartForm *form);

// Returns the const char* representing the error message.
const char* multipart_error_message(MultipartCode error);

// Parses the form boundary from the request body and copies it into the boundary buffer.
// size if the sizeof(boundary) buffer.
// Returns: true on success, false if the size is small or no boundary found.
bool multipart_parse_boundary(const char *body, char *boundary, size_t size);


// Parses the form boundary from the content-type header.
// Note that this boundary must always be -- shorter than what's in the body, so it's prefixed with -- for you.
// Returns true if successful, otherwise false(Invalid Content-Type, no boundary).
bool multipart_parse_boundary_from_header(const char *content_type, char *boundary, size_t size);

// =============== Fields API ========================
// Get the value of a field by name.
// Returns NULL if the field is not found.
const char* multipart_get_field_value(const MultipartForm *form, const char *name);

// =============== File API ==========================

// Get the first file matching the field name.
FileHeader* multipart_get_file(const MultipartForm *form, const char *field_name);

// Get all files matching the field name.
// @params: count is the number of files found and will be updated.
// Not that the array will be allocates and must be freed by the caller.
FileHeader* multipart_get_files(const MultipartForm *form, const char *field_name, size_t *count);


// Save file writes the file to the file system.
// @param: file is the FileHeader that has the correct offset and file size.
// @param:  body is the request body. (Must not have been modified) since the file offset is relative to it.
// @param: path is the path to save the file to.
//
// Returns: true on success, false on failure.
bool multipart_save_file(const FileHeader *file, const char *body, const char *path);

#endif // __MULTIPART_H__