#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include "mos-interface.h"
#include "vdp.h"
#include "bmp.h"

extern void write16bit(UINT16 w);
extern void write32bit(UINT32 w);

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

void fread(byte *buffer, uint32_t num_bytes, UINT8 file) {
    int byte_value, i;
	for (i = 0; i < num_bytes; i++) {
        byte_value = mos_fgetc(file);
        if (mos_feof(file)) {
            printf("Error: end of file reached unexpectedly\r\n");
            return;
        }
        buffer[i] = (byte) byte_value;
    }
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
		else if (bit_rate == 16) write32bit(data_size / 2);
		
		write16bit(sample_rate);
	
    for (i = 0; i < data_size && !mos_feof(fp); i++) {
        
		if (bit_rate == 8) {
			
			sample8 = mos_fgetc(fp) - 128;
			putch(sample8);
			
		}
    }

    mos_fclose(fp);
}

int main(int argc, char * argv[]) {
	
	int bmp_return;
	//read_file("assets.txt");
	//vdp_mode(2);
	//vdp_cursorDisable();
	
	//load_wav("sega.wav", 0);
	//if(!load_bmp("lena_colormap_im.bmp", 0)) printf("Error loading BMP");
	//if(!load_bmp("32bit_sprite_colormap_im.bmp", 4)) printf("Error loading BMP");
	//if(!load_bmp("doom_colormap_im.bmp", 1)) printf("Error loading BMP");
	if(!load_bmp("tilemap.bmp", 1)) printf("Error loading BMP");
	//if(!load_bmp_crop("tilemap.bmp", 1, 16, 336, 16, 16)) printf("Error loading BMP");
	//if(!load_bmp("24bit_gimp.bmp", 2)) printf("Error loading BMP");
	//if(!load_bmp("32bit_alpha_colomap_im.bmp", 3)) printf("Error loading BMP");
	
	vdp_bitmapDraw(1, 0, 0);
	
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

