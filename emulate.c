#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <sys/time.h>
#include "cpu.h"

unsigned DEBUG_STEP = 0;

uint8_t memory[0x10000];

void print_state(CPU_6502 cpu){
	printf("A: %02x X:%02x Y:%02x SP:%02x P:%02x PC:%02x\n", (int) cpu.A_reg, (int) cpu.X_reg, (int) cpu.Y_reg, (int) cpu.SP_reg, (int) cpu.P_reg, (int) cpu.PC_reg);
	printf("\nNext: %02x %02x %02x\n", (int) memory[cpu.PC_reg], (int) memory[cpu.PC_reg + 1], (int) memory[cpu.PC_reg + 2]);
}

//Special read memory routine for memory-mapped I/O
uint8_t read_mem(uint16_t index){
	uint8_t output;

	if(DEBUG_STEP){
		printf("READ: %04x ", index);
	}
	output = memory[index];
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

	cpu.A_reg = 0;
	cpu.X_reg = 0;
	cpu.Y_reg = 0;
	cpu.SP_reg = 0;
	cpu.P_reg = 0;
	cpu.PC_reg = 0xE000;

	//Load Integer Basic
	fp = fopen("BASIC", "rb");
	if(fp){
		fread(memory + 0xE000, 1, 0x2000, fp);
	} else {
		printf("file error\n");
		exit(1);
	}
	fclose(fp);
	
	//The emulator does not support ACI at the moment
	
	//Load Woz's monitor
	fp = fopen("WOZMON", "rb");
	if(fp){
		fread(memory + 0xFF00, 1, 0x100, fp);
	} else {
		printf("file error 3\n");
		exit(1);
	}
	fclose(fp);
	reset_6502(&cpu, read_mem);//Reset the cpu
	
	//Initialize the timing
	gettimeofday(&current_time, NULL);
	last_time = current_time.tv_usec + current_time.tv_sec*1000000;
	while(1){
		/*Limit the speed of the processor to
		 * a realistic 4usec per instruction
		 *
		 * Will count cpu cycles soon to make
		 * better timing.
		 */
		do{
			gettimeofday(&current_time, NULL);
			next_time = current_time.tv_usec + current_time.tv_sec*1000000;
		} while(next_time - last_time < 4);
		last_time = last_time + 4;

		//Execute the instruction
		execute_6502(&cpu, read_mem, write_mem);
		count = count + 1;
		
		//Debugging I/O
		if(DEBUG_STEP){
			print_state(cpu);
			key_hit = getchar();
			if(key_hit == '|'){
				DEBUG_STEP = 0;
			} else if(key_hit != ' '){
				memory[0xD010] = key_hit|0x80;
				memory[0xD011] = 0xFF;
			}
		}

		//Handle keyboard I/O
		if(!(count%200) && kbhit()){
			key_hit = getch();

			if(key_hit == '|'){
				DEBUG_STEP = 1;
			} else {
				memory[0xD010] = key_hit|0x80;
				memory[0xD011] |= 0x80;
			}
		}
	}
}
