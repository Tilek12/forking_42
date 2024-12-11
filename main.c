#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

typedef char i8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int i32;
typedef unsigned u32;
typedef unsigned long u64;

#define PRINT_ERROR(cstring) write(STDERR_FILENO, cstring, sizeof(cstring) - 1)

#pragma pack(1)
struct bmp_header
{
	// Note: header
	i8  signature[2]; // should equal to "BM"
	u32 file_size;
	u32 unused_0;
	u32 data_offset;

	// Note: info header
	u32 info_header_size;
	u32 width; // in px
	u32 height; // in px
	u16 number_of_planes; // should be 1
	u16 bit_per_pixel; // 1, 4, 8, 16, 24 or 32
	u32 compression_type; // should be 0
	u32 compressed_image_size; // should be 0
	// Note: there are more stuff there but it is not important here
};

struct file_content
{
	i8*   data;
	u32   size;
};

struct file_content   read_entire_file(char* filename)
{
	char* file_data = 0;
	unsigned long	file_size = 0;
	int input_file_fd = open(filename, O_RDONLY);
	if (input_file_fd >= 0)
	{
		struct stat input_file_stat = {0};
		stat(filename, &input_file_stat);
		file_size = input_file_stat.st_size;
		file_data = mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, input_file_fd, 0);
		close(input_file_fd);
	}
	return (struct file_content){file_data, file_size};
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		PRINT_ERROR("Usage: decode <input_filename>\n");
		return 1;
	}
	struct file_content file_content = read_entire_file(argv[1]);
	if (file_content.data == NULL)
	{
		PRINT_ERROR("Failed to read file\n");
		return 1;
	}
	struct bmp_header* header = (struct bmp_header*) file_content.data;

	// printf("\n=======================================\n\n");

	// printf("signature: %.2s\nfile_size: %u\ndata_offset: %u\ninfo_header_size: %u\nwidth: %u\nheight: %u\nplanes: %i\nbit_per_px: %i\ncompression_type: %u\ncompression_size: %u\n", header->signature, header->file_size, header->data_offset, header->info_header_size, header->width, header->height, header->number_of_planes, header->bit_per_pixel, header->compression_type, header->compressed_image_size);

	// printf("\n=======================================\n\n");

    u8* pixel_data = (u8*)(file_content.data + header->data_offset);
    u32 row_size = ((header->width * header->bit_per_pixel + 31) / 32) * 4;

    // Find the first occurrence of the target color (127, 188, 217) in BGR format
    int found_x = -1, found_y = -1;
    for (u32 y = 0; y < header->height; y++) {
        for (u32 x = 0; x < header->width; x++) {
            u32 pixel_offset = (header->height - 1 - y) * row_size + x * 4;
            u8 b = pixel_data[pixel_offset];
            u8 g = pixel_data[pixel_offset + 1];
            u8 r = pixel_data[pixel_offset + 2];

            if (b == 127 && g == 188 && r == 217) {
                // Check for 7 consecutive pixels of the same color
                int match = 1;
                for (int k = 1; k < 7 && x + k < header->width; k++) {
                    u32 next_pixel_offset = (header->height - 1 - y) * row_size + (x + k) * 4;
                    u8 next_b = pixel_data[next_pixel_offset];
                    u8 next_g = pixel_data[next_pixel_offset + 1];
                    u8 next_r = pixel_data[next_pixel_offset + 2];

                    if (!(next_b == 127 && next_g == 188 && next_r == 217)) {
                        match = 0;
                        break;
                    }
                }

                if (match) {
                    found_x = x;
                    found_y = y;
                    break;
                }
            }
        }
        if (found_x != -1 && found_y != -1) break;
    }

    if (found_x == -1 || found_y == -1) {
        PRINT_ERROR("Target color sequence not found\n");
        return 1;
    }

    // Determine the length of the hidden message
    u32 eighth_pixel_offset = (header->height - 1 - found_y) * row_size + (found_x + 7) * 4;
    u8 blue_8th = pixel_data[eighth_pixel_offset];
    u8 red_8th = pixel_data[eighth_pixel_offset + 2];
    int message_length = blue_8th + red_8th;

    // Allocate buffer for the hidden message
    char* message = (char*)malloc(message_length + 1);
    if (!message) {
        PRINT_ERROR("Memory allocation failed\n");
        return 1;
    }
    message[message_length] = '\0';

    // Read the hidden message
    int char_index = 0;
    for (u32 row = found_y + 2; row < header->height && char_index < message_length; row++) {
        for (u32 col = found_x + 2; col < header->width && char_index < message_length; col++) {
            u32 pixel_offset = (header->height - 1 - row) * row_size + col * 4;

            u8 b = pixel_data[pixel_offset];
            u8 g = pixel_data[pixel_offset + 1];
            u8 r = pixel_data[pixel_offset + 2];

            message[char_index++] = (char)b;
            if (char_index >= message_length) break;

            message[char_index++] = (char)g;
            if (char_index >= message_length) break;

            message[char_index++] = (char)r;

            if (char_index >= message_length) break;
        }
    }

    // Print the hidden message
    // printf("Hidden Message: %s\n", message);
	printf("%s", message);

    // Clean up
    free(message);
    munmap(file_content.data, file_content.size);

	// printf("\n=======================================\n\n");

	return 0;
}
