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

/* void handle_time() {
	
	waitvblank();
	miniticks++;
	if (miniticks == 60) {
		miniticks = 0;
		ticks++;
	}
	
} */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdint.h>

void delay_ticks(UINT16 ticks_end) { //16.7ms ticks
	
	UINT32 ticks = 0;
	ticks_end *= 6;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

void delay_cents(UINT16 ticks_end) { //100ms ticks
	
	UINT32 ticks = 0;
	ticks_end *= 6;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

void delay_secs(UINT16 ticks_end) { //1 sec ticks
	
	UINT32 ticks = 0;
	ticks_end *= 60;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

uint8_t fseek(UINT8 file, uint24_t offset, uint8_t whence) {

	uint24_t i;
	
	for (i = 0; i < offset; i++) {
	
		(void)mos_fgetc(file);
		if (mos_feof(file)) return 1;
		
    }
		
	return 0;
	
}

int fread(byte *buffer, uint32_t num_bytes, UINT8 file) {
    int byte_value, i;
	for (i = 0; i < num_bytes; i++) {
        byte_value = mos_fgetc(file);
        if (mos_feof(file)) {
            //printf("Error: end of file reached unexpectedly\r\n");
            return -1;
        }
        buffer[i] = (byte) byte_value;
    }
	return num_bytes;
}

int load_bmp(const char* filename, UINT8 slot) {

    int32_t width, height, bit_depth, row_padding, y, x, i, n;
    uint8_t header[54], pixel[4], color_table[1024], file, r, g, b, index;
	uint32_t pixel_value, color_table_size;
	uint32_t biSize;	
	
    file = mos_fopen(filename, fa_read);
    if (!file) {
        printf("Error: could not open file.\n");
        return 0;
    }
	
	fread(header, 54, file);

    biSize = *(UINT32*)&header[14];
	width = *(INT32*)&header[18];
    height = *(INT32*)&header[22];
    bit_depth = *(UINT16*)&header[28];
	color_table_size = *(UINT32*)&header[46];
	
	if (color_table_size == 0 && bit_depth == 8) {
        color_table_size = 256;
    }
	
	if (color_table_size > 0) fread(color_table, color_table_size * 4, file);
	
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

int load_bmp_crop(const char* filename, UINT8 slot, UINT16 start_x, UINT16 start_y, UINT16 crop_width, UINT16 crop_height) {

    int32_t width, height, bit_depth, row_padding, y, x, i, n;
    uint8_t header[54], pixel[4], color_table[1024], file, r, g, b, v, index;
	uint32_t pixel_value, color_table_size;
	uint32_t biSize;
	uint32_t pix_limit, pix_count = 0;
	
    file = mos_fopen(filename, fa_read);
    if (!file) {
        printf("Error: could not open file.\n");
        return 0;
    }
	
	fread(header, 54, file);

    biSize = *(UINT32*)&header[14];
	width = *(INT32*)&header[18];
    height = *(INT32*)&header[22];
    bit_depth = *(UINT16*)&header[28];
	color_table_size = *(UINT32*)&header[46];
	
	if (color_table_size == 0 && bit_depth == 8) {
        color_table_size = 256;
    }
	
	if (color_table_size > 0) fread(color_table, color_table_size * 4, file);
	
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
	
	write16bit(crop_width);
	write16bit(crop_height);
	pix_limit = crop_width * crop_height;
	
	if (bit_depth == 32) putch(1);
	if (bit_depth == 24 || bit_depth == 8) putch(0);
	
	if (bit_depth == 8) {
		
        //for (y = height - 1; y >= 0; y--) {
		for (y = height - 1 - start_y; y >= height - start_y - crop_height; y--) {
		
            //for (x = 0; x < width; x++) {
			for (x = 0; x < width; x++) {
				
				index = (UINT8)mos_fgetc(file);
				if (x >= start_x && x < start_x + crop_width && y >= start_y && y < start_y + crop_height) {
				
					b = color_table[index * 4];
					g = color_table[index * 4 + 1];
					r = color_table[index * 4 + 2];
					putch(b);
					putch(g);
					putch(r);
					if (++pix_count == pix_limit) return pix_count;
				
				}				

            }

            for (i = 0; i < row_padding; i++) {
                mos_fgetc(file);
            }
			
			// if (y >= height - start_y - crop_height && y < height - start_y) {
				// output_rows++;
				// if (output_rows == crop_height) {
					// break;
				// }
			// }
			
        }
		
	}
	
    else if (bit_depth == 32 || bit_depth == 24) {
        for (y = height - 1; y >= 0; y--) {
		
            for (x = 0; x < width; x++) {
					
				for (i = 0; i < bit_depth / 8; i++) {
						
					pixel[i] = mos_fgetc(file);//v = mos_fgetc(file);
					
					if (x >= start_x && x < start_x + crop_width && y >= start_y && y < start_y + crop_height) {
						
						for (i = 0; i < bit_depth / 8; i++) {
							putch(pixel[i]);
						}
						if (++pix_count == pix_limit) return pix_count;
						
					}
				}

            }

            for (i = 0; i < row_padding; i++) {
                mos_fgetc(file);
            }
			
			// if (y >= height - start_y - crop_height && y < height - start_y) {
				// output_rows++;
				// if (output_rows == crop_height) {
					// break; // Break out of the outer loop when all necessary rows have been outputted
				// }
			// }			
			
        }
		//printf("Width: %u, Height: %u, BPP: %u", width, height, bit_depth);    
	}

    mos_fclose(file);
	return crop_width * crop_height;
	
	
}

bool isKthBitSet(int n, int k)
{
    if (n & (1 << k)) return true;
    else return false;
}

void channel_ready(int channel) {

	waitvblank();
	while (isKthBitSet(getsysvar_audioSuccess(), channel)) continue;
		//if () break;
	
	
}

void change_volume(UINT8 channel, UINT8 volume) {
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(100); //Non envelope but custom waveform
	putch(volume);
	
}

void change_frequency(UINT8 channel, UINT16 frequency) {
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(101); //Non envelope but custom waveform
	write16bit(frequency);
	
}

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
	
	channel_ready(channel);
	
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

void load_sample(const char* filename, UINT8 sample_id) {
	
	int i;
	UINT8 fp;
	uint24_t c_count = 0;
	UINT8 c;
	
	
	// if (sample_id > 11) {
	// 
		// printf("Invalid sample slot\r\n");
		// return;
	// 
	// }
		
	fp = mos_fopen(filename, fa_read);
	if(fp) {
		printf("\r\nOpening %s for upload into slot %u\r\n", filename, sample_id);
		
		putch(23);
		putch(0);
		putch(133);
		putch(0);
		putch(5); //Sample mode

		putch(sample_id); //Sample ID
		putch(0); //Record mode
		
		while(!mos_feof(fp)) {
			
			c = mos_fgetc(fp);
				
			putch(c); //First six will be a basic header - 4 bytes for data length and 2 bytes for sample rate.
			
			c_count++;
			
		}
	
		printf("\r\nUploaded %u bytes.\r\n", c_count);
		mos_fclose(fp);
		
		
	} else printf("Failed to open %s\r\n", filename);
	
}

// void load_wav(const char* filename, uint8_t sample_id) {
    
// 	int i, header_size, bit_rate;
// 	int8_t sample8;
// 	int16_t sample16;
// 	uint32_t data_size, sample_rate;
//     unsigned char header[300];
//     UINT8 fp;
	
//     fp = mos_fopen(filename, fa_read);
//     if (!fp) {
//         printf("Error: could not open file %s\n", filename);
//         return;
//     }

//     for (i = 0; i < 300 && !mos_feof(fp); i++) {
//         header[i] = mos_fgetc(fp);
//         if (i >= 3 && header[i - 7] == 'd' && header[i - 6] == 'a' && header[i - 5] == 't' && header[i - 4] == 'a') {
// 			break; // found the start of the data
//         }
//     }

//     if (i >= 300 || !(header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F') ||
//         !(header[8] == 'W' && header[9] == 'A' && header[10] == 'V' && header[11] == 'E' &&
//           header[12] == 'f' && header[13] == 'm' && header[14] == 't' && header[15] == ' ') ||
//         !(header[16] == 0x10 && header[17] == 0x00 && header[18] == 0x00 && header[19] == 0x00 &&
//           header[20] == 0x01 && header[21] == 0x00 && header[22] == 0x01 && header[23] == 0x00)) {
//         printf("Error: invalid WAV file\n");
//         mos_fclose(fp);
//         return;
//     }
	
//     sample_rate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
// 	data_size = header[i - 3] | (header[i - 2] << 8) | (header[i - 1] << 16) | (header[i] << 24);

//     if (header[34] == 8 && header[35] == 0) {
//         bit_rate = 8;
//     } else {
//         printf("Error: unsupported PCM format\n");
//         mos_fclose(fp);
//         return;
//     }
	
// 		putch(23);
// 		putch(0);
// 		putch(133);
// 		putch(0);
// 		putch(5); //Sample mode

// 		putch(sample_id); //Sample ID
// 		putch(0); //Record mode
		
// 		if (bit_rate == 8) write32bit(data_size);
// 		else if (bit_rate == 16) write32bit(data_size / 2);
		
// 		write16bit(sample_rate);
	
//     for (i = 0; i < data_size && !mos_feof(fp); i++) {
        
// 		if (bit_rate == 8) {
			
// 			sample8 = mos_fgetc(fp) - 128;
// 			putch(sample8);
			
// 		}
//     }

//     mos_fclose(fp);
// }

void load_wav(const char* filename, uint8_t sample_id) {
    
	int i, header_size, bit_rate;
	int8_t sample8;
	int16_t sample16;
	uint32_t data_size, sample_rate;
    unsigned char header[300];
    UINT8 fp;
	
    fp = mos_fopen(filename, fa_read);
    if (!fp) {
        printf("Error: could not open file %s\n", filename);
        return;
    }

    // while (1) {
    //     char chunk_id[4];
    //     uint32_t chunk_size;
    //     fread(chunk_id, 4, fp);
    //     fread(&chunk_size, 4, fp);
    //     if (strncmp(chunk_id, "data", 4) == 0) {
    //         data_size = chunk_size;
    //         break;
    //     }
    //     fseek(fp, chunk_size, SEEK_CUR);
    // }

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
	
		putch(23);
		putch(0);
		putch(133);
		putch(0);
		putch(5); //Sample mode

		putch(sample_id); //Sample ID
		putch(0); //Record mode
		
		if (bit_rate == 8) write32bit(data_size);
		
		write16bit(sample_rate);

	remainder = data_size % 5000;
	uint8_t sample_buffer[5000];

	if (data_size < 5000) {

		mos_fread(sample_buffer, data_size, fp);
		mos_puts(sample_buffer, data_size, 0);

	}

	else {

		for (i = 0; i < data_size / 5000, i++;) {
			mos_fread(sample_buffer, 5000, fp);
			mos_puts(sample_buffer, 5000, 0);
		}

		if (remainder) {
			mos_fread(sample_buffer, remainder, fp);
			mos_puts(sample_buffer, remainder, 0);
		}
	}

	// for (i = 0; i < data_size && !mos_feof(fp); i++) {
        
	// 	if (bit_rate == 8) {
			
	// 		sample8 = mos_fgetc(fp) - 128;
	// 		putch(sample8);
			
	// 	}
    // }

    mos_fclose(fp);
}

#define AY_3_8910_CLOCK_SPEED 1789773
//#define AY_FREQ 55930
#define AY_3_8910_CHANNELS 3

typedef struct {
    uint8_t chip_type;
	uint32_t clock;
	uint8_t n_channels;
	uint24_t f_scale;
	uint24_t loop_start;
	uint32_t header_size;
	
	bool loop_enabled;
	
	uint32_t gd3_location;
	
	uint8_t data[60000];
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

uint8_t max(int a, int b) {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}

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
		
	else if (sn.latched_type == 1) change_volume(sn.latched_channel, sn_channels[sn.latched_channel].volume);	
	
}

void process_0xA0_command(unsigned char reg, unsigned char data) {
    int channel = 0;
	
	switch (reg) {
        case 0x00:
        case 0x01:
		case 0x08:
            channel = 0;
            break;
        case 0x02:
        case 0x03:
		case 0x09:
            channel = 1;
            break;
        case 0x04:
        case 0x05:
		case 0x0A:
            channel = 2;
            break;
		case 0x06:
			break;
		case 0x07:
			break;
        default:
            // Invalid register, ignore
            return;
    }
	
	if (reg == 7) {  //Mixer
		
        ay_states[0].enabled = isKthBitSet(data, 0) ? false : true;
		ay_states[1].enabled = isKthBitSet(data, 1) ? false : true;
		ay_states[2].enabled = isKthBitSet(data, 2) ? false : true;
		//printf("Mix toggle 0x%02X ", data);
		
		if (ay_states[0].enabled) change_volume(0, ay_states[channel].volume);
		else if (ay_states[0].enabled == false) change_volume(0, 0);
		if (ay_states[1].enabled) change_volume(0, ay_states[channel].volume);
		else if (ay_states[1].enabled == false) change_volume(0, 0);
		if (ay_states[2].enabled) change_volume(0, ay_states[channel].volume);
		else if (ay_states[2].enabled == false) change_volume(0, 0);
		
	}
	
	else if (reg == 00 || reg == 01 || reg == 02 || reg == 03 || reg == 04 || reg == 05) {
        if (reg == 1 || reg == 3 || reg == 5) {
            // Even-numbered register is frequency high byte
			ay_states[channel].frequency_coarse = data;
			ay_states[channel].frequency_hz = vgm_info.f_scale / (ay_states[channel].frequency_coarse << 8 | ay_states[channel].frequency_fine);
			if (ay_states[channel].enabled) change_frequency(channel, ay_states[channel].frequency_hz);
			
        } else {
            // Odd-numbered register is frequency low byte
            ay_states[channel].frequency_fine = data;
			ay_states[channel].frequency_hz = vgm_info.f_scale / (ay_states[channel].frequency_coarse << 8 | ay_states[channel].frequency_fine);
			if (ay_states[channel].enabled) change_frequency(channel, ay_states[channel].frequency_hz);
        }
 
	} else if (reg == 8 || reg == 9 || reg == 10) {
        // Volume register
		ay_states[channel].volume = (data & 0x0F) * 8;
		if (ay_states[channel].enabled) change_volume(channel, ay_states[channel].volume);
		else if (ay_states[channel].enabled == 0) change_volume(0, 0);
    }
}

UINT32 little_long(UINT8 b0, UINT8 b1, UINT8 b2, UINT8 b3) {

	UINT32 little_long = 0;
		
	little_long += (UINT32)(b0 << 24);
	little_long += (UINT32)(b1 << 16);
	little_long += (UINT32)(b2 << 8);
	little_long += (UINT32)b3;
		
	return little_long;
		
}

uint32_t bigtolittle32(const uint8_t *bytes) {
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

uint32_t littletobig32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

uint32_t l2b(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    return ((uint32_t)b0 << 24) |
           ((uint32_t)b1 << 16) |
           ((uint32_t)b2 << 8) |
           (uint32_t)b3;
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
	uint24_t data_length = 0;
	
	vgm_info.data_pointer = 0;
	vgm_info.loop_start = 0;
	vgm_info.gd3_location = 0;
	
	state = READ_COMMAND;
	
	// fread(&vgm_min_header, 0x40, fp);
	// 
	// if (vgm_min_header[0] != 'V' || vgm_min_header[1] != 'g' || vgm_min_header[2] != 'm') return 0;
	// 
	// if (*(uint32_t*)(vgm_min_header + 0x34) != 0) {
	// 
		// UINT32 header_size = littletobig32((const uint8_t *)&vgm_min_header[0x34]) - 0xC;// (less the 12 bytes between 0x34 and 0x40)
		//Ingest the correct *remaining* header (total header less thes 0x40 already read), if there is any (old VGM files start data at 0x40)
		// 
		// fread(&vgm_rem_header, header_size, fp);
		// vgm_info.header_size = header_size + 0x40;
		// printf("Remaining header length is 0x%04X, now stored in vgm_rem_header.\r\n", header_size);
		// 
	// }
	
	// data_length = littletobig32((const uint8_t *)&vgm_min_header[0x04]) - 0x04 - vgm_info.header_size - 0x40;
	// printf("Data length = 0x%04X\r\n", data_length);
	// fread(&vgm_info.data, data_length, fp);
	
	while(!mos_feof(fp)) {
			
		vgm_info.data[vgm_info.data_pointer++] = mos_fgetc(fp);
		if (vgm_info.data_pointer == 63) break;
			
	}
	
	if (vgm_info.data[0] != 'V' || vgm_info.data[1] != 'g' || vgm_info.data[2] != 'm') return 0;
	
	if (vgm_info.data[0x34] != 0) { //Is there more header than the classic 64 bytes?
		
		UINT32 header_size = littletobig32((const uint8_t *)&vgm_info.data[0x34]) - 0xC;// (less the 12 bytes between 0x34 and 0x40)
		while(!mos_feof(fp)) {
			
			vgm_info.data[vgm_info.data_pointer++] = mos_fgetc(fp);
			if (vgm_info.data_pointer == 63 + header_size) break; //Read the rest of the header.
			
		}			
		
	}
	
	//printf("Data pointer sits at %u bytes, reseting to 0\r\n", vgm_info.data_pointer);
	//vgm_info.data_pointer = 0;
	
	//if (*(uint32_t*)(vgm_min_header + 0x0C) != 0) { //SN76489?
	if (*(uint32_t*)(vgm_info.data + 0x0C) != 0) { //SN76489?

		vgm_info.chip_type = 1;
		//vgm_info.clock = little_long(vgm_header[0x0F], vgm_header[0x0E], vgm_header[0x0D], vgm_header[0x0C]);
		printf("Clock bytes: 0x%02X 0x%02X 0x%02X 0x%02X\r\n", vgm_info.data[0x0C], vgm_info.data[0x0D], vgm_info.data[0x0E], vgm_info.data[0x0F]);
		//vgm_info.clock = littletobig32((const uint8_t *)&vgm_info.data[0x0C]);
		vgm_info.clock = l2b(vgm_info.data[0x0F], vgm_info.data[0x0E], vgm_info.data[0x0D], vgm_info.data[0x0C]);
		vgm_info.n_channels = 3;
		
	}
	
	//if ((UINT32)vgm_header[0x74] != 0) { //AY-3-8910?
	//else if (*(uint32_t*)(vgm_rem_header + 0x34) != 0) { //AY-3-8910?
	else if (*(uint32_t*)(vgm_info.data + 0x74) != 0) { //AY-3-8910?

		vgm_info.chip_type = 0;
		//vgm_info.clock = little_long(vgm_header[0x77], vgm_header[0x76], vgm_header[0x75], vgm_header[0x74]);
		//vgm_info.clock = littletobig32((const uint8_t *)&vgm_rem_header[0x34]);
		//vgm_info.clock = littletobig32((const uint8_t *)&vgm_info.data[0x74]);
		vgm_info.clock = l2b(vgm_info.data[0x77], vgm_info.data[0x76], vgm_info.data[0x75], vgm_info.data[0x74]);
		vgm_info.n_channels = 3;
		
	}
	
	if (*(uint32_t*)(vgm_info.data + 0x1C) != 0) { //Loop detected
		
		//vgm_info.loop_start = littletobig32((const uint8_t *)vgm_info.data[0x1C]) + 0x1C;
		vgm_info.loop_start = l2b(vgm_info.data[0x1F], vgm_info.data[0x1E], vgm_info.data[0x1D], vgm_info.data[0x1C]) + 0x1C;
		
	} else vgm_info.loop_start = 0;
	
	if (vgm_info.loop_start) printf("Chip type %u, clock %uHz, loop starts at 0x%08X\r\n", vgm_info.chip_type, vgm_info.clock, vgm_info.loop_start);
	else printf("Chip type %u, clock %uHz\r\n", vgm_info.chip_type, vgm_info.clock);
	
	if (vgm_info.data[0x0C] == 0x80) {
	
		printf("Detected a dual chip VGM, exiting.\r\n");
		return 0;
		
	}
	
	if (*(uint32_t*)(vgm_info.data + 0x14) != 0) {
	
			vgm_info.gd3_location = l2b(vgm_info.data[0x17], vgm_info.data[0x16], vgm_info.data[0x15], vgm_info.data[0x14]) + 0x14;
		
	}
	
	//LAST CHANCE TO RETURN BEFORE READING THE REST OF THE FILE
	
	while(!mos_feof(fp)) {
			
		vgm_info.data[vgm_info.data_pointer++] = mos_fgetc(fp);
			
	}
	
	//printf("Testing GD3: %c%c%c%c\r\n", vgm_info.data[vgm_info.gd3_location + 12], vgm_info.data[vgm_info.gd3_location + 14], vgm_info.data[vgm_info.gd3_location + 16], vgm_info.data[vgm_info.gd3_location + 18]);	
	
	vgm_info.data_pointer = 0;
	
	timer0_begin(1044, 4); //44100 / 100 = 4410Hz
	
	if (vgm_info.chip_type == 0) {
		
		for (i = 0; i < vgm_info.n_channels; i++) {
		
			printf("Setting up channel %u\r\n", i);
			
			ay_states[i].volume = 0xFF;
			ay_states[i].frequency_coarse = 0xFF;
			ay_states[i].frequency_fine = 0xFF;
			ay_states[i].frequency_hz = 0;
			ay_states[i].enabled = 0;
			
			play_simple_force(i, 0, 100, 0); //Mute
	
		}
		
		vgm_info.f_scale = vgm_info.clock / 16;
		//printf("f_scale = %u", vgm_info.f_scale);
	}
	
	else if (vgm_info.chip_type == 1) {
		
		for (i = 0; i < vgm_info.n_channels; i++) {
		
			printf("Setting up channel %u\r\n", i);
			
			//sn_channels[i].volume_high = 0xFF;
			//sn_channels[i].volume_low = 0xFF;
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
	timer0_begin(1044, 4); //44100 / 100 = 4410Hz
	
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
    unsigned char byte, firstbyte, secondbyte;
    unsigned short reg = 0;
    unsigned char value = 0;
	
    //enum VgmParserState state = READ_COMMAND;

    //while (state != END_OF_SOUND_DATA) {
        switch (state) {
			printf("State: %u", state);
            case READ_COMMAND:
				//byte = vgm_info.data[vgm_info.data_pointer++];
				byte = vgm_info.data[vgm_info.data_pointer++];
                if (byte != EOF) {
                    switch (byte) {
                        case 0x61: // wait n samples
                            firstbyte = vgm_info.data[vgm_info.data_pointer++];
							secondbyte = vgm_info.data[vgm_info.data_pointer++];
							//samples_to_wait = (mos_fgetc(fp) << 8) | vgm_info.data[vgm_info.data_pointer++];
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
							value = vgm_info.data[vgm_info.data_pointer++];
							process_0x50_command(value);
							return 0;
						case 0xA0: // write to registry
                            //reg = fread(&byte, 1, fp);
							reg = vgm_info.data[vgm_info.data_pointer++];
                            value = vgm_info.data[vgm_info.data_pointer++];
                            //ay_write_register(reg, value);
							//printf("WRITE: %u value %u.\r\n", reg, value);
							//printf("W ");
							//updateAY38910(reg, value);
							process_0xA0_command(reg,value);
                            return 0;
							break;
                        case 0x66: // end of sound data
                            state = END_OF_SOUND_DATA;
                            //printf("\r\nEND OF DATA\r\n");
							
							if (vgm_info.loop_start && vgm_info.loop_enabled) {
								
								//vgm_loop_init();
								vgm_info.data_pointer = vgm_info.loop_start;
								state = READ_COMMAND;
								return 0;
								
							}
					
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
}

int main(int argc, char * argv[]) {
	
	int i;
	UINT24 t = 0;
	
	//vgm_file = mos_fopen("town.vgm", fa_read);
	//printf("%u %s %s %s", argc, argv[0], argv[1], argv[2]);
	
	if (argc < 2) return 0;
	if (argc == 3 && (!strcmp(argv[2], "loop=true"))) vgm_info.loop_enabled = true;
	else vgm_info.loop_enabled = false;
	
	vgm_file = mos_fopen(argv[1], fa_read);
    if (!vgm_file) { printf("Error: could not open file %s.\n", argv[1]); return 0; }
	if (vgm_init(vgm_file) == 0) return 0;
	while (true) if (parse_vgm_file(vgm_file) == 1) break;
	vgm_cleanup(vgm_file);

	return 0;
	
	//int bmp_return;
	//read_file("assets.txt");
	//vdp_mode(2);
	//vdp_cursorDisable();
	
	//load_wav("sega.wav", 0);
	//if(!load_bmp("lena_colormap_im.bmp", 0)) printf("Error loading BMP");
	//if(!load_bmp("32bit_sprite_colormap_im.bmp", 4)) printf("Error loading BMP");
	//if(!load_bmp("doom_colormap_im.bmp", 1)) printf("Error loading BMP");
	//if(!load_bmp("tilemap.bmp", 1)) printf("Error loading BMP");
	//if(!load_bmp_crop("tilemap.bmp", 1, 16, 336, 16, 16)) printf("Error loading BMP");
	//if(!load_bmp("24bit_gimp.bmp", 2)) printf("Error loading BMP");
	//if(!load_bmp("32bit_alpha_colomap_im.bmp", 3)) printf("Error loading BMP");
	
	//vdp_bitmapDraw(1, 0, 0);
	//parse_vgm("town.vgm");
	
	//bmp_return = load_bmp("snail2.bmp", 0, 255, 0, 255);
	
	//play_sample(0,0,120);
	
  	//load_sample("sega_intro.pcm", 0);
	//load_sample("c60.pcm", 10);
	
	// printf("\r\nQueuing a simple sound (default saw wave, 2 secs)");
	// printf("\r\nEquiv. to VDU 23 0 &85 0 1 1 &64 &70 &03 &D0 &07 in MOS/BASIC\r\n");
	// play_simple(0, 3, 100, 2000, 80);
	//delay_secs(3);
	// 
	// printf("\r\Queuing simple sound with custom wave shape (sine, 2 secs)");
	// printf("\r\nEquiv. to VDU 23 0 &85 0 1 1 &64 0 33 0 12\r\n");
	// play_simple(0, 1, 100, 2000, 1200);
	//delay_secs(3);
	
	// printf("\r\Queuing a sound with a custom ADSR envelope (pew pew!)");
	// printf("\r\nVDU 23 0 &85 0 2 2 &7F &B0 &04 &9A 0 05 00 &7F 0 0 39 0 &B8 1 1\r\n");
	// play_advanced(0, 5, 0, 127, 39, 2, 127, 154, 1200, 440, 1);
	//delay_secs(3);
		
	// printf("\r\Queuing the same envelope, varying only frequency");
	// printf("\r\nVDU 23 0 &85 0 6 0 &70 &03 0 0\r\n");
	// play_advanced_keep(0, 0, 0, 880);
	//delay_secs(3);
	
	// play_sample(0,10,120);
	// printf("\r\Queuing an uploaded PCM sample");
	// printf("\r\nEquivalent to a *tonne* of MOS/BASIC commands (but possible!)\r\n"); 
	
	// play_sample(1,0,120);
	// printf("\r\Queuing another uploaded PCM sample");
	// printf("\r\nEquivalent to a *tonne* of MOS/BASIC commands (but possible!)\r\n"); 
	 
	//while(true) {
		
		//handle_time();
		
		
/* 		if (last_sec != ticks) {
			printf("%d secs elapsed.\r\n", ticks);
			last_sec = ticks;
		} */
		
/* 		if (!step1) {
			printf("Testing delays\r\n");
			step1 = true;
		}
		
		if (delay(5) == 1 && step1 == TRUE) {
			printf ("Continuing!\r\n");
			continue;
		}
		
		if (!step2) {
			printf("\Five seconds later.\r\n");
			step2 = true;
		}
		
		if (delay(5) == 1 && step2 == TRUE) break; */
		
	//}
	
	return 0;
}

