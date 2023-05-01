#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include <eZ80.h>
#include <defines.h>

#include "mos-interface.h"
#include "vdp.h"
#include "agontimer.h"

extern void write16bit(UINT16 w);
extern void write32bit(UINT32 w);

uint8_t vgm_file;

int min(int a, int b) {
    if (a > b)
        return b;
    return a;
}

int max(int a, int b) {
    if (a > b)
        return a;
    return b;
}

void delay_ticks(UINT16 ticks_end) { //16.7ms ticks
	
	UINT32 ticks = 0;
	ticks_end *= 6;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

void delay_cents(UINT16 ticks_end) { //~100ms ticks
	
	UINT32 ticks = 0;
	ticks_end *= 6;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

void delay_secs(UINT16 ticks_end) { //~1 sec ticks
	
	UINT32 ticks = 0;
	ticks_end *= 60;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

int load_bmp(const char* filename, UINT8 slot) { //Slowest but almost no memory demand.

    int32_t width, height, bit_depth, row_padding, y, x, i, n;
    uint8_t pixel[4], file, r, g, b, index;
	char header[54], color_table[1024];
	uint32_t pixel_value, color_table_size;
	uint32_t biSize;
	
    file = mos_fopen(filename, fa_read);
    if (!file) {
        printf("Error: could not open file.\n");
        return 0;
    }
	
	mos_fread(file, header, 54);

    biSize = *(UINT32*)&header[14];
	width = *(INT32*)&header[18];
    height = *(INT32*)&header[22];
    bit_depth = *(UINT16*)&header[28];
	color_table_size = *(UINT32*)&header[46];
	
	if (color_table_size == 0 && bit_depth == 8) {
        color_table_size = 256;
    }
	
	if (color_table_size > 0) mos_fread(file, color_table, color_table_size * 4);
	
	else if (biSize > 40) { //If for any reason there's yet more data in the header
		
		i = biSize - 40;
		while (i-- > 0) {
            mos_fgetc(file);
        }
		
	}


	if ((bit_depth != 32) && (bit_depth != 24) && (bit_depth != 8)) {
        printf("Error: unsupported bit depth (not 8, 24 or 32-bit).\n");
        mos_fclose(file);
        return 0;
    }

    row_padding = (4 - (width * (bit_depth / 8)) % 4) % 4;

	vdp_bitmapSelect(slot);
	putch(23); // vdu_sys
	putch(27); // sprite command
	putch(100);  // send data to selected bitmap
	
	write16bit(width);
	write16bit(height);
	
	if (bit_depth == 32) putch(1);
	if (bit_depth == 24 || bit_depth == 8) putch(0);
	
	if (bit_depth == 8) {
		
        for (y = height - 1; y >= 0; y--) {
            for (x = 0; x < width; x++) {
				
					index = (UINT8)mos_fgetc(file);
					b = color_table[index * 4];
					g = color_table[index * 4 + 1];
					r = color_table[index * 4 + 2];
					putch(b);
					putch(g);
					putch(r);
				

            }

            for (i = 0; i < row_padding; i++) {
                mos_fgetc(file);
            }
			
        }
		
	}
	
    else if (bit_depth == 32 || bit_depth == 24) {
        for (y = height - 1; y >= 0; y--) {
            for (x = 0; x < width; x++) {
					
					for (i = 0; i < bit_depth / 8; i++) {
						
						putch(mos_fgetc(file));
						
					}
				

            }

            for (i = 0; i < row_padding; i++) {
                mos_fgetc(file);
            }
			
        }
		//printf("Width: %u, Height: %u, BPP: %u", width, height, bit_depth);    
	}

    mos_fclose(file);
	return width * height;
	
	
}

int load_bmp_big(const char* filename, UINT8 slot) { //Uses 64x64x4 chunks

    int32_t width, height, bit_depth, row_padding = 0, y, x, i, n;
    uint8_t pixel[4], file, r, g, b, index;
	char header[54], color_table[1024];
    uint32_t pixel_value, color_table_size, bytes_per_row;
    uint32_t biSize;
	FIL *fo;
	
	char* src;
	
	uint8_t chunk_width = 64;
    uint8_t chunk_height = 64;
    int num_chunks_x;
    int num_chunks_y;
	int chunk_y, chunk_x;
	
	char *image_buffer;
	
    file = mos_fopen(filename, fa_read);
    if (!file) {
        printf("Error: could not open file.\n");
        return 0;
    }
	fo = (FIL *)mos_getfil(file);
	
	mos_fread(file, header, 54);

    biSize = *(UINT32*)&header[14];
	width = *(INT32*)&header[18];
    height = *(INT32*)&header[22];
    bit_depth = *(UINT16*)&header[28];
	color_table_size = *(UINT32*)&header[46];
	
	image_buffer = (char *) malloc(width * bit_depth / 8);
	
    num_chunks_x = (width + chunk_width - 1) / chunk_width;
    num_chunks_y = (height + chunk_height - 1) / chunk_height;	
	
	if (color_table_size == 0 && bit_depth == 8) {
        color_table_size = 256;
    }
	
	if (color_table_size > 0) mos_fread(file, color_table, color_table_size * 4);
	
	else if (biSize > 40) { //If for any reason there's yet more data in the header
		
		i = biSize - 40;
		while (i-- > 0) {
            mos_fgetc(file);
        }
		
	}


	if ((bit_depth != 32) && (bit_depth != 24) && (bit_depth != 8)) {
        printf("Error: unsupported bit depth (not 8, 24 or 32-bit).\n");
        mos_fclose(file);
        return 0;
    }

	row_padding = (4 - (width * (bit_depth / 8)) % 4) % 4;

	vdp_bitmapSelect(slot);
	putch(23); // vdu_sys
	putch(27); // sprite command
	putch(100);  // send data to selected bitmap
	
	write16bit(width);
	write16bit(height);
	
	if (bit_depth == 32) putch(1);
	if (bit_depth == 24 || bit_depth == 8) putch(0);
	
	if (bit_depth == 8) {
		
        for (y = height - 1; y >= 0; y--) {
            for (x = 0; x < width; x++) {
				
					index = (UINT8)mos_fgetc(file);
					b = color_table[index * 4];
					g = color_table[index * 4 + 1];
					r = color_table[index * 4 + 2];
					putch(b);
					putch(g);
					putch(r);
				

            }

            for (i = 0; i < row_padding; i++) {
                mos_fgetc(file);
            }
			
        }
		
	}
	
	else if (bit_depth == 32 || bit_depth == 24) {
		int non_pad_row = width * bit_depth / 8;
		bytes_per_row = (width * bit_depth / 8) + row_padding;
		
		src = (char *) malloc(width * bit_depth / 8);

		for (y = height - 1; y >= 0; y--) {
			
			mos_fread(file, src, non_pad_row);
			memcpy(image_buffer, src, non_pad_row);
			mos_puts(image_buffer, non_pad_row, 0);
			mos_flseek(file, fo->fptr + row_padding);
			
		}

	}

    mos_fclose(file);
	free(image_buffer);
	return width * height;
	
	
}

int load_bmp_small(const char* filename, UINT8 slot) { //Much faster, but reads into RAM so unsuitable for bigger than ~64x64

    int32_t width, height, bit_depth, row_padding, y, x, i, n;
    uint8_t header[54], pixel[4], color_table[1024], file, r, g, b, index;
	uint32_t pixel_value, color_table_size, bytes_per_row;
	uint32_t biSize;
	char *file_buffer;
	char *image_buffer;
	FIL *fo;
	uint24_t file_pos = 0, image_pos = 0;
	
    file = mos_fopen(filename, fa_read);
    if (!file) {
        printf("Error: could not open file.\n");
        return 0;
    }
	
	fo = (FIL *)mos_getfil(file);
	if (fo->obj.objsize > 350000) return 0;
	
	file_buffer = (char *) malloc(fo->obj.objsize);
	
	mos_fread(file, file_buffer, fo->obj.objsize);
	mos_fclose(file);

    biSize = *(UINT32*)&file_buffer[14];
	width = *(INT32*)&file_buffer[18];
    height = *(INT32*)&file_buffer[22];
    bit_depth = *(UINT16*)&file_buffer[28];
	color_table_size = *(UINT32*)&file_buffer[46];
	
	image_buffer = (char *) malloc(width * height * (bit_depth / 8));
	
	file_pos += 54; //Header
	
	if (color_table_size == 0 && bit_depth == 8) {
        color_table_size = 256;
    }
	
	if (color_table_size > 0) file_pos += color_table_size; //mos_fread(file, color_table, color_table_size * 4);
	
 	else if (biSize > 40) { //If for any reason there's yet more data in the header
		
		i = biSize - 40;
		while (i-- > 0) {
            file_pos++; //file_pos should now be at start of data
        }
		
	}


	if ((bit_depth != 32) && (bit_depth != 24)) {
        printf("Error: unsupported bit depth (not 8, 24 or 32-bit).\n");
        return 0;
    }

    row_padding = (4 - (width * (bit_depth / 8)) % 4) % 4;
	
	vdp_bitmapSelect(slot);
	putch(23); // vdu_sys
	putch(27); // sprite command
	putch(100);  // send data to selected bitmap
	
	write16bit(width);
	write16bit(height);
	
	if (bit_depth == 32) putch(1);
	else if (bit_depth == 24) putch(0);
			
	bytes_per_row = (width * bit_depth / 8) + row_padding;

	for (y = height - 1; y >= 0; y--) {
		char* dst = image_buffer + (y * width * bit_depth / 8);
		char* src = (char*)file_buffer + file_pos;
		memcpy(dst, src, width * bit_depth / 8);
		file_pos += bytes_per_row;
	}
	
	mos_puts(image_buffer, width * height * bit_depth / 8, 0);
	free(image_buffer);

    
	free(file_buffer);
	return width * height;
	
	
}



bool isKthBitSet(uint8_t data, uint8_t k) {
    return (data & (1 << k)) != 0;
}

void channel_ready(int channel) {

	waitvblank();
	while (isKthBitSet(getsysvar_audioSuccess(), channel)) continue;
		//if () break;
	
	
}

#define change_volume(channel, volume) \
    do { \
        putch(23); \
        putch(0); \
        putch(133); \
        putch((channel)); \
        putch(100); \
        putch((volume)); \
    } while (0)
	
#define change_samplerate(channel, rate) \
    do { \
        putch(23); \
        putch(0); \
        putch(133); \
        putch((channel)); \
        putch(102); \
        write16bit((rate)); \
    } while (0)	

/* void change_frequency(UINT8 channel, UINT16 frequency) {
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(101); //Non envelope but custom waveform
	write16bit(frequency);
	
} */

#define change_frequency(channel, frequency) \
    do { \
        putch(23); \
        putch(0); \
        putch(133); \
        putch((channel)); \
        putch(101); \
        write16bit((frequency)); \
    } while (0)

void play_simple(int channel, int wavetype, int vol, int duration, int frequency) {
	
	channel_ready(channel);
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(1); //Non envelope but custom waveform
	putch(wavetype);
	putch(vol);
	
	write16bit(frequency);
	write16bit(duration);
	
}

void play_simple_force(int channel, int wavetype, int vol, int frequency) {
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(4); //Non envelope but custom waveform
	putch(wavetype);
	putch(vol);
	
	write16bit(frequency);
	write16bit(0);
	
}

void play_saw_force(int channel, int vol, int frequency) {
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(3); //Simple, play forever
	putch(vol);
	
	write16bit(frequency);
	write16bit(2000);
	
}

void play_advanced(int channel, int  attack, int decay, int sustain, int release, int wavetype, int peakvol, int duration, int frequency, int freq_end, int end_style) {
	
	//channel_ready(channel);
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(2);
	putch(wavetype);
	putch(peakvol);

	write16bit(frequency);
	write16bit(duration);
	write16bit(attack);
	putch(sustain);
	write16bit(decay);
	write16bit(release);
	write16bit(freq_end);
	putch(end_style);
		
}

void play_advanced_keep(int channel, int peakvol, int duration, int frequency) {
	
	channel_ready(channel);
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(6);
	
	putch(peakvol);
	write16bit(frequency);
	write16bit(duration);
		
}

void play_sample(UINT8 channel, UINT8 sample_id, UINT8 volume) {
	
	channel_ready(channel);
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(5);
	putch(sample_id);
	putch(1); //Playback mode
	
	putch(volume);
	
}

void load_wav(const char* filename, uint8_t sample_id) {
    
	int i, j, header_size, bit_rate;
	int8_t sample8;
	int16_t sample16;
	uint32_t data_size, sample_rate;
    unsigned char header[300];
	UINT32 remainder;
	char *sample_buffer;
    UINT8 fp;
	FIL *fo;
	
    fp = mos_fopen(filename, fa_read);
    if (!fp) {
        printf("Error: could not open file %s\n", filename);
        return;
    }

    for (i = 0; i < 300 && !mos_feof(fp); i++) {
        header[i] = mos_fgetc(fp);
        if (i >= 3 && header[i - 7] == 'd' && header[i - 6] == 'a' && header[i - 5] == 't' && header[i - 4] == 'a') {
			break; // found the start of the data
        }
    }

    if (i >= 300 || !(header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F') ||
        !(header[8] == 'W' && header[9] == 'A' && header[10] == 'V' && header[11] == 'E' &&
          header[12] == 'f' && header[13] == 'm' && header[14] == 't' && header[15] == ' ') ||
        !(header[16] == 0x10 && header[17] == 0x00 && header[18] == 0x00 && header[19] == 0x00 &&
          header[20] == 0x01 && header[21] == 0x00 && header[22] == 0x01 && header[23] == 0x00)) {
        printf("Error: invalid WAV file\n");
        mos_fclose(fp);
        return;
    }
	
    sample_rate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
	data_size = header[i - 3] | (header[i - 2] << 8) | (header[i - 1] << 16) | (header[i] << 24);

    if (header[34] == 8 && header[35] == 0) {
        bit_rate = 8;
    } else {
        printf("Error: unsupported PCM format\n");
        mos_fclose(fp);
        return;
    }
	
		//remainder = data_size % 5000;	
		//printf("%u total samples.\r\n", data_size);

		//printf("Done reading.\r\n");
		putch(23);
		putch(0);
		putch(133);
		putch(0);
		putch(5); //Sample mode

		putch(sample_id); //Sample ID
		putch(0); //Record mode
		
		write32bit(data_size);
		//else if (bit_rate == 16) write32bit(data_size / 2);
		
		write16bit(sample_rate);
		
		sample_buffer = (char *) malloc(data_size);
		mos_fread(fp, sample_buffer, data_size);
		mos_puts(sample_buffer, data_size, 0);
		free(sample_buffer);

    mos_fclose(fp);
}

#define AY_3_8910_CLOCK_SPEED 1789773
//#define AY_FREQ 55930
#define AY_3_8910_CHANNELS 3

typedef struct {
    uint8_t chip_type;
	uint8_t chip_variant;
	uint32_t clock;
	uint8_t n_channels;
	uint24_t f_scale;
	uint24_t loop_start;
	
	uint8_t header_size;
	uint24_t data_start;
	uint24_t data_length;
	
	bool loop_enabled;
	
	uint32_t gd3_location;
	
	float volume_multiplier;
	bool pause;
	
	char data[256];
	uint16_t data_pointer;
} vgm_info;

vgm_info vgm_info;

typedef struct {
    uint8_t volume;
    uint8_t frequency_coarse;
    uint8_t frequency_fine;
    int frequency_hz;
	bool enabled;
} ay_channel_state;

ay_channel_state ay_states[AY_3_8910_CHANNELS];

typedef struct {
	uint8_t latched_channel;
	uint8_t latched_type;
} sn_global;

sn_global sn;

typedef struct {
    // uint8_t volume_high;
	// uint8_t volume_low;
	uint16_t volume;
    uint8_t frequency_high;
    uint8_t frequency_low;
	uint16_t frequency_combined;
    uint16_t frequency_hz;
	bool enabled;
} sn_channel_state;

sn_channel_state sn_channels[6];
uint8_t sn_vol_table[16] = {127, 113, 101, 90, 80, 71, 63, 56, 50, 45, 40, 36, 32, 28, 25, 0};

void process_0x50_command(unsigned char data) {
    //uint8_t channel = 0;
	uint8_t type = 0;
	uint16_t value = 0;
	
	if (data & 0x80) { 	//Latch data
		
        sn.latched_channel = (data >> 5) & 0x03;	// Bits 5-6 are the channel number
		sn.latched_type = (data >> 4) & 0x01;   	// Bit 4 is the type (1 = volume, 0 = frequency)
		
		if (sn.latched_type == 0) sn_channels[sn.latched_channel].frequency_low = data & 0x0F;         	// Bits 0-3 are the value
		
		else if (sn.latched_type == 1) sn_channels[sn.latched_channel].volume = sn_vol_table[data & 0x0F];	
		
	}
	
	else {			//Pure data
		
		if (sn.latched_type == 0) sn_channels[sn.latched_channel].frequency_high = data & 0x3F;         	// Bits 0-3 are the value
		else if (sn.latched_type == 1) sn_channels[sn.latched_channel].volume = sn_vol_table[data & 0x0F];
		
	}
	
	if (sn.latched_channel == 3) return; //Ignore the noise channel
	
	//For an SN76489 vgm_info.f_scale will be MASTER_CLOCK / 32.
	
	sn_channels[sn.latched_channel].frequency_hz = vgm_info.f_scale / ((sn_channels[sn.latched_channel].frequency_high << 4) | (sn_channels[sn.latched_channel].frequency_low) & 0x0F);
	
	if (sn.latched_type == 0) change_frequency(sn.latched_channel, sn_channels[sn.latched_channel].frequency_hz);
		
	else if (sn.latched_type == 1) change_volume(sn.latched_channel, sn_channels[sn.latched_channel].volume * vgm_info.volume_multiplier);	
	
}

uint8_t ay_vol_table[16] = {0, 8, 17, 25, 34, 42, 51, 59, 68, 76, 85, 93, 102, 110, 119, 127};
uint8_t ym_vol_table[32] = {0, 1, 1, 1, 2, 2, 2, 3, 4, 4, 5, 6, 7, 8, 10, 11, 13, 15, 17, 20, 23, 26, 30, 35, 39, 45, 51, 59, 67, 76, 86, 98};

UINT32 little_long(UINT8 b0, UINT8 b1, UINT8 b2, UINT8 b3) {

	UINT32 little_long = 0;
		
	little_long += (UINT32)(b0 << 24);
	little_long += (UINT32)(b1 << 16);
	little_long += (UINT32)(b2 << 8);
	little_long += (UINT32)b3;
		
	return little_long;
		
}


enum VgmParserState {
    READ_COMMAND,
    WAIT_SAMPLES,
    WRITE_REGISTER,
    END_OF_SOUND_DATA
};

enum VgmParserState state = READ_COMMAND;
unsigned int start_timer0 = 0, target_timer0 = 0;
uint24_t delay_length = 0;
uint24_t samples_to_wait = 0;

UINT8 vgm_init(UINT8 fp) {

	uint8_t vgm_min_header[64];
	uint8_t vgm_rem_header[192]; //Up to, likely less.
	uint8_t i;
	UINT32 test_clock = 0;
	vgm_info.data_length = 0;
	vgm_info.data_start = 0;
	
	vgm_info.data_pointer = 0;
	vgm_info.loop_start = 0;
	vgm_info.gd3_location = 0;
	vgm_info.header_size = 0;
	
	vgm_info.pause = false;
	
	state = READ_COMMAND;
	
	mos_fread(fp, vgm_info.data, 64);
	
	if (vgm_info.data[0] != 'V' || vgm_info.data[1] != 'g' || vgm_info.data[2] != 'm') return 0;
	
	
	if (vgm_info.data[0x34] != 0) { //Is there more header than the classic 64 bytes?
		
		vgm_info.header_size = *(uint32_t*)(vgm_info.data + 0x34) - 0xC;// (less the 12 bytes between 0x34 and 0x40)
		mos_fread(fp, vgm_info.data + 64, vgm_info.header_size);

	}

	vgm_info.data_length = *(uint32_t*)(vgm_info.data + 0x04) - 0x04 - vgm_info.header_size - 0x40;
	vgm_info.data_start = vgm_info.header_size + 0x40;

	if (*(uint32_t*)(vgm_info.data + 0x0C) != 0) { //SN76489?

		vgm_info.chip_type = 1;
		vgm_info.clock = *(uint32_t*)(vgm_info.data + 0x0C);
		vgm_info.n_channels = 3;
		
	}
	
	else if (*(uint32_t*)(vgm_info.data + 0x74) != 0) { //AY-3-8910?

		vgm_info.chip_type = 0;
		vgm_info.clock = *(uint32_t*)(vgm_info.data + 0x74);
		
		vgm_info.chip_variant = vgm_info.data[0x78];
		
		vgm_info.n_channels = 3;
		
	}
	
	if (*(uint32_t*)(vgm_info.data + 0x1C) != 0) { //Loop detected
		
		vgm_info.loop_start = *(uint32_t*)(vgm_info.data + 0x1C);
		vgm_info.loop_enabled = true;
		
	} else vgm_info.loop_start = 0;
	
	
	if (vgm_info.data[0x0C] == 0x80) {
	
		printf("Detected a dual chip VGM, exiting.\r\n");
		return 0;
		
	}
	
	if (*(uint32_t*)(vgm_info.data + 0x14) != 0) {
	
			vgm_info.gd3_location = *(uint32_t*)(vgm_info.data + 0x14) + 0x14;
		
	}
	
	mos_flseek(fp, vgm_info.data_start);
	
	vgm_info.data_pointer = 0;
	
	//timer0_begin(1044, 4); 	//44100 / 10 = 44100Hz
	timer0_begin(313, 4); 	//4410Hz / 3 = 14700Hz
	//timer0_begin(1567, 4); //44100Hz / 15
	//timer0_begin(2089, 4); //44100Hz / 20
	
	if (vgm_info.chip_type == 0) {  //AY
		
		for (i = 0; i < vgm_info.n_channels; i++) {
			
			ay_states[i].volume = 0xFF;
			ay_states[i].frequency_coarse = 0xFF;
			ay_states[i].frequency_fine = 0xFF;
			ay_states[i].frequency_hz = 0;
			ay_states[i].enabled = 0;
			
			play_simple_force(i, 0, 100, 0); //Mute
	
		}
		
		if (vgm_info.chip_variant == 0x10) vgm_info.f_scale = vgm_info.clock / 32;
		else vgm_info.f_scale = vgm_info.clock / 16;
	}
	
	else if (vgm_info.chip_type == 1) { //SN
		
		for (i = 0; i < vgm_info.n_channels; i++) {
			
			sn_channels[i].volume = 0;
			sn_channels[i].frequency_high = 0x00;
			sn_channels[i].frequency_low = 0x00;
			sn_channels[i].frequency_hz = 0;
			sn_channels[i].enabled = 0;
			
			play_simple_force(i, 0, 100, 0); //Mute
	
		}
		vgm_info.f_scale = vgm_info.clock / 32;
	}
	
	return 1;
	
}

void vgm_loop_init() {
	
	uint8_t i;
	
	timer0_end();
	timer0_begin(313, 4); //44100 / 100 = 4410Hz
	
	for (i = 0; i < vgm_info.n_channels + 1; i++) {
		
		printf("Setting up channel %u\r\n", i);
		
		ay_states[i].volume = 0xFF;
		ay_states[i].frequency_coarse = 0xFF;
		ay_states[i].frequency_fine = 0xFF;
		ay_states[i].frequency_hz = 0;
		ay_states[i].enabled = 0;
		
		play_simple_force(i, 0, 100, 0); //Mute
	
	}
	
}

void vgm_cleanup(UINT8 fp) {
	uint8_t i;
	
	mos_fclose(fp);
	for (i = 0; i < vgm_info.n_channels * 2; i++) {
		play_simple_force(i, 0, 0, 1); //Mute
	}

	start_timer0 = 0;
	target_timer0 = 0;
	delay_length = 0;
	samples_to_wait = 0;
	timer0_end();
	
}

/* UINT8 parse_vgm_file(UINT8 fp) {  //FILE BASED VERSION
    unsigned char byte, firstbyte, secondbyte;
    unsigned short reg = 0;
    unsigned char value = 0;
	
    //enum VgmParserState state = READ_COMMAND;

    //while (state != END_OF_SOUND_DATA) {
        switch (state) {
			printf("State: %u", state);
            case READ_COMMAND:
				//byte = mos_fgetc(fp);
                if (byte != EOF) {
                    switch (byte) {
                        case 0x61: // wait n samples
                            firstbyte = mos_fgetc(fp);
							secondbyte = mos_fgetc(fp);
							//samples_to_wait = (mos_fgetc(fp) << 8) | mos_fgetc(fp);
							samples_to_wait = (secondbyte << 8) | firstbyte;
							start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							//printf("Delay command: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("D ");
							//printf("DELAY 0x61: 0x%02X 0x%02X (%u) samples (timer0=%u).\r\n", firstbyte, secondbyte, samples_to_wait, start_timer0);
							return 0;
                            break;
                        case 0x62: // wait 735 samples (1/60 second)
                            samples_to_wait = 735;
							start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							//printf("Delay command: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("D ");
							//printf("DELAY 0x62: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
                            return 0;
							break;
                        case 0x63: // wait 882 samples (1/50 second)
                            samples_to_wait = 882;
							start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							//printf("DELAY 0x63: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("Delay command: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("D ");
                            return 0;
							break;
                        case 0x70: // delay n+1 samples
                        case 0x71:
                        case 0x72:
                        case 0x73:
                        case 0x74:
                        case 0x75:
                        case 0x76:
                        case 0x77:
                        case 0x78:
                        case 0x79:
                        case 0x7A:
                        case 0x7B:
                        case 0x7C:
                        case 0x7D:
                        case 0x7E:
                        case 0x7F:
                            samples_to_wait = (byte & 0x0F) + 1;
							start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							//printf("DELAY 0x7n: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("D ");
                            return 0;
							break;
                        case 0x50:
							value = mos_fgetc(fp);
							//process_0x50_command(value);
							return 0;
						case 0xA0: // write to registry
                            //reg = fread(&byte, 1, fp);
							reg = mos_fgetc(fp);
                            value = mos_fgetc(fp);
                            //ay_write_register(reg, value);
							//printf("WRITE: %u value %u.\r\n", reg, value);
							//printf("W ");
							//updateAY38910(reg, value);
							process_0xA0_command(reg,value);
                            return 0;
							break;
                        case 0x66: // end of sound data
                            state = END_OF_SOUND_DATA;
                            printf("\r\nEND OF DATA\r\n");
							// if (vgm_info.loop_start) {
								// 
								// state = READ_COMMAND;
								// vgm_cleanup(fp);
								// vgm_loop_init(fp);
								// 
								// 
							// } else return 1;
					
							return 1;
                        default:
                            // unsupported command, ignore and continue
                            return 0;
							break;
                    }
                }
                break;
            case WAIT_SAMPLES:
				target_timer0 = start_timer0 + samples_to_wait;    
				if (timer0 >= target_timer0) {
                    state = READ_COMMAND;
                    samples_to_wait = 0;
                }
                return 0;
				break;
            default:
                // invalid state, exit loop
                state = END_OF_SOUND_DATA;
                return 1;
        }
	//}
} */

UINT8 parse_vgm_file(UINT8 fp) {
    uint8_t byte, firstbyte, secondbyte;
    uint8_t reg = 0;
    uint8_t value = 0;
	    
	//printf("%u\r\n", fo->fptr);
	switch (state) {
			
            case WAIT_SAMPLES:
				//target_timer0 = start_timer0 + samples_to_wait;    
				if (timer0 >= target_timer0) {
                    state = READ_COMMAND;
                    samples_to_wait = 0;
                }
                return 0;
		
			case READ_COMMAND:
				byte = mos_fgetc(fp);
				if (byte != EOF) {
                    switch (byte) {
						case 0x61: // wait n samples
							firstbyte = mos_fgetc(fp);
							secondbyte = mos_fgetc(fp);
							target_timer0 = timer0 + (secondbyte << 8) | firstbyte;
							//samples_to_wait = (secondbyte << 8) | firstbyte;
							//start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							return 0;
                        case 0x62: // wait 735 samples (1/60 second)
                            target_timer0 = timer0 + 735;
                            state = WAIT_SAMPLES;
                            return 0;
                        case 0x63: // wait 882 samples (1/50 second)
                            target_timer0 = timer0 + 882;
                            state = WAIT_SAMPLES;
                            return 0;
                        case 0x70: // delay n+1 samples
                        case 0x71:
                        case 0x72:
                        case 0x73:
                        case 0x74:
                        case 0x75:
                        case 0x76:
                        case 0x77:
                        case 0x78:
                        case 0x79:
                        case 0x7A:
                        case 0x7B:
                        case 0x7C:
                        case 0x7D:
                        case 0x7E:
                        case 0x7F:
                            target_timer0 = timer0 + (byte & 0x0F) + 1;
                            state = WAIT_SAMPLES;
                            return 0;							
                        case 0x50:
							value = mos_fgetc(fp);
							process_0x50_command(value);
							return 0;
						case 0xA0: // write to registry
							//process_0xA0_command(mos_fgetc(fp),mos_fgetc(fp));
							switch (mos_fgetc(fp)) {
								case 0x06:
									return 0; //We're not doing noises.
								case 0x00:
									ay_states[0].frequency_fine = mos_fgetc(fp);
									ay_states[0].frequency_hz = vgm_info.f_scale / (ay_states[0].frequency_coarse << 8 | ay_states[0].frequency_fine);
									if (ay_states[0].enabled) change_frequency(0, ay_states[0].frequency_hz);
									return 0;
								case 0x01:
									ay_states[0].frequency_coarse = mos_fgetc(fp);
									ay_states[0].frequency_hz = vgm_info.f_scale / (ay_states[0].frequency_coarse << 8 | ay_states[0].frequency_fine);
									if (ay_states[0].enabled) change_frequency(0, ay_states[0].frequency_hz);
									return 0;
								case 0x08:
									ay_states[0].volume = ay_vol_table[(mos_fgetc(fp) & 0x0F)];
									if (ay_states[0].enabled) change_volume(0, ay_states[0].volume * vgm_info.volume_multiplier);
									return 0;
								case 0x09:
									ay_states[1].volume = ay_vol_table[(mos_fgetc(fp) & 0x0F)];
									if (ay_states[1].enabled) change_volume(1, ay_states[1].volume * vgm_info.volume_multiplier);
									return 0;
								case 0x0A:
									ay_states[2].volume = ay_vol_table[(mos_fgetc(fp) & 0x0F)];
									if (ay_states[2].enabled) change_volume(2, ay_states[2].volume * vgm_info.volume_multiplier);
									return 0;			
								case 0x02:
									ay_states[1].frequency_fine = mos_fgetc(fp);
									ay_states[1].frequency_hz = vgm_info.f_scale / (ay_states[1].frequency_coarse << 8 | ay_states[1].frequency_fine);
									if (ay_states[1].enabled) change_frequency(1, ay_states[1].frequency_hz);
									return 0;		
								case 0x03:
									ay_states[1].frequency_coarse = mos_fgetc(fp);
									ay_states[1].frequency_hz = vgm_info.f_scale / (ay_states[1].frequency_coarse << 8 | ay_states[1].frequency_fine);
									if (ay_states[1].enabled) change_frequency(1, ay_states[1].frequency_hz);				
									return 0;		
								case 0x04:
									ay_states[2].frequency_fine = mos_fgetc(fp);
									ay_states[2].frequency_hz = vgm_info.f_scale / (ay_states[2].frequency_coarse << 8 | ay_states[2].frequency_fine);
									if (ay_states[2].enabled) change_frequency(2, ay_states[2].frequency_hz);
									return 0;
								case 0x05:
									ay_states[2].frequency_coarse = mos_fgetc(fp);
									ay_states[2].frequency_hz = vgm_info.f_scale / (ay_states[2].frequency_coarse << 8 | ay_states[2].frequency_fine);
									if (ay_states[2].enabled) change_frequency(2, ay_states[2].frequency_hz);
									return 0;
								case 0x07:			
									value = mos_fgetc(fp);
									ay_states[0].enabled = !(value & 0x01);
									ay_states[1].enabled = !(value & 0x02);
									ay_states[2].enabled = !(value & 0x04);
									
									change_volume(0, !!ay_states[0].enabled * ay_states[0].volume * vgm_info.volume_multiplier);
									change_volume(1, !!ay_states[1].enabled * ay_states[1].volume * vgm_info.volume_multiplier);
									change_volume(2, !!ay_states[2].enabled * ay_states[2].volume * vgm_info.volume_multiplier);	
									return 0;
								
								default:
									// Invalid register, ignore
									return 0;
							}						
                            return 0;
                        case 0x66: // end of sound data
                            state = END_OF_SOUND_DATA;
							
							if (vgm_info.loop_start && vgm_info.loop_enabled) {
								
								mos_flseek(fp, vgm_info.loop_start);
								//vgm_info.data_pointer = vgm_info.loop_start;
								state = READ_COMMAND;
								return 0;
								
							} else vgm_cleanup(fp);
					
							return 1;
                        default:
                            // unsupported command, ignore and continue
                            return 0;
							break;
                    }
                }
                return 0;
            default:
                // invalid state, exit loop
                state = END_OF_SOUND_DATA;
                return 1;
        }
		return 0;
}

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Rectangle;


typedef struct {
    uint8_t id;
	uint8_t dir; //0=North, 3=East, 6=South, 9=West
	uint8_t seq; //0 to 3;
	Rectangle rect;
	
} sprite_state;

sprite_state player;

#define FRAME_WHOLE	119
#define FRAME_NW	120
#define FRAME_N		121
#define FRAME_NE	122
#define FRAME_E		123
#define FRAME_SE	124
#define FRAME_S		125
#define FRAME_SW	126
#define FRAME_W		127
#define FRAME_MID	128

void load_font_frame() {
	
	printf("Loading font...");
	load_bmp_big("assets/f_blue/nw.bmp", FRAME_NW);		//NW
	load_bmp_big("assets/f_blue/n.bmp", FRAME_N);		//N
	load_bmp_big("assets/f_blue/ne.bmp", FRAME_NE);		//NE
	load_bmp_big("assets/f_blue/e.bmp", FRAME_E);		//E
	load_bmp_big("assets/f_blue/se.bmp", FRAME_SE);		//SE
	load_bmp_big("assets/f_blue/s.bmp", FRAME_S);		//S
	load_bmp_big("assets/f_blue/sw.bmp", FRAME_SW);		//SW
	load_bmp_big("assets/f_blue/w.bmp", FRAME_W);		//W
	load_bmp_big("assets/f_blue/mid.bmp", FRAME_MID);	//W
	
	load_bmp_big("assets/font/32.bmp", 132); //Space
	
	load_bmp_big("assets/font/33.bmp", 133); //!
	load_bmp_big("assets/font/36.bmp", 136); //$
	load_bmp_big("assets/font/38.bmp", 138); //&
	load_bmp_big("assets/font/39.bmp", 139); //'
	load_bmp_big("assets/font/44.bmp", 144); //,
	load_bmp_big("assets/font/46.bmp", 146); //.
	
	load_bmp_big("assets/font/48.bmp", 148); //0
	load_bmp_big("assets/font/49.bmp", 149); //1
	load_bmp_big("assets/font/50.bmp", 150); //2
	load_bmp_big("assets/font/51.bmp", 151); //3
	load_bmp_big("assets/font/52.bmp", 152); //4
	load_bmp_big("assets/font/53.bmp", 153); //5
	load_bmp_big("assets/font/54.bmp", 154); //6
	load_bmp_big("assets/font/55.bmp", 155); //7
	load_bmp_big("assets/font/56.bmp", 156); //8
	load_bmp_big("assets/font/57.bmp", 157); //9
	
	load_bmp_big("assets/font/63.bmp", 163); //?
	
	load_bmp_big("assets/font/65.bmp", 165); //A
	load_bmp_big("assets/font/66.bmp", 166); //B
	load_bmp_big("assets/font/67.bmp", 167); //C
	load_bmp_big("assets/font/68.bmp", 168); //D
	load_bmp_big("assets/font/69.bmp", 169); //E
	load_bmp_big("assets/font/70.bmp", 170); //F
	load_bmp_big("assets/font/71.bmp", 171); //G
	load_bmp_big("assets/font/72.bmp", 172); //H
	load_bmp_big("assets/font/73.bmp", 173); //I
	load_bmp_big("assets/font/74.bmp", 174); //J
	load_bmp_big("assets/font/75.bmp", 175); //K
	load_bmp_big("assets/font/76.bmp", 176); //L
	load_bmp_big("assets/font/77.bmp", 177); //M
	load_bmp_big("assets/font/78.bmp", 178); //N
	load_bmp_big("assets/font/79.bmp", 179); //O
	load_bmp_big("assets/font/80.bmp", 180); //P
	load_bmp_big("assets/font/81.bmp", 181); //Q
	load_bmp_big("assets/font/82.bmp", 182); //R
	load_bmp_big("assets/font/83.bmp", 183); //S
	load_bmp_big("assets/font/84.bmp", 184); //T
	load_bmp_big("assets/font/85.bmp", 185); //U
	load_bmp_big("assets/font/86.bmp", 186); //V
	load_bmp_big("assets/font/87.bmp", 187); //W
	load_bmp_big("assets/font/88.bmp", 188); //X
	load_bmp_big("assets/font/89.bmp", 189); //Y
	load_bmp_big("assets/font/90.bmp", 190); //Z
	
	// load_bmp_big("assets/font/97.bmp", 197); //a
	// load_bmp_big("assets/font/98.bmp", 198); //b
	// load_bmp_big("assets/font/99.bmp", 199); //c
	// load_bmp_big("assets/font/100.bmp", 200); //d
	// load_bmp_big("assets/font/101.bmp", 201); //e
	// load_bmp_big("assets/font/102.bmp", 202); //f
	// load_bmp_big("assets/font/103.bmp", 203); //g
	// load_bmp_big("assets/font/104.bmp", 204); //h
	// load_bmp_big("assets/font/105.bmp", 205); //i
	// load_bmp_big("assets/font/106.bmp", 206); //k
	// load_bmp_big("assets/font/107.bmp", 207); //k
	// load_bmp_big("assets/font/108.bmp", 208); //l
	// load_bmp_big("assets/font/109.bmp", 209); //m
	// load_bmp_big("assets/font/110.bmp", 210); //n
	// load_bmp_big("assets/font/111.bmp", 211); //o
	// load_bmp_big("assets/font/112.bmp", 212); //p
	// load_bmp_big("assets/font/113.bmp", 213); //q
	// load_bmp_big("assets/font/114.bmp", 214); //r
	// load_bmp_big("assets/font/115.bmp", 215); //s
	// load_bmp_big("assets/font/116.bmp", 216); //t
	// load_bmp_big("assets/font/117.bmp", 217); //u
	// load_bmp_big("assets/font/118.bmp", 218); //v
	// load_bmp_big("assets/font/119.bmp", 219); //w
	// load_bmp_big("assets/font/120.bmp", 220); //x
	// load_bmp_big("assets/font/121.bmp", 221); //y
	// load_bmp_big("assets/font/122.bmp", 222); //z	
	
}

void raw_text(const char* text, uint16_t x, uint16_t y) {
	uint8_t i, l;
	uint16_t w;
	l = strlen(text);
	if (l > 30) return;
	
	w = (l * 8);
	
	vdp_bitmapDraw(FRAME_NW, x, y);
	vdp_bitmapDraw(FRAME_NE, x + w, y);
	
	for (i = 1; i < (w / 8); i++) {
		vdp_bitmapDraw(FRAME_N, x + (i * 8), y);
		vdp_bitmapDraw(FRAME_S, x + (i * 8), y + 8);
	}
	
	vdp_bitmapDraw(FRAME_SW, x, y + 8);
	vdp_bitmapDraw(FRAME_SE, x + w, y + 8);
	
	for (i = 0; i < l; i++)	vdp_bitmapDraw(toupper(text[i]) + 100, x + 4 + (8 * i), y + 4);
}

void raw_text_centre(const char* text, uint16_t y) {
	uint8_t i, l;
	uint16_t w, x;
	l = strlen(text);
	if (l > 30) return;
	
	w = (l * 8);
	x = (320 - w) / 2;
	
	vdp_bitmapDraw(FRAME_NW, x, y);
	vdp_bitmapDraw(FRAME_NE, x + w, y);
	
	for (i = 1; i < (w / 8); i++) {
		vdp_bitmapDraw(FRAME_N, x + (i * 8), y);
		vdp_bitmapDraw(FRAME_S, x + (i * 8), y + 8);
	}
	
	vdp_bitmapDraw(FRAME_SW, x, y + 8);
	vdp_bitmapDraw(FRAME_SE, x + w, y + 8);
	
	for (i = 0; i < l; i++)	vdp_bitmapDraw(toupper(text[i]) + 100, x + 4 + (8 * i), y + 4);
}

uint8_t solidity[20][15];

void movesprite_x(sprite_state *sprite, int16_t x) {
	
	uint8_t i = 0;
	switch (sprite->dir) {
	
		case 0: //Currently north
			if (x < 0) { //Turn west
			
				sprite->dir = 9;
				sprite->seq = 0;
				
			} else if (x > 0) { //Turn east
				
				sprite->dir = 3;
				sprite->seq = 0;
				
			}
			break;
		case 3: //Currently east
			if (x < 0) { //Turn west
			
				sprite->dir = 9;
				sprite->seq = 0;
				
			} else if (x > 0) { //Keep east
				
				if (sprite->seq == 2) sprite->seq = 0;
				else sprite->seq++;
				
			}		
			break;
		case 6: //Currently south
			if (x < 0) { //Turn west
			
				sprite->dir = 9;
				sprite->seq = 0;
				
			} else if (x > 0) { //Turn east
				
				sprite->dir = 3;
				sprite->seq = 0;
				
			}		
			break;
		case 9: //Currently west
			if (x < 0) { //Keep west
			
				if (sprite->seq == 2) sprite->seq = 0;
				else sprite->seq++;
				
			} else if (x > 0) { //Turn east
				
				sprite->dir = 3;
				sprite->seq = 0;			
				
			}		
			break;
			
		default:
			break;
	}
	
	//printf("[%u][%u][%c] ", (sprite->rect.x + x) / 16, (sprite->rect.y) / 16, solidity[(sprite->rect.x + x) / 16][(sprite->rect.y) / 16]);
	
	vdp_spriteSelect(sprite->id);
	vdp_spriteSetFrameSelected(sprite->dir + sprite->seq);

	if (sprite->dir == 9 && sprite->rect.x + x + (sprite->rect.width / 2) <= 0) return;
	else if (sprite->dir == 3 && sprite->rect.x + x + (sprite->rect.width / 2) >= 320) return;
	else if (solidity[(sprite->rect.x + (sprite->rect.width / 2) + x) / 16][(sprite->rect.y + (sprite->rect.height / 2)) / 16] == '0') {
		
		sprite->rect.x += x;
		vdp_spriteMoveBySelected(x, 0);
	} //else play_sample(8, 1, 120);
	
}

void movesprite_y(sprite_state *sprite, int16_t y) {
	
	switch (sprite->dir) {
	
		case 0: //Currently north
			if (y < 0) { //Keep north
			
				if (sprite->seq == 2) sprite->seq = 0;
				else sprite->seq++;
				
			} else if (y > 0) { //Turn south
				
				sprite->dir = 6;
				sprite->seq = 0;
				
			}
			break;
		case 3: //Currently east
			if (y < 0) { //Turn north
			
				sprite->dir = 0;
				sprite->seq = 0;
				
			} else if (y > 0) { //Turn south
				
				sprite->dir = 6;
				sprite->seq = 0;
				
			}		
			break;
		case 6: //Currently south
			if (y < 0) { //Turn north
			
				sprite->dir = 0;
				sprite->seq = 0;
				
			} else if (y > 0) { //Keep south
				
				if (sprite->seq == 2) sprite->seq = 0;
				else sprite->seq++;
				
			}		
			break;
		case 9: //Currently west
			if (y < 0) { //Turn north
			
				sprite->dir = 0;
				sprite->seq = 0;
				
			} else if (y > 0) { //Turn south
				
				sprite->dir = 6;
				sprite->seq = 0;			
				
			}		
			break;
			
		default:
			break;
	}
	
	vdp_spriteSelect(sprite->id);
	vdp_spriteSetFrameSelected(sprite->dir + sprite->seq);
	
	if (sprite->dir == 6 && sprite->rect.y + y + (sprite->rect.height / 2) >= 224) return;
	else if (solidity[(sprite->rect.x + (sprite->rect.width / 2)) / 16][(sprite->rect.y + (sprite->rect.height / 2) + y) / 16] == '0') {
		sprite->rect.y += y;
		vdp_spriteMoveBySelected(0, y);
	} else {
	
		sprite->rect.y += (y * -1);
		vdp_spriteMoveBySelected(0, (y * -1));
		//play_sample(4, 1, 40);
		
	}
	//else play_sample(8, 1, 120);
	//printf("[%u][%u][%c] ", (sprite->rect.x) / 16, (sprite->rect.y + y) / 16, solidity[(sprite->rect.x) / 16][(sprite->rect.y + y) / 16]);
	
}

uint8_t my_atoi(const char *str) {
    uint8_t result = 0;

    while (isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result;
}

#define COLS 20
#define ROWS 14

void print2d() {
    uint8_t i, j;
	for(i=0; i < 14; i++) {
        for(j=0; j < 20; j++) {
            printf("%d ", solidity[j][i]);
        }
        printf("\r\n");
    }
}

void collision_map(const char* col) {
	
	
	//FIL *fo;
	uint8_t file = mos_fopen(col, fa_read);
	uint8_t i = 0, j = 0;
	if (!file) printf("Error reading colission map\r\n");
	
	//fo = (FIL *)mos_getfil(file);
	//file_size = fo->obj.objsize;
	//n = 0; x = 0, y = 0;
	
    for (i = 0; i < ROWS; i++) {
        for (j = 0; j < COLS; j++) {
            char c = mos_fgetc(file);
            while (!isdigit(c) && !isalpha(c)) {  // Skip non-digit/non-letter characters
                c = mos_fgetc(file);
            }
            solidity[j][i] = c;
        }
    }	
			
	mos_fclose(file);
	//free(file_buffer);	
	
    return;
}

uint8_t bg_grid[ROWS][COLS];
uint8_t fg_grid[ROWS][COLS];

void read_background(const char *filename) {
    uint8_t row = 0, col = 0;
    char c;
	uint8_t val;
	//uint16_t grid[ROWS][COLS];
	uint8_t fp = mos_fopen(filename, fa_read);
	if (!fp) printf("Error reading background\r\n");
	
	memset(bg_grid, 0, sizeof bg_grid);

    while (!mos_feof(fp) && row < ROWS) {
        c = mos_fgetc(fp);
		if (c >= '0' && c <= '9') {
            // we've read in a digit
            bg_grid[row][col] *= 10;
            bg_grid[row][col] += c - '0';
        } else if (c == '\r') continue;
		else if (c == ';') continue;
		else if (isspace(c) || c == EOF) {
            // we've encountered whitespace or a newline
            //printf("%03d ", grid[row][col]);
			vdp_bitmapDraw(bg_grid[row][col], col * 16, row * 16);
			
			col++;
            if (col == 20) {
                //printf("\r\n");
				row++;
                col = 0;
            }
        }
    }
	//printf("%u ", grid[row][col]);
	//vdp_bitmapDraw(bg_grid[row][col], col * 16, row * 16);
	mos_fclose(fp);

}

uint8_t redraw_start_row = 0, redraw_start_col = 0, redraw_end_row = 0, redraw_end_col = 0;
uint8_t redraw_current_row = 0,	redraw_current_col = 0;
bool redraw_pending = false;

void redraw_prep(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {

	redraw_pending		= true;
	redraw_start_row	= (y / 16);
	redraw_current_row	= (y / 16);
	redraw_start_col	= (x / 16);
	redraw_current_col	= (x / 16);
	redraw_end_row		= ((y + h) / 16) + 1;
	redraw_end_col		= ((x + w) / 16) + 1;
	
}

BOOL box_open = false;

void redraw_area() {
	
    // if (redraw_pending == true) {
	// 
		// vdp_bitmapDraw(bg_grid[redraw_current_row][redraw_current_col], redraw_current_col++ * 16, redraw_current_row * 16);
		// if (redraw_current_col == redraw_end_col) {
			// if (redraw_current_row == redraw_end_row) { redraw_pending = false; return; }
			// else { redraw_current_col = redraw_start_col; redraw_current_row++; }
		// }

	// }
	
	if (redraw_pending == true) {
        vdp_bitmapDraw(bg_grid[redraw_current_row][redraw_current_col], redraw_current_col * 16, redraw_current_row * 16);
		if (fg_grid[redraw_current_row][redraw_current_col] != 0) vdp_bitmapDraw(fg_grid[redraw_current_row][redraw_current_col], redraw_current_col * 16, redraw_current_row * 16);
        redraw_current_col++;
        if (redraw_current_col > redraw_end_col) {
            redraw_current_col = redraw_start_col;
            redraw_current_row++;
            if (redraw_current_row > redraw_end_row) {
                redraw_pending = false;
				if (box_open) box_open = false;
            }
        }
    }
	
}

void read_foreground(const char *filename) {
    uint8_t row = 0, col = 0;
    char c;
	uint8_t val;
	//uint16_t grid[ROWS][COLS];
	uint8_t fp = mos_fopen(filename, fa_read);
	if (!fp) printf("Error reading foreground\r\n");
	
	memset(fg_grid, 0, sizeof fg_grid);

    while (!mos_feof(fp) && row < ROWS) {
        c = mos_fgetc(fp);
		if (c >= '0' && c <= '9') {
            // we've read in a digit
            fg_grid[row][col] *= 10;
            fg_grid[row][col] += c - '0';
        } else if (c == '\r') continue;
		else if (isspace(c) || c == EOF) {
            // we've encountered whitespace or a newline
            //printf("%03d ", grid[row][col]);
			if (fg_grid[row][col] != 0) vdp_bitmapDraw(fg_grid[row][col], col * 16, row * 16);
			
			col++;
            if (col == 20) {
                //printf("\r\n");
				row++;
                col = 0;
            }
        }
    }
	//printf("%u ", grid[row][col]);
	//if (fg_grid[row][col] != 0) vdp_bitmapDraw(fg_grid[row][col], col * 16, row * 16);
	mos_fclose(fp);
}

void process_assets(const char *asset_file) {
    
    char buffer[128];
    uint8_t section = 0;
    uint8_t index = 0;
    uint8_t key_index = 0;
	uint8_t asset_count = 0;
    char c = 0;
    uint8_t colon_found = 0;
    uint8_t fp = mos_fopen(asset_file, fa_read);
	if (!fp) printf("Error reading manifest %s\r\n", asset_file);

    c = mos_fgetc(fp);
	while (!mos_feof(fp)) {
        
		if (c == '\r') {
            // Ignore carriage returns
        }
        else if (c == '\n') {
            buffer[index] = '\0';
            // Ignore blank lines and lines starting with semicolon
            if (index > 0 && buffer[0] != ';') {
                if (strcmp(buffer, "[Player]") == 0) {
					section = 0;
					//vdp_cls();
					//printf("Loading player sprite");
					raw_text_centre("Loading player sprite", 10);
					vdp_spriteSelect(0);
				}
                else if (strcmp(buffer, "[Background]") == 0) {
					section = 1;
					//vdp_cls();
					//printf("\rLoading background");
					raw_text_centre("Loading background", 30);
				}
                else if (strcmp(buffer, "[Foreground]") == 0) {
					section = 2;
					//vdp_cls();
					raw_text_centre("Loading foreground", 50);
					//printf("\rLoading foreground");
				}
				else if (strcmp(buffer, "[End]") == 0);
                else {
					//printf("%s in Section %d!\r\n", buffer, section);
					if (section == 0) {
						//printf(".");
						load_bmp_big(buffer, asset_count);
						vdp_spriteAddFrame(0, asset_count++);
					}
					else {
						//printf("Loading %s as asset %u\r\n", buffer, asset_count);
						//printf(".");
						load_bmp_big(buffer, asset_count++);					
					}
				}
            }
            index = 0;
        }
        else if (isprint(c)) {
            buffer[index] = c;
            index++;
        }

        c = mos_fgetc(fp);
    }

	c = mos_fgetc(fp);
	if (c == '\r') {
		// Ignore carriage returns
	}
	else if (c == '\n') {
		buffer[index] = '\0';
		// Ignore blank lines and lines starting with semicolon
		if (index > 0 && buffer[0] != ';') {
			if (strcmp(buffer, "[Player]") == 0) section = 0;
			else if (strcmp(buffer, "[Background]") == 0) section = 1;
			else if (strcmp(buffer, "[Foreground]") == 0) section = 2;
			else if (strcmp(buffer, "[End]") == 0);
			else printf("%s in Section %d!\r\n", buffer, section);
		}
		index = 0;
	}
	else if (isprint(c)) {
		buffer[index] = c;
		index++;
	}
    mos_fclose(fp);
}

//uint8_t fbuffer[320][240];

void setup_text_sprites() {
	
	vdp_spriteAddFrame(1, 148);			//0
	vdp_spriteAddFrameSelected(149);	//1
	vdp_spriteAddFrameSelected(150);	//2
	vdp_spriteAddFrameSelected(151);	//3
	vdp_spriteAddFrameSelected(152);	//4
	vdp_spriteAddFrameSelected(153);	//5
	vdp_spriteAddFrameSelected(154);	//6
	vdp_spriteAddFrameSelected(155);	//7
	vdp_spriteAddFrameSelected(156);	//8
	vdp_spriteAddFrameSelected(157);	//9

	vdp_spriteAddFrame(2, 148);			//0
	vdp_spriteAddFrameSelected(149);	//1
	vdp_spriteAddFrameSelected(150);	//2
	vdp_spriteAddFrameSelected(151);	//3
	vdp_spriteAddFrameSelected(152);	//4
	vdp_spriteAddFrameSelected(153);	//5
	vdp_spriteAddFrameSelected(154);	//6
	vdp_spriteAddFrameSelected(155);	//7
	vdp_spriteAddFrameSelected(156);	//8
	vdp_spriteAddFrameSelected(157);	//9
	
}

void draw_bottom_frame() {
	
	uint8_t i;
	vdp_bitmapDraw(FRAME_NW, 0, 224);
	vdp_bitmapDraw(FRAME_NE, 312, 224);
	vdp_bitmapDraw(FRAME_SW, 0, 232);
	vdp_bitmapDraw(FRAME_SE, 312, 232);
	
	for (i = 1; i < 39; i++) {
		
		vdp_bitmapDraw(FRAME_N, 8 * i, 224);
		vdp_bitmapDraw(FRAME_S, 8 * i, 232);
		
	}

}

BOOL dialogue_pending = false;
uint8_t dialogue_current_row = 0;
uint8_t dialogue_total_rows = 0;
uint16_t dialogue_x;
uint8_t dialogue_y;
uint16_t dialogue_w;
uint8_t dialogue_h;

void setup_text_frame(uint16_t x, uint8_t y, uint16_t w, uint8_t h) {
	
	dialogue_x = (x + 7) & ~7; //Round up to next multiple of 8
	dialogue_y = (y + 7) & ~7;
	dialogue_w = (w + 7) & ~7;
	dialogue_h = (h + 7) & ~7;
	dialogue_pending = true;
	dialogue_total_rows = (dialogue_h / 8) - 1;
}

void redraw_box() {

	redraw_pending		= true;
	redraw_start_row	= (dialogue_y / 16);
	redraw_current_row	= (dialogue_y / 16);
	redraw_start_col	= (dialogue_x / 16);
	redraw_current_col	= (dialogue_x / 16);
	redraw_end_row		= ((dialogue_y + dialogue_h) / 16) + 1;
	redraw_end_col		= ((dialogue_x + dialogue_w) / 16) + 1;
	
}

bool draw_text_frame() {
	
	if (dialogue_pending == true) {
		
		uint8_t i;

		if (dialogue_current_row == 0) {
			vdp_bitmapDraw(FRAME_NW, dialogue_x, dialogue_y);
			vdp_bitmapDraw(FRAME_NE, dialogue_x + dialogue_w - 8, dialogue_y);
			for (i = 1; i < (dialogue_w / 8) - 1; i++) {
				vdp_bitmapDraw(FRAME_N, dialogue_x + (i * 8), dialogue_y);
			}
		} 
		else if (dialogue_current_row == (dialogue_h / 8) - 1) {
			vdp_bitmapDraw(FRAME_SW, dialogue_x, dialogue_y + dialogue_h - 8);
			vdp_bitmapDraw(FRAME_SE, dialogue_x + dialogue_w - 8, dialogue_y + dialogue_h - 8);
			for (i = 1; i < (dialogue_w / 8) - 1; i++) {
				vdp_bitmapDraw(FRAME_S, dialogue_x + (i * 8), dialogue_y + dialogue_h - 8);
			}
		} 
		else {
			vdp_bitmapDraw(FRAME_W, dialogue_x, dialogue_y + (dialogue_current_row * 8));
			vdp_bitmapDraw(FRAME_E, dialogue_x + dialogue_w - 8, dialogue_y + (dialogue_current_row * 8));
			for (i = 1; i < (dialogue_w / 8) - 1; i++) {
				vdp_bitmapDraw(FRAME_MID, dialogue_x + (i * 8), dialogue_y + (dialogue_current_row * 8));
			}
		}
		
		if (dialogue_current_row == dialogue_total_rows) {
			
			dialogue_pending = false;
			dialogue_current_row = 0;
			dialogue_total_rows = 0;
			
			return false;
			
		} else dialogue_current_row++;
		return true;
	}
	else return false;
}



uint8_t count(const char *text, char test) {
    uint8_t count = 0, k = 0;
    while (text[k] != '\0')
    {
          if (text[k] == test)
              count++;
          k++;
    }
    return count;
}

#include <string.h>

uint8_t longest_stretch(char *str) {
    uint8_t len = 0, max_len = 0;
    char *p = str;

    while (*p != '\0') {
        if (*p == '\n') {
            if (len > max_len) {
                max_len = len;
            }
            len = 0;
        } else {
            len++;
        }
        p++;
    }

    if (len > max_len) {
        max_len = len;
    }

    return max_len;
}


bool 		text_pending = false;
uint8_t 	text_length = 0;
uint8_t		text_max_length = 0;
char		*text_string = NULL;
uint8_t		text_pos = 0;
uint16_t	text_pos_x = 0;
uint16_t	text_pos_y = 0;
uint16_t	text_x = 0;
uint16_t	text_y = 0;
uint8_t		text_rows = 0;
uint24_t	last_print = 0;

void print_text(const char *text, uint8_t x, uint8_t y) {

	uint8_t i;
	
	if (text_string != NULL) free(text_string);
	
	text_length = strlen(text);
	text_string = (char*)malloc(text_length + 1);
	strncpy(text_string, text, text_length);
	text_string[text_length] = '\0';
	text_max_length = longest_stretch(text_string);
	text_pending = true;
	text_x = x;
	text_y = y;
	text_rows = 1 + count(text_string, '\n');

}

void text_box(const char *text, uint16_t x, uint16_t y) {
	
	print_text(text, x + 6, y + 6);
	setup_text_frame(x, y, (text_max_length * 8) + 2, (text_rows * 10) + 2);
	box_open = true;
	
}

void text_box_centre(const char *text, uint16_t y) {
	
	uint16_t x = (320 - (longest_stretch(text) * 8)) / 2;
	print_text(text, x + 6, y + 6);
	setup_text_frame(x, y, (text_max_length * 8) + 2, (text_rows * 10) + 2);
	box_open = true;
	
}

void text_box_centre_away(const char *text) {
	
	uint16_t x = (320 - (longest_stretch(text) * 8)) / 2;
	uint8_t y = 24;
	if (player.rect.y > 120) y = 24;
	else if (player.rect.y <= 120) y = 160;
	print_text(text, x + 8, y + 4);
	setup_text_frame(x, y, (text_max_length * 8) + 1, (text_rows * 10) + 2);
	box_open = true;
	
}

void service_text(uint8_t delay) {

	if (text_pending == true && (timer0 - last_print) > delay) {
		
		//vdp_bitmapDraw(toupper(text_string[text_pos]) + 100, text_x + (8 * text_pos++), text_y);
		if (text_string[text_pos] == '\n') {
			text_pos_y += 10;
			text_pos_x = 0;
			text_pos++;
			return;
		}
		
		vdp_bitmapDraw(toupper(text_string[text_pos]) + 100, text_x + (8 * text_pos_x), text_y + text_pos_y);
		text_pos++;
		text_pos_x++;
		
		if (text_pos == text_length) {
			text_pending = false;
			text_pos = 0;
			//text_x = 0;
			text_pos_x = 0;
			text_pos_y = 0;
			//text_y = 0;
			free(text_string);
			text_string = NULL;
		} else last_print = timer0;
		
	}
	
}

uint8_t player_score = 0;

void show_score() {
	
	vdp_spriteSetFrame(1, player_score / 10);
	vdp_spriteSetFrame(2, player_score % 10); 
	
}

void change_score(int8_t change) {
	
	if (player_score + change > 99) player_score = 99;
	else if (player_score + change < 0) player_score = 0;
	else player_score += change;
	vdp_spriteSetFrame(1, player_score / 10);
	vdp_spriteSetFrame(2, player_score % 10); 
	
}

char transition_manifest[100];
char transition_bg[32];
char transition_fg[32]; 
char transition_col[32];
char transition_bgm[32];
uint16_t transition_x = 0;
uint16_t transition_y = 0;
uint8_t transition_dir = 0;
bool transition_due = false;

void load_room(const char* manifest, const char* bg, const char* fg, const char * col, const char * bgm, uint16_t player_start_x, uint8_t player_start_y, uint8_t player_dir) {
	
	if (vgm_file!= NULL) vgm_cleanup(vgm_file);
	if (transition_due) transition_due = false;
	
	vdp_spriteHide(0);
	vdp_spriteHide(1);
	vdp_spriteHide(2);
	vdp_cls();
	vdp_clearGraphics();
	process_assets(manifest);
    read_background(bg);
	read_foreground(fg);
	
	vdp_bitmapCreateSolidColor(254, 16, 16, 0); //Solid black tile
	
	collision_map(col);
	
	load_wav("assets/sfx/point.wav", 0);
	load_wav("assets/sfx/thud.wav", 1);
	
	vgm_file = mos_fopen(bgm, fa_read);
	if (!vgm_file) printf("Error reading VGM\r\n");
	vgm_init(vgm_file);
	vgm_info.volume_multiplier = 0.2;
	
	draw_bottom_frame();
	
	vdp_bitmapDraw(100 + '$', 8, 229);
	vdp_spriteMoveTo(1, 16, 229);
	//vdp_spriteSetFrameSelected(0);
	vdp_spriteShowSelected();
	vdp_spriteMoveTo(2, 24, 229);
	//vdp_spriteSetFrameSelected(0);
	vdp_spriteShowSelected();
	vdp_spriteSetFrame(1, player_score / 10);
	vdp_spriteSetFrame(2, player_score % 10); 
	
	player.rect.x = player_start_x;
	player.rect.y = player_start_y;
	player.dir = player_dir;
	
	vdp_spriteMoveTo(0, player.rect.x, player.rect.y);
	vdp_spriteSetFrameSelected(player.dir);
	
	vdp_spriteActivateTotal(3);
	vdp_spriteShow(0);
	vdp_spriteShow(1);
	vdp_spriteShow(2);
	
	
}

void do_action() {
	
	//printf(" (%u %u:%u=%u)[%u:%u] ",player.dir, (player.rect.x - (player.rect.width / 2)) / 16, (player.rect.y) / 16, solidity[(player.rect.x) / 16][(player.rect.y) / 16], player.rect.x, player.rect.y);
	switch (player.dir) { //0=North, 3=East, 6=South, 9=West
		
		case 0:
			if (solidity[((player.rect.x + (player.rect.width / 2)) / 16)][(player.rect.y) / 16] == 'C') { //The +1 is to offset the fact that we're comparing the corner of a rect with a grid.
				text_box_centre_away("You found 1 gold coin!");
				change_score(1);
				play_sample(3, 0, 127); //Play coin noise
				solidity[((player.rect.x + (player.rect.width / 2)) / 16)][(player.rect.y) / 16] = 'X';
			} else if (solidity[((player.rect.x + (player.rect.width / 2)) / 16)][(player.rect.y) / 16] == 'D') { //Door 
		
				if (player_score == 0) text_box_centre_away("Go away, no visitors!");
				else if (player_score > 0) {
					text_box_centre_away("A gold coin? Alright...\nBut don't tell anybody!");
					change_score(-1);
					play_sample(3, 0, 127);
					//printf("manifest now: %s\r\n", transition_manifest);
					strcpy(transition_manifest, "assets/castle_manifest.txt");
					//printf("manifest now: %s\r\n", transition_manifest);
					strcpy(transition_bg, "assets/castle_bg.txt");
					strcpy(transition_fg, "assets/castle_fg.txt");
					strcpy(transition_col, "assets/castle_col.txt");
					strcpy(transition_bgm, "start.vgm");
					transition_x = 30;
					transition_y = 200;
					transition_dir = 0;
					transition_due = true;
				}

			} else if (solidity[((player.rect.x + (player.rect.width / 2)) / 16)][(player.rect.y) / 16] == 'T') { //Magic stuff

			text_box_centre_away("And this...\nis the limit of my creativity!");
			
			}				
			
		
	}
	
}

int main(int argc, char * argv[]) {
	
	int i = 0, j = 0;
	UINT24 t = 0;
	uint8_t key = 0, keycount;
	FIL *fo;
	uint8_t *tile_map;
	
	player.id = 0;
	player.dir = 6; //Face south
	player.seq = 0; //Starting pose
	player.rect.x = 160; //Middle X
	player.rect.y = 120; //Middle Y
	player.rect.height = 24;
	player.rect.width = 24;
	
	//vgm_file = mos_fopen("town.vgm", fa_read);
	//printf("%u %s %s %s", argc, argv[0], argv[1], argv[2]);
	
	//play_sample(8, 0, 120);
	
 	//if (argc < 2) return 0;
		
	//if (argc == 4 && (!strcmp(argv[2], "loop=true"))) vgm_info.loop_enabled = true;
	//else vgm_info.loop_enabled = false;
	//vgm_info.loop_enabled = true; //Force looping right now.
	

	//fo = (FIL *)mos_getfil(vgm_file);
	
	//vdp_clearGraphics();
	vdp_mode(2);
	vdp_cursorDisable();
	vdp_cls();
	load_font_frame();
	setup_text_sprites();
	
	//vdp_clearGraphics();
	
	// process_assets("assets/manifest.txt");
    // read_background("assets/bg2.txt");
	// read_foreground("assets/fg2.txt");
	// collision_map("assets/pass.txt");
	// draw_bottom_frame();
	load_room("assets/garden_manifest.txt","assets/garden_bg.txt","assets/garden_fg.txt","assets/garden_col.txt", "outside.vgm", 160, 80, 6);
	
	keycount = getsysvar_vkeycount();
	while (true) {
 																//Main loop	
		if (!vgm_info.pause) if (parse_vgm_file(vgm_file) == 1) break;								//Progress BGM
		if (!draw_text_frame()) service_text(500);
		redraw_area();
		
		if (getsysvar_vkeycount() != keycount && getsysvar_vkeydown() == 1) { 	//Handle keypresses
			key = getsysvar_keyascii();
			
			if (key == 'q') break;
			
			if(!box_open) {
				if (key == 'w') movesprite_y(&player, -4);
				if (key == 'a') movesprite_x(&player, -4);
				if (key == 's') movesprite_y(&player, 4);
				if (key == 'd') movesprite_x(&player, 4);
			}
			
			if (box_open && key == ' ') redraw_box();
			else if (key == ' ') do_action();
			
			if (key == 'u') print2d();
			
			if (key == '1') change_score(-5);
			if (key == '2') change_score(5);
			
			if (key == 'n') play_sample(8, 1, 120);
			if (key == 'i') {

				load_bmp_big("doom_24.bmp", 100);
				vdp_bitmapDraw(100, 0, 0);					
				
			}	

			if (key == 'j') {

				load_bmp_small("doom_24_120.bmp", 100);
				vdp_bitmapDraw(100, 0, 0);					
				
			}				
			
			if (key == 'l' && vgm_info.volume_multiplier <= 0.9) vgm_info.volume_multiplier += 0.1;
			if (key == 'k' && vgm_info.volume_multiplier >= 0.1) vgm_info.volume_multiplier -= 0.1;
			if (key == 'm') vgm_info.pause = !vgm_info.pause;
			
			if (key == 'p') text_box_centre_away("Hello world!");
			if (key == '[') text_box("Hello\nworld!", 30, 30);
			if (key == ']') load_room("assets/castle_manifest.txt","assets/castle_bg.txt","assets/castle_fg.txt","assets/castle_col.txt", "start.vgm", 160, 200, 6);
			//if (key == 'o') redraw_prep(text_x, text_y, text_length * 8, text_rows * 10); 
			if (key == 'o') redraw_box();
				//redraw_prep(30, 30, 110, 16); 
			 
			if (key == 'x') printf("[%u][%u] ", player.rect.x / 16, player.rect.y / 16);
			
			keycount = getsysvar_vkeycount();
		}
		
		if (timer0 % 735 == 0) vdp_spriteRefresh(); //735 44.1KHz ticks = ~60Hz
		if (transition_due && !box_open) load_room(transition_manifest, transition_bg, transition_fg, transition_col, transition_bgm, transition_x, transition_y, transition_dir);
	}
	
	vgm_cleanup(vgm_file); 
	
	return 0;
}

