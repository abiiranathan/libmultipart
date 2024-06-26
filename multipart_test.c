#include "multipart.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_unterminated_body();

int main() {
    // Read in form text with a multipart/form with username,password and an image.
    FILE* f = fopen("form.bin", "rb");
    assert(f);

    // Get the file size in bytes.
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Allocate a buffer on heap to holder file data.
    // This would be your request body.
    char* data = malloc(file_size);
    assert(data);

    // Make sure read was successful!
    int n = fread(data, 1, file_size, f);
    if (n < file_size) {
        fclose(f);
        fprintf(stderr, "Read %d instead of %ld\n", n, file_size);
        return EXIT_FAILURE;
    }

    // close the file.
    fclose(f);

    printf("Read %d bytes from from file\n", n);

    // Parse boundary from request body
    char boundary[128] = {0};
    if (!multipart_parse_boundary(data, boundary, sizeof(boundary))) {
        free(data);
        return EXIT_FAILURE;
    }
    printf("Boundary from body: %s\n", boundary);

    // Parse boundary from header
    const char* header = "multipart/form-data; boundary=----WebKitFormBoundaryS3sDR2atmc8KJS5U";
    char boundary2[128];
    if (!multipart_parse_boundary_from_header(header, boundary2, sizeof(boundary2))) {
        free(data);
        return EXIT_FAILURE;
    }
    printf("Boundary from head: %s\n", boundary2);

    // test that our boundary functions work as expected
    assert(strcmp(boundary, boundary2) == 0);

    // parse the form
    MultipartForm form = {0};
    MultipartCode code;
    code = multipart_parse_form(data, n, boundary, &form);
    if (code != MULTIPART_OK) {
        free(data);
        return EXIT_FAILURE;
    }

    // Let test our assumptions
    assert(form.num_fields == 2);
    assert(form.num_files == 1);

    const char* username = multipart_get_field_value(&form, "username");
    const char* password = multipart_get_field_value(&form, "password");
    assert(strcmp(username, "nabiizy") == 0);
    assert(strcmp(password, "password") == 0);

    // test files
    FileHeader* file;
    file = multipart_get_file(&form, "file");
    assert(file != NULL);

    assert(strcmp(file->filename, "Screenshot from 2024-06-07 23-13-39.png") == 0);
    assert(strcmp(file->mimetype, "image/png") == 0);
    assert(strcmp(file->field_name, "file") == 0);

    assert(file->size == 306279);
    assert(file->offset > 0);

    // Getting multiple files
    size_t num_files = 0;
    size_t* indices = multipart_get_files(&form, "file", &num_files);
    assert(indices);
    assert(num_files == 1);
    assert(indices[0] == 0);
    free(indices);

    // validate the count
    assert(num_files == 1);

    // Save the file
    bool saved = multipart_save_file(form.files[0], data, "form_upload_screenshot.png");
    assert(saved);
    printf("File saved\n");

    // Free the data
    free(data);

    // Free data allocated in the form.
    multipart_free_form(&form);

    assert(form.fields == NULL);
    assert(form.files == NULL);

    test_unterminated_body();
    printf("All tests passed\n");
    return EXIT_SUCCESS;
}

// test should still pass even if the body is not null terminated
void test_unterminated_body() {
    char boundary[128] = {0};
    const char body[] = {
        '-', '-', 'W', 'e', 'b', 'K', 'i', 't', 'F', 'o', 'r', 'm', 'B', 'o', 'u', 'n', 'd', 'a', 'r', 'y', 'S', '3',
        's', 'D', 'R', '2', 'a', 't', 'm', 'c', '8', 'K', 'J', 'S', '5', 'U', '\r', '\n', 'C', 'o', 'n', 't', 'e', 'n',
        't', '-', 'D', 'i', 's', 'p', 'o', 's', 'i', 't', 'i', 'o', 'n', ':', ' ', 'f', 'o', 'r', 'm', '-', 'd', 'a',
        't', 'a', ';', ' ', 'n', 'a', 'm', 'e', '=', '"', 'u', 's', 'e', 'r', 'n', 'a', 'm', 'e', '"', '\r', '\n', '\r',
        '\n', 'n', 'a', 'b', 'i', 'i', 'z', 'y', '\r', '\n', '-', '-', 'W', 'e', 'b', 'K', 'i', 't', 'F', 'o', 'r', 'm',
        'B', 'o', 'u', 'n', 'd', 'a', 'r', 'y', 'S', '3', 's', 'D', 'R', '2', 'a', 't', 'm', 'c', '8', 'K', 'J', 'S',
        '5', 'U', '\r', '\n',
        //  Add terminating boundary
        '-', '-', 'W', 'e', 'b', 'K', 'i', 't', 'F', 'o', 'r', 'm', 'B', 'o', 'u', 'n', 'd', 'a', 'r', 'y', 'S', '3',
        's', 'D', 'R', '2', 'a', 't', 'm', 'c', '8', 'K', 'J', 'S', '5', 'U', '-', '-', '\r', '\n'};

    assert(multipart_parse_boundary(body, boundary, sizeof(boundary)));

    assert(strncmp(boundary, "--WebKitFormBoundaryS3sDR2atmc8KJS5U", sizeof(boundary)) == 0);

    MultipartForm form = {0};
    MultipartCode code;
    code = multipart_parse_form(body, sizeof(body) - 1, boundary, &form);
    assert(code == MULTIPART_OK);

    assert(form.num_fields == 1);
    assert(form.num_files == 0);

    const char* username = multipart_get_field_value(&form, "username");
    assert(username);
    assert(strcmp(username, "nabiizy") == 0);

    printf("Test with non-null terminated body passed\n");
}
