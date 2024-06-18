## libmultipart


<div style="display:flex; align-items:center; column-gap: 4px;">

![Version 1.0.0](https://img.shields.io/badge/version-1.0.0-blue.svg)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[![C](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
</div>


<img width="100" height="auto" src="./logo.jpeg" alt="libmultipart">

libmultipart is a lightweight C library for parsing ***multipart/form-data*** requests. It's designed to be simple and efficient, offering the following features:

- **Parses multipart/form-data requests:**  Handles both file uploads and regular form data.
- **Efficient memory management:** Avoids unnecessary memory copying by storing file data by offset and size.
- **Error handling:** Provides informative error codes and messages for debugging.
- **Customizable maximum sizes:**  Allows adjusting maximum sizes for file uploads, field names, filenames, mimetypes, and values.

### Installation

1. **Download the library:** Get the source code from [Github](http://github.com/abiiranathan/libmultipart.git).
2. **Compile the library:**
   ```bash
   gcc -c multipart.c -o multipart.o
   ar rcs libmultipart.a multipart.o
   ```
3. **Include the library:** 
   In your project, add the following include line to your header files:
   ```c
   #include "multipart.h"
   ```
4. **Link the library:**
   When linking your project, add the library to the linker command:
   ```bash
   gcc your_program.c -L. -lmultipart -o your_program
   ```

### Usage Example

```c
#include <stdio.h>
#include "multipart.h"

int main() {
    // Simulate a multipart/form-data request body
    const char *data = "--boundary\r\n"
                     "Content-Disposition: form-data; name=\"text_field\"\r\n\r\n"
                     "Hello, world!\r\n"
                     "--boundary\r\n"
                     "Content-Disposition: form-data; name=\"file_field\"; filename=\"example.txt\"\r\n"
                     "Content-Type: text/plain\r\n\r\n"
                     "This is the content of the file.\r\n"
                     "--boundary--\r\n";

    // Parse the boundary string
    char boundary[256] = {0};
    multipart_parse_boundary(data, boundary, sizeof(boundary));

    // Create a MultipartForm structure
    MultipartForm form = {0};

    // Parse the multipart data
    MultipartCode result = multipart_parse_form(data, strlen(data), boundary, &form);

    if (result == MULTIPART_OK) {
        // Print form fields
        for (size_t i = 0; i < form.num_fields; i++) {
            printf("Field: %s, Value: %s\n", form.fields[i].name, form.fields[i].value);
        }

        // Print file information
        for (size_t i = 0; i < form.num_files; i++) {
            printf("File: %s, Mimetype: %s, Size: %zu\n", form.files[i]->filename,
                   form.files[i]->mimetype, form.files[i]->size);

            // Save the file to disk (example)
            char filename[256] = {0};
            sprintf(filename, "uploads/%s", form.files[i]->filename);
            if (multipart_save_file(form.files[i], data, filename)) {
                printf("File saved to %s\n", filename);
            } else {
                printf("Failed to save file!\n");
            }
        }

        // Free memory
        multipart_free_form(&form);
    } else {
        printf("Error parsing form: %s\n", multipart_error_message(result));
    }

    return 0;
}
```

### API Reference

The library provides the following functions:

- **`multipart_parse_form(const char* data, size_t size, char* boundary, MultipartForm* form)`**: Parses a multipart form from the request body.
- **`multipart_free_form(MultipartForm* form)`**: Frees memory allocated by `multipart_parse_form`.
- **`multipart_error_message(MultipartCode error)`**: Returns a string describing the given error code.
- **`multipart_parse_boundary(const char* body, char* boundary, size_t size)`**: Parses the form boundary from the request body.
- **`multipart_parse_boundary_from_header(const char* content_type, char* boundary, size_t size)`**: Parses the form boundary from the Content-Type header.
- **`multipart_get_field_value(const MultipartForm* form, const char* name)`**: Retrieves the value of a field by name.
- **`multipart_get_file(const MultipartForm* form, const char* field_name)`**: Retrieves the first file associated with a field name.
- **`multipart_get_files(const MultipartForm* form, const char* field_name, size_t* count)`**: Retrieves indices of all files associated with a field name.
- **`multipart_save_file(const FileHeader* file, const char* body, const char* path)`**: Saves a file to the file system.

### Run the tests
```bash
make test
```

### License

MIT