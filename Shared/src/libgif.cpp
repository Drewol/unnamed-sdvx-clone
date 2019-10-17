/*
 *  Cross platform GIF source code.
 *
 *  Platform: Neutral
 *
 *  Version: 2.30  1997/07/07  Original version by Lachlan Patrick.
 *  Version: 2.35  1998/09/09  Minor upgrade to list functions.
 *  Version: 2.50  2000/01/01  Added the ability to load an animated gif.
 *  Version: 3.00  2001/03/03  Fixed a few bugs and updated the interface.
 *  Version: 3.34  2002/12/18  Debugging code is now better encapsulated.
 *  Version: 3.56  2005/08/09  Silenced a size_t conversion warning.
 *  Version: 3.60  2007/06/06  Fixed a memory leak in del_gif.
 */

/* Copyright (c) L. Patrick

   This file is part of the App cross-platform programming package.
   You may redistribute it and/or modify it under the terms of the
   App Software License. See the file LICENSE.TXT for details.
*/

/*
 *  Gif.c - Cross-platform code for loading and saving GIFs
 *
 *  The LZW encoder and decoder used in this file were
 *  written by Gershon Elber and Eric S. Raymond as part of
 *  the GifLib package.
 *
 *  The remainder of the code was written by Lachlan Patrick
 *  as part of the GraphApp cross-platform graphics library.
 *
 *  GIF(sm) is a service mark property of CompuServe Inc.
 *  For better compression and more features than GIF,
 *  use PNG: the Portable Network Graphics format.
 */

/*
 *  Copyright and patent information:
 *
 *  Because the LZW algorithm has been patented by
 *  CompuServe Inc, you probably can't use this file
 *  in a commercial application without first paying
 *  CompuServe the appropriate licensing fee.
 *  Contact CompuServe for more information about that.
 */

/*
 *  Known problems with this code:
 *
 *  There is really only one thing to watch out for:
 *  on a PC running a 16-bit operating system, such
 *  as Windows 95 or Windows 3.1, there is a 64K limit
 *  to the size of memory blocks. This may limit the
 *  size of GIF files you can load, perhaps to less
 *  than 256 pixels x 256 pixels. The new row pointer
 *  technique used in this version of this file should
 *  remove that limitation, but you should test this
 *  on your system before believing me.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "libgif.hpp"

/*
 *  GIF memory allocation helper functions.
 */

void * gif_alloc(long bytes)
{
	return app_zero_alloc(bytes);
}

/*
 *  GIF file input/output functions.
 */

static unsigned char read_byte(FILE *file)
{
	int ch = getc(file);
	if (ch == EOF)
		ch = 0;
	return ch;
}

static int write_byte(FILE *file, int ch)
{
	return putc(ch, file);
}

static int read_stream(FILE *file, unsigned char buffer[], int length)
{
	int count = (int) fread(buffer, 1, length, file);
	int i = count;
	while (i < length)
		buffer[i++] = '\0';
	return count;
}

static int write_stream(FILE *file, unsigned char buffer[], int length)
{
	return (int) fwrite(buffer, 1, length, file);
}

int read_gif_int(FILE *file)
{
	int output;
	unsigned char buf[2];

	if (fread(buf, 1, 2, file) != 2)
		return 0;
	output = (((unsigned int) buf[1]) << 8) | buf[0];
	return output;
}

void write_gif_int(FILE *file, int output)
{
	putc ( (output & 0xff), file);
	putc ( (((unsigned int) output) >> 8) & 0xff, file);
}

/*
 *  Gif data blocks:
 */

GifData * new_gif_data(int size)
{
	GifData *data = gif_alloc(sizeof(GifData));
	if (data) {
		data->byte_count = size;
		data->bytes = app_zero_alloc(size * sizeof(unsigned char));
	}
	return data;
}

void del_gif_data(GifData *data)
{
	app_free(data->bytes);
	app_free(data);
}

/*
 *  Read one code block from the Gif file.
 *  This routine should be called until NULL is returned.
 *  Use app_free() to free the returned array of bytes.
 */
GifData * read_gif_data(FILE *file)
{
	GifData *data;
	int size;

	size = read_byte(file);

	if (size > 0) {
		data = new_gif_data(size);
		read_stream(file, data->bytes, size);
	}
	else {
		data = NULL;
	}
	return data;
}

/*
 *  Write a Gif data block to a file.
 *  A Gif data block is a size-byte followed by that many
 *  bytes of data (0 to 255 of them).
 */
void write_gif_data(FILE *file, GifData *data)
{
	if (data) {
		write_byte(file, data->byte_count);
		write_stream(file, data->bytes, data->byte_count);
	}
	else
		write_byte(file, 0);
}

#ifdef GIF_DEBUG
void print_gif_data(FILE *file, GifData *data)
{
	int i, ch, prev;
	int ch_printable, prev_printable;

	if (data) {
		fprintf(file, "(length=%d) [", data->byte_count);
		prev_printable = 1;
		for (i=0; i < data->byte_count; i++) {
			ch = data->bytes[i];
			ch_printable = isprint(ch) ? 1 : 0;

			if (ch_printable != prev_printable)
				fprintf(file, " ");

			if (ch_printable)
				fprintf(file, "%c", (char)ch);
			else
				fprintf(file, "%02X,", ch);

			prev = ch;
			prev_printable = isprint(prev) ? 1 : 0;
		}
		fprintf(file, "]\n");
	}
	else {
		fprintf(file, "[]\n");
	}
}
#endif

/*
 *  Read the next byte from a Gif file.
 *
 *  This function is aware of the block-nature of Gif files,
 *  and will automatically skip to the next block to find
 *  a new byte to read, or return 0 if there is no next block.
 */
static unsigned char read_gif_byte(FILE *file, GifDecoder *decoder)
{
	unsigned char *buf = decoder->buf;
	unsigned char next;

	if (decoder->file_state == IMAGE_COMPLETE)
		return '\0';

	if (decoder->position == decoder->bufsize)
	{	/* internal buffer now empty! */
		/* read the block size */
		decoder->bufsize = read_byte(file);
		if (decoder->bufsize == 0) {
			decoder->file_state = IMAGE_COMPLETE;
			return '\0';
		}
		read_stream(file, buf, decoder->bufsize);
		next = buf[0];
		decoder->position = 1;	/* where to get chars */
	}
	else {
		next = buf[decoder->position++];
	}

	return next;
}

/*
 *  Read to end of an image, including the zero block.
 */
static void finish_gif_picture(FILE *file, GifDecoder *decoder)
{
	unsigned char *buf = decoder->buf;

	while (decoder->bufsize != 0) {
		decoder->bufsize = read_byte(file);
		if (decoder->bufsize == 0) {
			decoder->file_state = IMAGE_COMPLETE;
			break;
		}
		read_stream(file, buf, decoder->bufsize);
	}
}

/*
 *  Write a byte to a Gif file.
 *
 *  This function is aware of Gif block structure and buffers
 *  chars until 255 can be written, writing the size byte first.
 *  If FLUSH_OUTPUT is the char to be written, the buffer is
 *  written and an empty block appended.
 */
static void write_gif_byte(FILE *file, GifEncoder *encoder, int ch)
{
	unsigned char *buf = encoder->buf;

	if (encoder->file_state == IMAGE_COMPLETE)
		return;

	if (ch == FLUSH_OUTPUT)
	{
		if (encoder->bufsize) {
			write_byte(file, encoder->bufsize);
			write_stream(file, buf, encoder->bufsize);
			encoder->bufsize = 0;
		}
		/* write an empty block to mark end of data */
		write_byte(file, 0);
		encoder->file_state = IMAGE_COMPLETE;
	}
	else {
		if (encoder->bufsize == 255) {
			/* write this buffer to the file */
			write_byte(file, encoder->bufsize);
			write_stream(file, buf, encoder->bufsize);
			encoder->bufsize = 0;
		}
		buf[encoder->bufsize++] = ch;
	}
}

/*
 *  Colour maps:
 */

GifPalette * new_gif_palette(void)
{
	return gif_alloc(sizeof(GifPalette));
}

void del_gif_palette(GifPalette *cmap)
{
	app_free(cmap->colours);
	app_free(cmap);
}

void read_gif_palette(FILE *file, GifPalette *cmap)
{
	int i;
	unsigned char r, g, b;

	cmap->colours = app_alloc(cmap->length * sizeof(Colour));

	for (i=0; i<cmap->length; i++) {
		r = read_byte(file);
		g = read_byte(file);
		b = read_byte(file);
		cmap->colours[i] = rgb(r,g,b);
	}
}

void write_gif_palette(FILE *file, GifPalette *cmap)
{
	int i;
	Colour c;

	for (i=0; i<cmap->length; i++) {
		c = cmap->colours[i];
		write_byte(file, c.red);
		write_byte(file, c.green);
		write_byte(file, c.blue);
	}
}

#ifdef GIF_DEBUG
void print_gif_palette(FILE *file, GifPalette *cmap)
{
	int i;

	fprintf(file, "  GifPalette (length=%d):\n", cmap->length);
	for (i=0; i<cmap->length; i++) {
		fprintf(file, "   %02X = 0x", i);
		fprintf(file, "%02X", cmap->colours[i].red);
		fprintf(file, "%02X", cmap->colours[i].green);
		fprintf(file, "%02X\n", cmap->colours[i].blue);
	}
}
#endif

/*
 *  GifScreen:
 */

GifScreen * new_gif_screen(void)
{
	GifScreen *screen = gif_alloc(sizeof(GifScreen));
	if (screen)
		screen->cmap = new_gif_palette();
	return screen;
}

void del_gif_screen(GifScreen *screen)
{
	del_gif_palette(screen->cmap);
	app_free(screen);
}

void read_gif_screen(FILE *file, GifScreen *screen)
{
	unsigned char info;

	screen->width       = read_gif_int(file);
	screen->height      = read_gif_int(file);

	info                = read_byte(file);
	screen->has_cmap    =  (info & 0x80) >> 7;
	screen->color_res   = ((info & 0x70) >> 4) + 1;
	screen->sorted      =  (info & 0x08) >> 3;
	screen->cmap_depth  =  (info & 0x07)       + 1;

	screen->bgcolour    = read_byte(file);
	screen->aspect      = read_byte(file);

	if (screen->has_cmap) {
		screen->cmap->length = 1 << screen->cmap_depth;
		read_gif_palette(file, screen->cmap);
	}
}

void write_gif_screen(FILE *file, GifScreen *screen)
{
	unsigned char info;

	write_gif_int(file, screen->width);
	write_gif_int(file, screen->height);

	info = 0;
	info = info | (screen->has_cmap ? 0x80 : 0x00);
	info = info | ((screen->color_res - 1) << 4);
	info = info | (screen->sorted ? 0x08 : 0x00);
	if (screen->cmap_depth > 0)
		info = info | ((screen->cmap_depth) - 1);
	write_byte(file, info);

	write_byte(file, screen->bgcolour);
	write_byte(file, screen->aspect);

	if (screen->has_cmap) {
		write_gif_palette(file, screen->cmap);
	}
}

#ifdef GIF_DEBUG
void print_gif_screen(FILE *file, GifScreen *screen)
{
	fprintf(file, " GifScreen:\n");
	fprintf(file, "  width      = %d\n", screen->width);
	fprintf(file, "  height     = %d\n", screen->height);

	fprintf(file, "  has_cmap   = %d\n", screen->has_cmap ? 1:0);
	fprintf(file, "  color_res  = %d\n", screen->color_res);
	fprintf(file, "  sorted     = %d\n", screen->sorted ? 1:0);
	fprintf(file, "  cmap_depth = %d\n", screen->cmap_depth);

	fprintf(file, "  bgcolour   = %02X\n", screen->bgcolour);
	fprintf(file, "  aspect     = %d\n", screen->aspect);

	if (screen->has_cmap) {
		print_gif_palette(file, screen->cmap);
	}
}
#endif

/*
 *  GifExtension:
 */

GifExtension *new_gif_extension(void)
{
	return gif_alloc(sizeof(GifExtension));
}

void del_gif_extension(GifExtension *ext)
{
	int i;

	for (i=0; i < ext->data_count; i++)
		del_gif_data(ext->data[i]);
	app_free(ext->data);
	app_free(ext);
}

void read_gif_extension(FILE *file, GifExtension *ext)
{
	GifData *data;
	int i;

	ext->marker = read_byte(file);

	data = read_gif_data(file);
	while (data) {
		/* Append the data object: */
		i = ++ext->data_count;
		ext->data = app_realloc(ext->data, i * sizeof(GifData *));
		ext->data[i-1] = data;
		data = read_gif_data(file);
	}
}

void write_gif_extension(FILE *file, GifExtension *ext)
{
	int i;

	write_byte(file, ext->marker);

	for (i=0; i < ext->data_count; i++)
		write_gif_data(file, ext->data[i]);
	write_gif_data(file, NULL);
}

#ifdef GIF_DEBUG
void print_gif_extension(FILE *file, GifExtension *ext)
{
	int i;

	fprintf(file, " GifExtension:\n");
	fprintf(file, "  marker = 0x%02X\n", ext->marker);
	for (i=0; i < ext->data_count; i++) {
		fprintf(file, "  data = ");
		print_gif_data(file, ext->data[i]);
	}
}
#endif

/*
 *  GifDecoder:
 */

GifDecoder * new_gif_decoder(void)
{
	return gif_alloc(sizeof(GifDecoder));
}

void del_gif_decoder(GifDecoder *decoder)
{
	app_free(decoder);
}

void init_gif_decoder(FILE *file, GifDecoder *decoder)
{
	int i, depth;
	int lzw_min;
	unsigned int *prefix;

	lzw_min = read_byte(file);
	depth = lzw_min;

	decoder->file_state   = IMAGE_LOADING;
	decoder->position     = 0;
	decoder->bufsize      = 0;
	decoder->buf[0]       = 0;
	decoder->depth        = depth;
	decoder->clear_code   = (1 << depth);
	decoder->eof_code     = decoder->clear_code + 1;
	decoder->running_code = decoder->eof_code + 1;
	decoder->running_bits = depth + 1;
	decoder->max_code_plus_one = 1 << decoder->running_bits;
	decoder->stack_ptr    = 0;
	decoder->prev_code    = NO_SUCH_CODE;
	decoder->shift_state  = 0;
	decoder->shift_data   = 0;

	prefix = decoder->prefix;
	for (i = 0; i <= LZ_MAX_CODE; i++)
		prefix[i] = NO_SUCH_CODE;
}

/*
 *  Read the next Gif code word from the file.
 *
 *  This function looks in the decoder to find out how many
 *  bits to read, and uses a buffer in the decoder to remember
 *  bits from the last byte input.
 */
int read_gif_code(FILE *file, GifDecoder *decoder)
{
	int code;
	unsigned char next_byte;
	static int code_masks[] = {
		0x0000, 0x0001, 0x0003, 0x0007,
		0x000f, 0x001f, 0x003f, 0x007f,
		0x00ff, 0x01ff, 0x03ff, 0x07ff,
		0x0fff
	};

	while (decoder->shift_state < decoder->running_bits)
	{
		/* Need more bytes from input file for next code: */
		next_byte = read_gif_byte(file, decoder);
		decoder->shift_data |=
		  ((unsigned long) next_byte) << decoder->shift_state;
		decoder->shift_state += 8;
	}

	code = decoder->shift_data & code_masks[decoder->running_bits];

	decoder->shift_data >>= decoder->running_bits;
	decoder->shift_state -= decoder->running_bits;

	/* If code cannot fit into running_bits bits,
	 * we must raise its size.
	 * Note: codes above 4095 are used for signalling. */
	if (++decoder->running_code > decoder->max_code_plus_one
		&& decoder->running_bits < LZ_BITS)
	{
		decoder->max_code_plus_one <<= 1;
		decoder->running_bits++;
	}
	return code;
}

/*
 *  Routine to trace the prefix-linked-list until we get
 *  a prefix which is a pixel value (less than clear_code).
 *  Returns that pixel value.
 *
 *  If the picture is defective, we might loop here forever,
 *  so we limit the loops to the maximum possible if the
 *  picture is okay, i.e. LZ_MAX_CODE times.
 */
static int trace_prefix(unsigned int *prefix, int code, int clear_code)
{
	int i = 0;

	while (code > clear_code && i++ <= LZ_MAX_CODE)
		code = prefix[code];
	return code;
}

/*
 *  The LZ decompression routine:
 *  Call this function once per scanline to fill in a picture.
 */
void read_gif_line(FILE *file, GifDecoder *decoder,
			unsigned char *line, int length)
{
    int i = 0, j;
    int current_code, eof_code, clear_code;
    int current_prefix, prev_code, stack_ptr;
    unsigned char *stack, *suffix;
    unsigned int *prefix;

    prefix	= decoder->prefix;
    suffix	= decoder->suffix;
    stack	= decoder->stack;
    stack_ptr	= decoder->stack_ptr;
    eof_code	= decoder->eof_code;
    clear_code	= decoder->clear_code;
    prev_code	= decoder->prev_code;

    if (stack_ptr != 0) {
	/* Pop the stack */
	while (stack_ptr != 0 && i < length)
		line[i++] = stack[--stack_ptr];
    }

    while (i < length)
    {
	current_code = read_gif_code(file, decoder);

	if (current_code == eof_code)
	{
	   /* unexpected EOF */
	   if (i != length - 1 || decoder->pixel_count != 0)
		return;
	   i++;
	}
	else if (current_code == clear_code)
	{
	    /* reset prefix table etc */
	    for (j = 0; j <= LZ_MAX_CODE; j++)
		prefix[j] = NO_SUCH_CODE;
	    decoder->running_code = decoder->eof_code + 1;
	    decoder->running_bits = decoder->depth + 1;
	    decoder->max_code_plus_one = 1 << decoder->running_bits;
	    prev_code = decoder->prev_code = NO_SUCH_CODE;
	}
	else {
	    /* Regular code - if in pixel range
	     * simply add it to output pixel stream,
	     * otherwise trace code-linked-list until
	     * the prefix is in pixel range. */
	    if (current_code < clear_code) {
		/* Simple case. */
		line[i++] = current_code;
	    }
	    else {
		/* This code needs to be traced:
		 * trace the linked list until the prefix is a
		 * pixel, while pushing the suffix pixels on
		 * to the stack. If finished, pop the stack
		 * to output the pixel values. */
		if ((current_code < 0) || (current_code > LZ_MAX_CODE))
			return; /* image defect */
		if (prefix[current_code] == NO_SUCH_CODE) {
		    /* Only allowed if current_code is exactly
		     * the running code:
		     * In that case current_code = XXXCode,
		     * current_code or the prefix code is the
		     * last code and the suffix char is
		     * exactly the prefix of last code! */
		    if (current_code == decoder->running_code - 2) {
			current_prefix = prev_code;
			suffix[decoder->running_code - 2]
			    = stack[stack_ptr++]
			    = trace_prefix(prefix, prev_code, clear_code);
		    }
		    else {
			return; /* image defect */
		    }
		}
		else
		    current_prefix = current_code;

		/* Now (if picture is okay) we should get
		 * no NO_SUCH_CODE during the trace.
		 * As we might loop forever (if picture defect)
		 * we count the number of loops we trace and
		 * stop if we get LZ_MAX_CODE.
		 * Obviously we cannot loop more than that. */
		j = 0;
		while (j++ <= LZ_MAX_CODE
			&& current_prefix > clear_code
			&& current_prefix <= LZ_MAX_CODE)
		{
		    stack[stack_ptr++] = suffix[current_prefix];
		    current_prefix = prefix[current_prefix];
		}
		if (j >= LZ_MAX_CODE || current_prefix > LZ_MAX_CODE)
		    return; /* image defect */

		/* Push the last character on stack: */
		stack[stack_ptr++] = current_prefix;

		/* Now pop the entire stack into output: */
		while (stack_ptr != 0 && i < length)
		    line[i++] = stack[--stack_ptr];
	    }
	    if (prev_code != NO_SUCH_CODE) {
		if ((decoder->running_code < 2) ||
		  (decoder->running_code > LZ_MAX_CODE+2))
			return; /* image defect */
		prefix[decoder->running_code - 2] = prev_code;

		if (current_code == decoder->running_code - 2) {
		    /* Only allowed if current_code is exactly
		     * the running code:
		     * In that case current_code = XXXCode,
		     * current_code or the prefix code is the
		     * last code and the suffix char is
		     * exactly the prefix of the last code! */
		    suffix[decoder->running_code - 2]
			= trace_prefix(prefix, prev_code, clear_code);
		}
		else {
		    suffix[decoder->running_code - 2]
			= trace_prefix(prefix, current_code, clear_code);
		}
	    }
	    prev_code = current_code;
	}
    }

    decoder->prev_code = prev_code;
    decoder->stack_ptr = stack_ptr;
}

/*
 *  Hash table:
 */

/*
 *  The 32 bits contain two parts: the key & code:
 *  The code is 12 bits since the algorithm is limited to 12 bits
 *  The key is a 12 bit prefix code + 8 bit new char = 20 bits.
 */
#define HT_GET_KEY(x)	((x) >> 12)
#define HT_GET_CODE(x)	((x) & 0x0FFF)
#define HT_PUT_KEY(x)	((x) << 12)
#define HT_PUT_CODE(x)	((x) & 0x0FFF)

/*
 *  Generate a hash key from the given unique key.
 *  The given key is assumed to be 20 bits as follows:
 *    lower 8 bits are the new postfix character,
 *    the upper 12 bits are the prefix code.
 */
static int gif_hash_key(unsigned long key)
{
	return ((key >> 12) ^ key) & HT_KEY_MASK;
}

/*
 *  Clear the hash_table to an empty state.
 */
static void clear_gif_hash_table(unsigned long *hash_table)
{
	int i;
	for (i=0; i<HT_SIZE; i++)
		hash_table[i] = 0xFFFFFFFFL;
}

/*
 *  Insert a new item into the hash_table.
 *  The data is assumed to be new.
 */
static void add_gif_hash_entry(unsigned long *hash_table, unsigned long key, int code)
{
	int hkey = gif_hash_key(key);

	while (HT_GET_KEY(hash_table[hkey]) != 0xFFFFFL) {
		hkey = (hkey + 1) & HT_KEY_MASK;
	}
	hash_table[hkey] = HT_PUT_KEY(key) | HT_PUT_CODE(code);
}

/*
 *  Determine if given key exists in hash_table and if so
 *  returns its code, otherwise returns -1.
 */
static int lookup_gif_hash(unsigned long *hash_table, unsigned long key)
{
	int hkey = gif_hash_key(key);
	unsigned long htkey;

	while ((htkey = HT_GET_KEY(hash_table[hkey])) != 0xFFFFFL) {
		if (key == htkey)
			return HT_GET_CODE(hash_table[hkey]);
		hkey = (hkey + 1) & HT_KEY_MASK;
	}
	return -1;
}

/*
 *  GifEncoder:
 */

GifEncoder *new_gif_encoder(void)
{
	return gif_alloc(sizeof(GifEncoder));
}

void del_gif_encoder(GifEncoder *encoder)
{
	app_free(encoder);
}

/*
 *  Write a Gif code word to the output file.
 *
 *  This function packages code words up into whole bytes
 *  before writing them. It uses the encoder to store
 *  codes until enough can be packaged into a whole byte.
 */
void write_gif_code(FILE *file, GifEncoder *encoder, int code)
{
	if (code == FLUSH_OUTPUT) {
		/* write all remaining data */
		while (encoder->shift_state > 0)
		{
			write_gif_byte(file, encoder,
				encoder->shift_data & 0xff);
			encoder->shift_data >>= 8;
			encoder->shift_state -= 8;
		}
		encoder->shift_state = 0;
		write_gif_byte(file, encoder, FLUSH_OUTPUT);
	}
	else {
		encoder->shift_data |=
			((long) code) << encoder->shift_state;
		encoder->shift_state += encoder->running_bits;

		while (encoder->shift_state >= 8)
		{
			/* write full bytes */
			write_gif_byte(file, encoder,
				encoder->shift_data & 0xff);
			encoder->shift_data >>= 8;
			encoder->shift_state -= 8;
		}
	}

	/* If code can't fit into running_bits bits, raise its size.
	 * Note that codes above 4095 are for signalling. */
	if (encoder->running_code >= encoder->max_code_plus_one
		&& code <= 4095)
	{
    		encoder->max_code_plus_one = 1 << ++encoder->running_bits;
	}
}

/*
 *   Initialise the encoder, given a GifPalette depth.
 */
void init_gif_encoder(FILE *file, GifEncoder *encoder, int depth)
{
	int lzw_min = depth = (depth < 2 ? 2 : depth);

	encoder->file_state   = IMAGE_SAVING;
	encoder->position     = 0;
	encoder->bufsize      = 0;
	encoder->buf[0]       = 0;
	encoder->depth        = depth;
	encoder->clear_code   = (1 << depth);
	encoder->eof_code     = encoder->clear_code + 1;
	encoder->running_code = encoder->eof_code + 1;
	encoder->running_bits = depth + 1;
	encoder->max_code_plus_one = 1 << encoder->running_bits;
	encoder->current_code = FIRST_CODE;
	encoder->shift_state  = 0;
	encoder->shift_data   = 0;

	/* Write the LZW minimum code size: */
	write_byte(file, lzw_min);

	/* Clear hash table, output Clear code: */
	clear_gif_hash_table(encoder->hash_table);
	write_gif_code(file, encoder, encoder->clear_code);
}

/*
 *  Write one scanline of pixels out to the Gif file,
 *  compressing that line using LZW into a series of codes.
 */
void write_gif_line(FILE *file, GifEncoder *encoder, unsigned char *line, int length)
{
    int i = 0, current_code, new_code;
    unsigned long new_key;
    unsigned char pixval;
    unsigned long *hash_table;

    hash_table = encoder->hash_table;

    if (encoder->current_code == FIRST_CODE)
	current_code = line[i++];
    else
	current_code = encoder->current_code;

    while (i < length)
    {
	pixval = line[i++]; /* Fetch next pixel from stream */

	/* Form a new unique key to search hash table for the code
	 * Combines current_code as prefix string with pixval as
	 * postfix char */
	new_key = (((unsigned long) current_code) << 8) + pixval;
	if ((new_code = lookup_gif_hash(hash_table, new_key)) >= 0) {
	    /* This key is already there, or the string is old,
	     * so simply take new code as current_code */
	    current_code = new_code;
	}
	else {
	    /* Put it in hash table, output the prefix code,
	     * and make current_code equal to pixval */
	    write_gif_code(file, encoder, current_code);
	    current_code = pixval;

	    /* If the hash_table if full, send a clear first
	     * then clear the hash table: */
	    if (encoder->running_code >= LZ_MAX_CODE) {
		write_gif_code(file, encoder, encoder->clear_code);
		encoder->running_code = encoder->eof_code + 1;
		encoder->running_bits = encoder->depth + 1;
		encoder->max_code_plus_one = 1 << encoder->running_bits;
		clear_gif_hash_table(hash_table);
	    }
	    else {
		/* Put this unique key with its relative code in hash table */
		add_gif_hash_entry(hash_table, new_key, encoder->running_code++);
	    }
	}
    }

    /* Preserve the current state of the compression algorithm: */
    encoder->current_code = current_code;
}

void flush_gif_encoder(FILE *file, GifEncoder *encoder)
{
	write_gif_code(file, encoder, encoder->current_code);
	write_gif_code(file, encoder, encoder->eof_code);
	write_gif_code(file, encoder, FLUSH_OUTPUT);
}

/*
 *  GifPicture:
 */

GifPicture * new_gif_picture(void)
{
	GifPicture *pic = gif_alloc(sizeof(GifPicture));
	if (pic) {
		pic->cmap = new_gif_palette();
		pic->data = NULL;
	}
	return pic;
}

void del_gif_picture(GifPicture *pic)
{
	int row;

	del_gif_palette(pic->cmap);
	if (pic->data) {
		for (row=0; row < pic->height; row++)
			app_free(pic->data[row]);
		app_free(pic->data);
	}
	app_free(pic);
}

static void read_gif_picture_data(FILE *file, GifPicture *pic)
{
	GifDecoder *decoder;
	long w, h;
	int interlace_start[] = {0, 4, 2, 1};
	int interlace_step[]  = {8, 8, 4, 2};
	int scan_pass, row;

	w = pic->width;
	h = pic->height;
	pic->data = app_alloc(h * sizeof(unsigned char *));
	if (pic->data == NULL)
		return;
	for (row=0; row < h; row++)
		pic->data[row] = app_zero_alloc(w * sizeof(unsigned char));

	decoder = new_gif_decoder();
	init_gif_decoder(file, decoder);

	if (pic->interlace) {
	  for (scan_pass = 0; scan_pass < 4; scan_pass++) {
	    row = interlace_start[scan_pass];
	    while (row < h) {
	      read_gif_line(file, decoder, pic->data[row], w);
	      row += interlace_step[scan_pass];
	    }
	  }
	}
	else {
	  row = 0;
	  while (row < h) {
	    read_gif_line(file, decoder, pic->data[row], w);
	    row += 1;
	  }
	}
	finish_gif_picture(file, decoder);

	del_gif_decoder(decoder);
}

void read_gif_picture(FILE *file, GifPicture *pic)
{
	unsigned char info;

	pic->left   = read_gif_int(file);
	pic->top    = read_gif_int(file);
	pic->width  = read_gif_int(file);
	pic->height = read_gif_int(file);

	info = read_byte(file);
	pic->has_cmap    = (info & 0x80) >> 7;
	pic->interlace   = (info & 0x40) >> 6;
	pic->sorted      = (info & 0x20) >> 5;
	pic->reserved    = (info & 0x18) >> 3;

	if (pic->has_cmap) {
		pic->cmap_depth  = (info & 0x07) + 1;
		pic->cmap->length = 1 << pic->cmap_depth;
		read_gif_palette(file, pic->cmap);
	}

	read_gif_picture_data(file, pic);
}

static void write_gif_picture_data(FILE *file, GifPicture *pic)
{
	GifEncoder *encoder;
	long w, h;
	int interlace_start[] = {0, 4, 2, 1};
	int interlace_step[]  = {8, 8, 4, 2};
	int scan_pass, row;

	w = pic->width;
	h = pic->height;

	encoder = new_gif_encoder();
	init_gif_encoder(file, encoder, pic->cmap_depth);

	if (pic->interlace) {
	  for (scan_pass = 0; scan_pass < 4; scan_pass++) {
	    row = interlace_start[scan_pass];
	    while (row < h) {
	      write_gif_line(file, encoder, pic->data[row], w);
	      row += interlace_step[scan_pass];
	    }
	  }
	}
	else {
	  row = 0;
	  while (row < h) {
	    write_gif_line(file, encoder, pic->data[row], w);
	    row += 1;
	  }
	}

	flush_gif_encoder(file, encoder);
	del_gif_encoder(encoder);
}

void write_gif_picture(FILE *file, GifPicture *pic)
{
	unsigned char info;

	write_gif_int(file, pic->left);
	write_gif_int(file, pic->top);
	write_gif_int(file, pic->width);
	write_gif_int(file, pic->height);

	info = 0;
	info = info | (pic->has_cmap    ? 0x80 : 0x00);
	info = info | (pic->interlace   ? 0x40 : 0x00);
	info = info | (pic->sorted      ? 0x20 : 0x00);
	info = info | ((pic->reserved << 3) & 0x18);
	if (pic->has_cmap)
		info = info | (pic->cmap_depth - 1);

	write_byte(file, info);

	if (pic->has_cmap)
		write_gif_palette(file, pic->cmap);

	write_gif_picture_data(file, pic);
}

#ifdef GIF_DEBUG
static void print_gif_picture_data(FILE *file, GifPicture *pic)
{
	int pixval, row, col;

	for (row = 0; row < pic->height; row++) {
	  fprintf(file, "   [");
	  for (col = 0; col < pic->width; col++) {
	    pixval = pic->data[row][col];
	    fprintf(file, "%02X", pixval);
	  }
	  fprintf(file, "]\n");
	}
}

static void print_gif_picture_header(FILE *file, GifPicture *pic)
{
	fprintf(file, " GifPicture:\n");
	fprintf(file, "  left       = %d\n", pic->left);
	fprintf(file, "  top        = %d\n", pic->top);
	fprintf(file, "  width      = %d\n", pic->width);
	fprintf(file, "  height     = %d\n", pic->height);

	fprintf(file, "  has_cmap   = %d\n", pic->has_cmap);
	fprintf(file, "  interlace  = %d\n", pic->interlace);
	fprintf(file, "  sorted     = %d\n", pic->sorted);
	fprintf(file, "  reserved   = %d\n", pic->reserved);
	fprintf(file, "  cmap_depth = %d\n", pic->cmap_depth);
}

void print_gif_picture(FILE *file, GifPicture *pic)
{
	print_gif_picture_header(file, pic);

	if (pic->has_cmap)
		print_gif_palette(file, pic->cmap);

	print_gif_picture_data(file, pic);
}
#endif

/*
 *  GifBlock:
 */

GifBlock *new_gif_block(void)
{
	return gif_alloc(sizeof(GifBlock));
}

void del_gif_block(GifBlock *block)
{
	if (block->pic)
		del_gif_picture(block->pic);
	if (block->ext)
		del_gif_extension(block->ext);
	app_free(block);
}

void read_gif_block(FILE *file, GifBlock *block)
{
	block->intro = read_byte(file);
	if (block->intro == 0x2C) {
		block->pic = new_gif_picture();
		read_gif_picture(file, block->pic);
	}
	else if (block->intro == 0x21) {
		block->ext = new_gif_extension();
		read_gif_extension(file, block->ext);
	}
}

void write_gif_block(FILE *file, GifBlock *block)
{
	write_byte(file, block->intro);
	if (block->pic)
		write_gif_picture(file, block->pic);
	if (block->ext)
		write_gif_extension(file, block->ext);
}

#ifdef GIF_DEBUG
void print_gif_block(FILE *file, GifBlock *block)
{
	fprintf(file, " GifBlock (intro=0x%02X):\n", block->intro);
	if (block->pic)
		print_gif_picture(file, block->pic);
	if (block->ext)
		print_gif_extension(file, block->ext);
}
#endif

/*
 *  Gif:
 */

Gif * new_gif(void)
{
	Gif *gif = gif_alloc(sizeof(Gif));
	if (gif) {
		strcpy(gif->header, "GIF87a");
		gif->screen = new_gif_screen();
		gif->blocks = NULL;
	}
	return gif;
}

void del_gif(Gif *gif)
{
	int i;

	del_gif_screen(gif->screen);
	for (i=0; i < gif->block_count; i++)
		del_gif_block(gif->blocks[i]);
	app_free(gif->blocks);
	app_free(gif);
}

void read_gif(FILE *file, Gif *gif)
{
	int i;
	GifBlock *block;

	for (i=0; i<6; i++)
		gif->header[i] = read_byte(file);
	if (strncmp(gif->header, "GIF", 3) != 0)
		return; /* error */

	read_gif_screen(file, gif->screen);

	while (1) {
		block = new_gif_block();
		read_gif_block(file, block);

		if (block->intro == 0x3B) {	/* terminator */
			del_gif_block(block);
			break;
		}
		else  if (block->intro == 0x2C) {	/* image */
			/* Append the block: */
			i = ++gif->block_count;
			gif->blocks = app_realloc(gif->blocks, i * sizeof(GifBlock *));
			gif->blocks[i-1] = block;
		}
		else  if (block->intro == 0x21) {	/* extension */
			/* Append the block: */
			i = ++gif->block_count;
			gif->blocks = app_realloc(gif->blocks, i * sizeof(GifBlock *));
			gif->blocks[i-1] = block;
		}
		else {	/* error */
			del_gif_block(block);
			break;
		}
	}
}

void read_one_gif_picture(FILE *file, Gif *gif)
{
	int i;
	GifBlock *block;

	for (i=0; i<6; i++)
		gif->header[i] = read_byte(file);
	if (strncmp(gif->header, "GIF", 3) != 0)
		return; /* error */

	read_gif_screen(file, gif->screen);

	while (1) {
		block = new_gif_block();
		read_gif_block(file, block);

		if (block->intro == 0x3B) {	/* terminator */
			del_gif_block(block);
			break;
		}
		else if (block->intro == 0x2C) { /* image */
			/* Append the block: */
			i = ++gif->block_count;
			gif->blocks = app_realloc(gif->blocks, i * sizeof(GifBlock *));
			gif->blocks[i-1] = block;
			break;
		}
		else if (block->intro == 0x21) { /* extension */
			/* Append the block: */
			i = ++gif->block_count;
			gif->blocks = app_realloc(gif->blocks, i * sizeof(GifBlock *));
			gif->blocks[i-1] = block;
			continue;
		}
		else {	/* error! */
			del_gif_block(block);
			break;
		}
	}
}

void write_gif(FILE *file, Gif *gif)
{
	int i;

	fprintf(file, "%s", gif->header);
	write_gif_screen(file, gif->screen);
	for (i=0; i < gif->block_count; i++)
		write_gif_block(file, gif->blocks[i]);
	write_byte(file, 0x3B);
}

#ifdef GIF_DEBUG
void print_gif(FILE *file, Gif *gif)
{
	int i;

	fprintf(file, "Gif header=%s\n", gif->header);
	print_gif_screen(file, gif->screen);
	for (i=0; i < gif->block_count; i++)
		print_gif_block(file, gif->blocks[i]);
	fprintf(file, "End of gif.\n\n");
}
#endif

/*
 *  Reading and Writing Gif files:
 */

Gif * read_gif_file(const char *filename)
{
	Gif *gif;
	FILE *file;

	file = app_open_file(filename, "rb");
	if (file == NULL)
		return NULL;
	gif = new_gif();
	if (gif == NULL) {
		app_close_file(file);
		return NULL;
	}
	read_gif(file, gif);
	app_close_file(file);
	if (strncmp(gif->header, "GIF", 3) != 0) {
		del_gif(gif);
		gif = NULL;
	}
	return gif;
}

void write_gif_file(const char *filename, Gif *gif)
{
	FILE *file;

	file = app_open_file(filename, "wb");
	if (file == NULL)
		return;
	if (gif == NULL) {
		app_close_file(file);
		return;
	}
	write_gif(file, gif);
	app_close_file(file);
}
  