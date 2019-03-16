/*
 * 6502 Emulator
 *
 * Spring Break programming project
 * 
 * 3/5/2019
 * by Ben Jones
 */

#include <stdint.h>

//Bits of the status register and correseponding flags
#define CARRY 0
#define ZERO 1
#define INTERRUPT 2
#define DECIMAL 3
#define BREAK 4
#define OVERFLOW 6
#define NEGATIVE 7

typedef struct CPU_6502 CPU_6502;

//Store the state of cpu
struct CPU_6502{
	//The ALU loads and stores to and from the accumulator
	uint8_t A_reg;
	//Index registers
	uint8_t X_reg;
	uint8_t Y_reg;
	//The stack pointer is actually a 16 bit value but the first byte is hard-wired to 0x01, so you only need 8 bits to store it
	uint8_t SP_reg;
	uint16_t PC_reg;
	//The bits status register are all of the processor flags, but it is its own register whose value can be pushed to the stack
	uint8_t P_reg;
	unsigned long long int cycles;
};


void execute_6502(CPU_6502 *cpu, uint8_t (*read)(uint16_t), void (*write)(uint16_t, uint8_t));

void reset_6502(CPU_6502 *cpu, uint8_t (*read)(uint16_t));
