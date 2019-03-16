#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <sys/time.h>
#include "cpu.h"

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
	printf("A: %02x X:%02x Y:%02x SP:%02x P:%02x PC:%02x\n", (int) cpu.A_reg, (int) cpu.X_reg, (int) cpu.Y_reg, (int) cpu.SP_reg, (int) cpu.P_reg, (int) cpu.PC_reg);
	printf("\nNext: %02x %02x %02x\n", (int) memory[cpu.PC_reg], (int) memory[cpu.PC_reg + 1], (int) memory[cpu.PC_reg + 2]);
}

void load_tape(char *file_name){
	FILE *fp;
	fp = fopen(file_name, "rb");
	printf("Reading from tape file named \"%s\"\n", file_name);
	if(fp){
		fread(tape, 4, 0x100000, fp);
		fclose(fp);
	} else {
		printf("file error\n");
	}

	return;
}

void store_tape(char *file_name){
	FILE *fp;
	printf("Storing tape to file named \"%s\"\n", file_name);
	fp = fopen(file_name, "wb");
	if(fp){
		fwrite(tape, 4, 0x100000, fp);
		fclose(fp);
	} else {
		printf("file error\n");
	}
}

//Special read memory routine for memory-mapped I/O
uint8_t read_mem(uint16_t index){
	uint8_t output;

	if(DEBUG_STEP){
		printf("READ: %04x ", index);
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
		printf("%02x\n", memory[index]);
	}
	return output;
}

//Special write memory routine for memory mapped I/O
void write_mem(uint16_t index, uint8_t value){
	uint8_t x;
	uint8_t y;

	if(DEBUG_STEP){
		printf("WRITE: %02x --> %04x\n", value, index);
	}
	if((index&0xFF0F) == 0xD002){
		if((value&0x7F) == '\n' || (value&0x7F) == '\r'){
			printf("\n");
		} else if((value&0x7F) >= 0x20 && (value&0x7F) != 127){
			//Output a character
			printf("%c", value&0x7F);
		}
	}
	memory[index] = value;
}

int main(){
	CPU_6502 cpu;
	FILE *fp;
	unsigned char key_hit;
	unsigned int count = 0;
	unsigned long int last_time;
	unsigned long int next_time;
	struct timeval current_time;
	char temp_char;
	unsigned char str_index;
	unsigned long long int last_cycles;
	unsigned char last_cycle_diff;

	cpu.A_reg = 0;
	cpu.X_reg = 0;
	cpu.Y_reg = 0;
	cpu.SP_reg = 0;
	cpu.P_reg = 0;
	cpu.PC_reg = 0xE000;
	tape_writing = 0;
	tape_reading = 0;

	//Load Integer Basic
	fp = fopen("BASIC", "rb");
	if(fp){
		fread(memory + 0xE000, 1, 0x1000, fp);
	} else {
		printf("Warning: could not load file named \"BASIC\".\nStarting without apple 1 BASIC loaded.\nApple 1 basic can still be loaded from a cassette file into address 0xE000.\n---\n");
	}
	fclose(fp);
	
	//Load Woz's ACI
	fp = fopen("WOZACI", "rb");
	if(fp){
		fread(memory + 0xC000, 1, 0x100, fp);
	} else {
		printf("file error\n");
		exit(1);
	}
	fclose(fp);

	memcpy(memory + 0xC100, memory + 0xC000, 0x100);

	//Load Woz's monitor
	fp = fopen("WOZMON", "rb");
	if(fp){
		fread(memory + 0xFF00, 1, 0x100, fp);
	} else {
		printf("file error\n");
		exit(1);
	}
	fclose(fp);
	reset_6502(&cpu, read_mem);//Reset the cpu
	cpu.cycles = 0;
	last_cycles = 0;
	
	//Initialize the timing
	gettimeofday(&current_time, NULL);
	last_time = current_time.tv_usec + current_time.tv_sec*1000000;
	last_cycle_diff = 0;
	next_tape_index = 0;
	while(1){
		/* Limit the speed of the processor to
		 * a realistic 4nsec per instruction
		 *
		 * Will count cpu cycles soon to make
		 * better timing.
		 */
		if(!DEBUG_STEP){
			do{
				gettimeofday(&current_time, NULL);
				next_time = current_time.tv_usec + current_time.tv_sec*1000000;
			} while(next_time - last_time < last_cycle_diff);
			last_time = last_time + last_cycle_diff;
		}

		//Execute the instruction
		execute_6502(&cpu, read_mem, write_mem);
		count = count + 1;
		
		//Debugging I/O
		if(DEBUG_STEP){
			memset(str_buffer, 256, 0);
			fgets(str_buffer, 256, stdin);
			temp_char = str_buffer[6];
			str_buffer[6] = (char) 0;

			if(!strcmp(str_buffer, "resume")){
				DEBUG_STEP = 0;
				gettimeofday(&current_time, NULL);
				last_time = current_time.tv_usec + current_time.tv_sec*1000000;
			} else if(temp_char == ' ' && !strcmp(str_buffer, "tstart")){
				tape_active = 1;
				str_buffer[12] = (char) 0;
				if(temp_char == ' ' && !strcmp(str_buffer + 7, "write")){
					tape_writing = 1;
					printf("WRITING TO TAPE\n");
				}
				str_buffer[11] = (char) 0;
				if(temp_char == ' ' && !strcmp(str_buffer + 7, "read")){
					tape_reading = 1;
					current_tape_value = 0;
					printf("READING FROM TAPE\n");
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
			}

			if(temp_char == ' ' && !strcmp(str_buffer, "tload")){
				str_index = 0;
				while(str_buffer[str_index] != '\n' && str_index < 255){
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
		if(!(count%200) && kbhit()){
			key_hit = getch();

			if(key_hit == '|'){
				DEBUG_STEP = 1;
				printf("\n");
			} else {
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
	}
}
