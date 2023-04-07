#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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

UINT16 fread(void *ptr, UINT16 size, UINT16 count, UINT8 stream) {
    UINT16 total_bytes_read = 0;
    UINT16 i, j;
    
    for (i = 0; i < count; i++) {
        for (j = 0; j < size; j++) {
            int ch = mos_fgetc(stream);
            
            if (ch == EOF) {
                return total_bytes_read / size;
            }
            
            *((char *)ptr + total_bytes_read + j) = ch;
        }
        
        total_bytes_read += size;
    }
    
    return count;
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

int load_bmp(const char* filename, UINT8 slot, UINT8 alpha_g, UINT8 alpha_r, UINT8 alpha_b) {

    int width, height, bit_depth, row_padding, color_table_size, y, x, i, n, index;
    uint8_t header[54], pixel[4], color_table[1024], file, r, g, b;
	uint32_t pixel_value;
	
    file = mos_fopen(filename, fa_read);
    if (!file) {
        printf("Error: could not open file.\n");
        return -1;
    }

    for (i = 0; i < 54; i++) {
        header[i] = mos_fgetc(file);
    }

    width = *(int*)&header[18];
    height = *(int*)&header[22];
    bit_depth = *(int*)&header[28];

    if (bit_depth != 24) {
        printf("Error: unsupported bit depth.\n");
        mos_fclose(file);
        return -1;
    }

    row_padding = (4 - (width * (bit_depth / 8)) % 4) % 4;

	vdp_bitmapSelect(slot);
	putch(23); // vdu_sys
	putch(27); // sprite command
	putch(101);  // send data to selected bitmap
	
	putch(alpha_r);
	putch(alpha_g);
	putch(alpha_b);
	
	write16bit(width);
	write16bit(height);
	
    if (bit_depth == 24) {
        for (y = height - 1; y >= 0; y--) {
            for (x = 0; x < width; x++) {
                for (i = 0; i < bit_depth / 8; i++) {
                    //pixel[i] = mos_fgetc(file); //0 = B, 1 = G, 2 = R
					putch(mos_fgetc(file));
                }

                // if (bit_depth == 24) {
                    // 
					// pixel_value = 0x000000FF | (pixel[2] << 24) | (pixel[0] << 16) | pixel[1] << 8;
                // } else {
                    // pixel_value = (pixel[3] << 24) | (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];
                // }
                //putch(0xFF); 		//AA
				//putch(pixel[0]);	//BB
				//putch(pixel[1]);	//GG
				//putch(pixel[2]);	//RR
				
				//write32bit(pixel_value);
				//output++ = pixel_value;
				//printf("0x%08X ", pixel_value);
            }

            for (i = 0; i < row_padding; i++) {
                mos_fgetc(file);
            }
        }
		printf("Width: %u, Height: %u, BPP: %u", width, height, bit_depth);    
	}

    mos_fclose(file);
	return width * height;
	
	
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

int main(int argc, char * argv[]) {
	
	int bmp_return;
	//vdp_mode(0);
	bmp_return = load_bmp("snail2.bmp", 0);
	vdp_bitmapDraw(0, 0, 0);
	
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

