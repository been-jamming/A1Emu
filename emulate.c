#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cpu.h"

#ifdef _WIN32

#include <curses.h>

#else

#include <ncurses.h>

void Sleep(long int msec){
	struct timespec sleep_time;

	sleep_time.tv_sec = msec/1000;
	sleep_time.tv_nsec = (msec%1000)*1000000;
	nanosleep(&sleep_time, NULL);
}

#endif

unsigned char DEBUG_STEP = 0;

uint8_t memory[0x10000];

uint32_t tape[0x100000];

uint32_t tape_index;

char str_buffer[256];

unsigned char next_tape_index;

unsigned char tape_active;

unsigned char tape_writing;

unsigned char tape_reading;

unsigned char current_tape_value;

void print_state(CPU_6502 cpu){
	printw("A: %02x X:%02x Y:%02x SP:%02x P:%02x PC:%02x\n", (int) cpu.A_reg, (int) cpu.X_reg, (int) cpu.Y_reg, (int) cpu.SP_reg, (int) cpu.P_reg, (int) cpu.PC_reg);
	printw("\nNext: %02x %02x %02x\n", (int) memory[cpu.PC_reg], (int) memory[cpu.PC_reg + 1], (int) memory[cpu.PC_reg + 2]);
}

void load_tape(char *file_name){
	FILE *fp;
	fp = fopen(file_name, "rb");
	printw("Reading from tape file named \"%s\"\n", file_name);
	if(fp){
		fread(tape, 4, 0x100000, fp);
		fclose(fp);
	} else {
		printw("file error\n");
	}

	return;
}

void store_tape(char *file_name){
	FILE *fp;
	printw("Storing tape to file named \"%s\"\n", file_name);
	fp = fopen(file_name, "wb");
	if(fp){
		fwrite(tape, 4, 0x100000, fp);
		fclose(fp);
	} else {
		printw("file error\n");
	}
}

//Special read memory routine for memory-mapped I/O
uint8_t read_mem(uint16_t index){
	uint8_t output;

	if(DEBUG_STEP){
		printw("READ: %04x ", index);
	}
	
	if(tape_writing && index >= 0xC000 && index <= 0xC0FF){
		next_tape_index = 1;
	} else if(index >= 0xC081 && index <= 0xC0FF){
		if(current_tape_value){
			output = memory[index];
		} else {
			output = memory[index&0xFFFE];
		}
	} else {
		output = memory[index];
	}

	if(index == 0xD010){
		memory[0xD011] &= 0x7F;
	} else if((index&0xFF0F) == 0xD002){
		output = 0;
	}
	if(DEBUG_STEP){
		printw("%02x\n", memory[index]);
	}
	return output;
}

//Special write memory routine for memory mapped I/O
void write_mem(uint16_t index, uint8_t value){
	uint8_t x;
	uint8_t y;

	if(DEBUG_STEP){
		printw("WRITE: %02x --> %04x\n", value, index);
	}
	if((index&0xFF0F) == 0xD002){
		if((value&0x7F) == '\n' || (value&0x7F) == '\r'){//Print \n instead of \r
			printw("\n");
		} else if((value&0x7F) == 0x5F){//Make the 0x5F character map to ASCII backspace
			printw("\b \b");
		} else if((value&0x7F) >= 0x20 && (value&0x7F) != 127){
			//Output a character
			printw("%c", value&0x7F);
		}
	}
	memory[index] = value;
}

int main(){
	CPU_6502 cpu;
	FILE *fp;
	int key_hit;
	unsigned int count = 0;
	char temp_char;
	unsigned char str_index;
	unsigned long long int last_cycles;
	unsigned char last_cycle_diff;
	unsigned long long int last_time;
	struct timespec current_time;
	unsigned long long int next_time;

	cpu.A_reg = 0;
	cpu.X_reg = 0;
	cpu.Y_reg = 0;
	cpu.SP_reg = 0;
	cpu.P_reg = 0;
	cpu.PC_reg = 0xE000;
	tape_writing = 0;
	tape_reading = 0;

	initscr();
	cbreak();
	noecho();
	scrollok(stdscr, 1);

	//Load Integer Basic
	fp = fopen("BASIC", "rb");
	if(fp){
		fread(memory + 0xE000, 1, 0x1000, fp);
		fclose(fp);
	} else {
		printw("Warning: could not load file named \"BASIC\".\nStarting without apple 1 BASIC loaded.\nApple 1 basic can still be loaded from a cassette file into address 0xE000.\n---\n");
		printw("Press any key to continue...\n");
		getch();
		clear();
	}
	
	//Load Woz's ACI
	fp = fopen("WOZACI", "rb");
	if(fp){
		fread(memory + 0xC000, 1, 0x100, fp);
		fclose(fp);
	} else {
		fprintf(stderr, "file error\n");
		exit(1);
	}

	memcpy(memory + 0xC100, memory + 0xC000, 0x100);

	//Load Woz's monitor
	fp = fopen("WOZMON", "rb");
	if(fp){
		fread(memory + 0xFF00, 1, 0x100, fp);
		fclose(fp);
	} else {
		fprintf(stderr, "file error\n");
		exit(1);
	}
	reset_6502(&cpu, read_mem);//Reset the cpu
	cpu.cycles = 0;
	last_cycles = 0;
	
	//Initialize the timing
	clock_gettime(CLOCK_MONOTONIC, &current_time);
	last_time = 0;
	last_cycle_diff = 0;
	next_tape_index = 0;
	nodelay(stdscr, 1);
	while(1){
		/* Limit the speed of the processor
		 * based on how many cycles the last
		 * instruction took. The 6502 on the
		 * Apple 1 was clocked at 1 MHz.
		 */
		if(!DEBUG_STEP){
			last_time += last_cycle_diff;
			if(last_time > 1000){
				Sleep(last_time/1000);
				last_time -= 1000;
			}
		}

		//Execute the instruction
		execute_6502(&cpu, read_mem, write_mem);
		count = count + 1;
		
		//Debugging I/O
		if(DEBUG_STEP){
			memset(str_buffer, 256, 0);
			echo();
			getstr(str_buffer);
			noecho();
			temp_char = str_buffer[6];
			str_buffer[6] = (char) 0;

			if(!strcmp(str_buffer, "resume")){
				DEBUG_STEP = 0;
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				last_time = 0;
				nodelay(stdscr, 1);
			} else if(temp_char == ' ' && !strcmp(str_buffer, "tstart")){
				tape_active = 1;
				str_buffer[12] = (char) 0;
				if(temp_char == ' ' && !strcmp(str_buffer + 7, "write")){
					tape_writing = 1;
					printw("WRITING TO TAPE\n");
				}
				str_buffer[11] = (char) 0;
				if(temp_char == ' ' && !strcmp(str_buffer + 7, "read")){
					tape_reading = 1;
					current_tape_value = 0;
					printw("READING FROM TAPE\n");
				}
				tape_index = 0;
			} else if(temp_char == ' ' && !strcmp(str_buffer, "tstore")){
				str_index = 0;
				while(str_buffer[str_index] != '\n' && str_index < 255){
					str_index++;
				}
				str_buffer[str_index] = (char) 0;
				store_tape(str_buffer + 7);
			}

			str_buffer[6] = temp_char;
			temp_char = str_buffer[5];
			str_buffer[5] = (char) 0;

			if(!strcmp(str_buffer, "tstop")){
				tape_active = 0;
				tape_writing = 0;
				tape_reading = 0;
			} else if(!strcmp(str_buffer, "reset")){
				reset_6502(&cpu, read_mem);
			} else if(!strcmp(str_buffer, "quit")){
				break;
			}

			if(temp_char == ' ' && !strcmp(str_buffer, "tload")){
				str_index = 0;
				while(str_buffer[str_index] != '\r' && str_buffer[str_index] != '\n' && str_index < 255){
					str_index++;
				}
				str_buffer[str_index] = (char) 0;
				load_tape(str_buffer + 6);
			}

			if(str_buffer[0] != (char) 0 && str_buffer[1] == (char) 0){
				memory[0xD010] = str_buffer[0]|0x80;
				memory[0xD011] |= 0x80;
			}
		}

		//Handle keyboard I/O
		if(!(count%200) && (key_hit = getch()) != ERR){
			if(key_hit == 0x08){//Emulate the backspace character
				memory[0xD010] = 0xDF;
				memory[0xD011] |= 0x80;
			} else if(key_hit == '~'){//Emulate the control character
				nodelay(stdscr, 0);
				key_hit = getch();
				nodelay(stdscr, 1);
				if(key_hit == 'd' || key_hit == 'D'){//Ctrl-D
					memory[0xD010] = 0x84;
					memory[0xD011] |= 0x80;
				} else if(key_hit == 'g' || key_hit == 'G'){//Ctrl-G (bell character)
					memory[0xD010] = 0x87;
					memory[0xD011] |= 0x80;
				} else if(key_hit == '`'){//Escape
					memory[0xD010] = 0x9B;
					memory[0xD011] |= 0x80;
				}
			} else if(key_hit == '|'){
				DEBUG_STEP = 1;
				printw("\n");
				nodelay(stdscr, 0);
			} else {
				//Convert lower case characters to upper case
				if(key_hit >= 'a' && key_hit <= 'z'){
					key_hit += 'A' - 'a';
				}

				//Replace \n with \r
				if(key_hit == '\n'){
					key_hit = '\r';
				}

				memory[0xD010] = key_hit|0x80;
				memory[0xD011] |= 0x80;
			}
		}

		last_cycle_diff = cpu.cycles - last_cycles;

		//Handle tape
		if(tape_active && tape_reading){
			while(last_cycles < cpu.cycles){
				if(!tape[tape_index]){
					tape_index++;
					current_tape_value = !current_tape_value;
				}
				tape[tape_index] -= 1;
				last_cycles++;
			}
		} else if(tape_active && tape_writing){
			tape[tape_index] += cpu.cycles - last_cycles;
			if(next_tape_index){
				tape_index++;
				next_tape_index = 0;
			}
			last_cycles = cpu.cycles;
		} else {
			last_cycles = cpu.cycles;
		}
		refresh();
	}

	printw("Press any key to exit...\n");
	nodelay(stdscr, 0);
	getch();

	endwin();
}
