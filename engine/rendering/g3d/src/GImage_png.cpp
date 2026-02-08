/**
 @file GImage_png.cpp
 @author Morgan McGuire, [http://graphics.cs.williams.edu](http://graphics.cs.williams.edu)
 @created 2002-05-27

 updated by nicolas industries
 @edited 2025-07-11
*/
#include "g3d/platform.h"
#include "g3d/GImage.h"
#include <libpng16/png.h>
#include <libpng16/pngconf.h>
#include <sstream>
#include "g3d/BinaryOutput.h"
#include "g3d/BinaryInput.h" // Assuming BinaryInput is defined here or included elsewhere
#include <setjmp.h> // Required for setjmp/longjmp

namespace G3D {

// A thread-local variable to store the last error message from libpng callbacks.
// This is used because png_ptr->error_ptr is not a standard way to pass messages
// in modern libpng, and we need to retrieve the message after longjmp.
thread_local const char* g_png_last_error_message = nullptr;

//libpng required function signature for writing data
static void png_write_data(png_structp png_ptr,
    png_bytep data,
    png_size_t length) {

    // Use png_get_io_ptr to safely access the user-defined I/O pointer
    BinaryOutput* bo = static_cast<BinaryOutput*>(png_get_io_ptr(png_ptr));
    debugAssert( bo != NULL );
    debugAssert( data != NULL );

    bo->writeBytes(data, length);
}

//libpng required function signature for flushing data
static void png_flush_data(
    png_structp png_ptr) {
    (void)png_ptr; // Parameter not used in this implementation
    // Do nothing. Flushing is typically handled by the underlying stream if needed.
}

//libpng required function signature for error handling
static void png_error(
    png_structp png_ptr,
    png_const_charp error_msg) {

    // Store the error message in the thread-local variable for later retrieval
    g_png_last_error_message = error_msg;
    // Perform a long jump to the point where setjmp was called, indicating an error
    longjmp(png_jmpbuf(png_ptr), 1);
}

//libpng required function signature for warning handling
void png_warning(
    png_structp png_ptr,
    png_const_charp warning_msg)
{
    // Warnings are typically ignored or logged. For this drop-in replacement,
    // we maintain the original behavior of doing nothing.
    (void)png_ptr;
    (void)warning_msg;
}

//libpng required function signature for reading data
static void png_read_data(
    png_structp png_ptr,
    png_bytep data,
    png_size_t length) {

    // Use png_get_io_ptr to safely access the user-defined I/O pointer
    BinaryInput* bi = static_cast<BinaryInput*>(png_get_io_ptr(png_ptr));
    debugAssert( bi != NULL );
    debugAssert( length >= 0 );
    debugAssert( data != NULL );

    int64 pos = bi->getPosition();
    int64 binaryInputLen = bi->getLength();

    // Check if reading past the end of the input stream
    if (pos + length > binaryInputLen)
    {
        // Adjust length to read only available bytes, and signal an error
        length = binaryInputLen - pos;
        G3D::png_error(png_ptr, "Trying to load incomplete image");
    }

    bi->readBytes(data, length);
}

void GImage::encodePNG(
    BinaryOutput&           out) const {

    if (! (m_channels == 1 || m_channels == 2 || m_channels == 3 || m_channels == 4)) {
        throw GImage::Error(format("Illegal channels for PNG: %d", m_channels));
    }
    if (m_width <= 0) {
        throw GImage::Error(format("Illegal width for PNG: %d", m_width));
    }
    if (m_height <= 0) {
        throw GImage::Error(format("Illegal height for PNG: %d", m_height));
    }

    // PNG library requires that the height * pointer size fit within an int
    // This check ensures that the array of row pointers can be allocated correctly.
    if (png_uint_32(m_height) * sizeof(png_bytep) > PNG_UINT_32_MAX) {
        throw GImage::Error("Unsupported PNG height.");
    }

    out.setEndian(G3D_LITTLE_ENDIAN);

    // Create the PNG write struct. The last three arguments are for user_error_ptr,
    // error_fn, and warning_fn respectively. We pass NULL for user_error_ptr as
    // we're using a thread_local variable for error messages.
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, png_error, png_warning);
    if (! png_ptr) {
        throw GImage::Error("Unable to initialize PNG encoder." );
    }

    // Set up the long jump buffer for error handling. If an error occurs in libpng,
    // png_error will call longjmp, returning control here with a non-zero value.
    if (setjmp(png_jmpbuf(png_ptr))) {
        // Retrieve the error message stored by png_error
        const char* message = g_png_last_error_message ? g_png_last_error_message : "Unknown PNG error";
        g_png_last_error_message = nullptr; // Clear the message for the next operation

        // Clean up libpng structures before throwing the exception
        png_infop info_ptr = png_get_io_ptr(png_ptr) ? static_cast<png_infop>(png_get_io_ptr(png_ptr)) : NULL; // Attempt to get info_ptr if available
        png_destroy_write_struct(&png_ptr, &info_ptr); // info_ptr might be NULL if error occurred early
        throw GImage::Error(message);
    }

    // Create the PNG info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (! info_ptr) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        throw GImage::Error("Unable to initialize PNG encoder.");
    }

    // Set up the custom write function to use BinaryOutput
    png_set_write_fn(png_ptr, (void*)&out, png_write_data, png_flush_data);

    png_color_8_struct sig_bit; // Structure for significant bits

    // Set image header based on the number of channels
    switch (m_channels) {
    case 1: // Grayscale
        png_set_IHDR(png_ptr, info_ptr, m_width, m_height, 8, PNG_COLOR_TYPE_GRAY,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        sig_bit.red = 0;
        sig_bit.green = 0;
        sig_bit.blue = 0;
        sig_bit.alpha = 0;
        sig_bit.gray = 8;
        break;

    case 2: // Grayscale with Alpha
        png_set_IHDR(png_ptr, info_ptr, m_width, m_height, 8, PNG_COLOR_TYPE_GRAY_ALPHA,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        sig_bit.red = 0;
        sig_bit.green = 0;
        sig_bit.blue = 0;
        sig_bit.alpha = 8;
        sig_bit.gray = 8;
        break;

    case 3: // RGB
        png_set_IHDR(png_ptr, info_ptr, m_width, m_height, 8, PNG_COLOR_TYPE_RGB,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        sig_bit.red = 8;
        sig_bit.green = 8;
        sig_bit.blue = 8;
        sig_bit.alpha = 0;
        sig_bit.gray = 0;
        break;

    case 4: // RGBA
        png_set_IHDR(png_ptr, info_ptr, m_width, m_height, 8, PNG_COLOR_TYPE_RGBA,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        sig_bit.red = 8;
        sig_bit.green = 8;
        sig_bit.blue = 8;
        sig_bit.alpha = 8;
        sig_bit.gray = 0;
        break;

    default:
        png_destroy_write_struct(&png_ptr, &info_ptr);
        throw GImage::Error("Unsupported number of channels for PNG.");
    }

    // Set the significant bits information
    png_set_sBIT(png_ptr, info_ptr, &sig_bit);

    // Write the PNG header and information
    png_write_info(png_ptr, info_ptr);

    // Create an array of row pointers to the image data
    png_bytepp row_pointers = new png_bytep[m_height];
    for (int i=0; i < m_height; ++i) {
        row_pointers[i] = (png_bytep)&m_byte[m_width * m_channels * i];
    }

    // Write the image data
    png_write_image(png_ptr, row_pointers);

    // Finish writing the PNG file
    png_write_end(png_ptr, info_ptr);

    // Clean up allocated memory
    delete[] row_pointers;

    // Destroy the libpng write structures
    png_destroy_write_struct(&png_ptr, &info_ptr);
}


void GImage::decodePNG(
    BinaryInput&            input) {

    // Create the PNG read struct
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_error, png_warning);
    if (png_ptr == NULL) {
        throw GImage::Error("Unable to initialize PNG decoder.");
    }

    // Set up the long jump buffer for error handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        // Retrieve the error message stored by png_error
        const char* message = g_png_last_error_message ? g_png_last_error_message : "Unknown PNG error";
        g_png_last_error_message = nullptr; // Clear the message for the next operation

        // Clean up libpng structures before throwing the exception
        png_infop info_ptr = png_get_io_ptr(png_ptr) ? static_cast<png_infop>(png_get_io_ptr(png_ptr)) : NULL; // Attempt to get info_ptr if available
        png_infop end_info = png_get_io_ptr(png_ptr) ? static_cast<png_infop>(png_get_io_ptr(png_ptr)) : NULL; // Attempt to get end_info if available
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info); // info_ptr/end_info might be NULL
        throw GImage::Error(message);
    }

    // Create the PNG info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        throw GImage::Error("Unable to initialize PNG decoder.");
    }

    // Create the PNG end info struct (optional, but good practice for completeness)
    png_infop end_info = png_create_info_struct(png_ptr);
    if (end_info == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
        throw GImage::Error("Unable to initialize PNG decoder.");
    }

    // Set up the custom read function to use BinaryInput
    png_set_read_fn(png_ptr, (png_voidp)&input, png_read_data);

    // Read the PNG file information (header and chunks before image data)
    png_read_info(png_ptr, info_ptr);

    png_uint_32 png_width, png_height;
    int bit_depth, color_type, interlace_type;

    // Get image header information
    png_get_IHDR(png_ptr, info_ptr, &png_width, &png_height, &bit_depth, &color_type,
        &interlace_type, NULL, NULL);

    m_width  = static_cast<uint32>(png_width);
    m_height = static_cast<uint32>(png_height);

    // Transformations for consistent 8-bit per channel output:

    // Swap bytes of 16-bit files to least significant byte first (if applicable).
    // Note: If png_set_strip_16 is used, this might be redundant as data is stripped to 8-bit.
    png_set_swap(png_ptr);

    // Strip 16-bit per channel data down to 8-bit per channel
    png_set_strip_16(png_ptr);

    // Expand paletted colors into true RGB triplets
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    // Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel
    // Using png_set_expand_gray_1_2_4_to_8 for modern libpng.
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    // Expand paletted or RGB images with transparency (tRNS chunk) to full alpha channels
    // so the data will be available as RGBA quartets.
    bool has1BitAlpha = png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS);
    if (has1BitAlpha) {
        png_set_tRNS_to_alpha(png_ptr);
    }

    // Fix sub-8 bit_depth to 8bit (for packing/unpacking pixels)
    if (bit_depth < 8) {
        png_set_packing(png_ptr);
    }

    size_t bytesToAlloc = 0;

    // Determine the number of channels based on color type and transformations applied
    if (color_type == PNG_COLOR_TYPE_RGBA ||
        (color_type == PNG_COLOR_TYPE_PALETTE && has1BitAlpha) || // If palette and has transparency (tRNS)
        (color_type == PNG_COLOR_TYPE_RGB && has1BitAlpha))       // If RGB and has transparency (tRNS)
    {
        m_channels = 4; // Will be RGBA
    }
    else if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_PALETTE)
    {
        m_channels = 3; // Will be RGB (after palette expansion if applicable)
    }
    else if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA || (color_type == PNG_COLOR_TYPE_GRAY && has1BitAlpha))
    {
        m_channels = 2; // Will be Grayscale with Alpha
    }
    else if (color_type == PNG_COLOR_TYPE_GRAY)
    {
        m_channels = 1; // Will be Grayscale
    }
    else
    {
        throw GImage::Error("Unsupported PNG bit-depth or type after transformations.");
    }

    // Allocate memory for the image data
    bytesToAlloc = m_width * m_height * m_channels;
    m_byte = (uint8*)m_memMan->alloc(bytesToAlloc);

    if (!m_byte)
    {
        std::stringstream ss;
        ss << "Out of memory while allocating " << bytesToAlloc << " bytes";
        throw GImage::Error(ss.str());
    }

    // Since we are reading row by row, this is required to handle interlacing
    uint32 number_passes = png_set_interlace_handling(png_ptr);

    // Update the info structure after all transformations have been set
    png_read_update_info(png_ptr, info_ptr);

    // Read the image data row by row
    for (uint32 pass = 0; pass < number_passes; ++pass) {
        for (uint32 y = 0; y < (uint32)m_height; ++y) {
            png_bytep rowPointer = &m_byte[m_width * m_channels * y];
            png_read_rows(png_ptr, &rowPointer, NULL, 1); // Read one row at a time
        }
    }

    // Finish reading the PNG file
    png_read_end(png_ptr, info_ptr);

    // Destroy the libpng read structures
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
}

} // namespace G3D
