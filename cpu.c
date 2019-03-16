/*
 * 6502 Emulator
 *
 * Spring Break programming project
 * 
 * 3/5/2019
 * by Ben Jones
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <conio.h>
#include <time.h>
#include "cpu.h"

unsigned char CROSSED_PAGE;

unsigned char different_page(uint16_t address1, uint16_t address2){
	return (address1&0xFF00) != (address2&0xFF00);
}

uint8_t get_absolute(CPU_6502 *cpu, uint8_t index, uint8_t (*read)(uint16_t)){
	uint16_t address1;
	uint16_t address2;

	address1 = ((uint16_t) read(cpu->PC_reg + 1)) | (((uint16_t) read(cpu->PC_reg + 2))<<8);
	address2 = address1 + index;

	CROSSED_PAGE = different_page(address1, address2);

	return read(address2);
}

uint8_t get_indirect_X(CPU_6502 *cpu, uint8_t index, uint8_t (*read)(uint16_t)){
	return read(((uint16_t) read(cpu->X_reg + index)) | (((uint16_t) read(cpu->X_reg + index + 1))<<8));
}

uint8_t get_indirect_Y(CPU_6502 *cpu, uint8_t index, uint8_t (*read)(uint16_t)){
	uint16_t address1;
	uint16_t address2;

	address1 = ((uint16_t) read(index)) | (((uint16_t) read(index + 1))<<8);
	address2 = address1 + cpu->Y_reg;

	CROSSED_PAGE = different_page(address1, address2);

	return read(address2);
}

void set_absolute(CPU_6502 *cpu, uint8_t index, uint8_t (*read)(uint16_t), void (*write)(uint16_t, uint8_t), uint8_t value){
	uint16_t address1;
	uint16_t address2;

	address1 = ((uint16_t) read(cpu->PC_reg + 1)) | (((uint16_t) read(cpu->PC_reg + 2))<<8);
	address2 = address1 + index;

	CROSSED_PAGE = different_page(address1, address2);

	write(address2, value);
}

void set_indirect_X(CPU_6502 *cpu, uint8_t index, uint8_t (*read)(uint16_t), void (*write)(uint16_t, uint8_t), uint8_t value){
	write(((uint16_t) read(cpu->X_reg + index)) | (((uint16_t) read(cpu->X_reg + index + 1))<<8), value);
}

void set_indirect_Y(CPU_6502 *cpu, uint8_t index, uint8_t (*read)(uint16_t), void (*write)(uint16_t, uint8_t), uint8_t value){
	uint16_t address1;
	uint16_t address2;

	address1 = ((uint16_t) read(index)) | (((uint16_t) read(index + 1))<<8);
	address2 = address1 + cpu->Y_reg;

	CROSSED_PAGE = different_page(address1, address2);
	
	write(address2, value);
}

void push(CPU_6502 *cpu, void (*write)(uint16_t, uint8_t), uint8_t value){
	write(0x100 | cpu->SP_reg, value);
	cpu->SP_reg -= 1;
}

uint8_t pop(CPU_6502 *cpu, uint8_t (*read)(uint16_t)){
	cpu->SP_reg += 1;
	return read(0x100 | cpu->SP_reg);
}

//Execute a single 6502 instruction, updating the state of the CPU
void execute_6502(CPU_6502 *cpu, uint8_t (*read)(uint16_t), void (*write)(uint16_t, uint8_t)){
	//Various intermediate values for calculation
	uint8_t value1;
	uint8_t value2;
	uint8_t value3;
	uint16_t value_absolute;
	uint8_t prev;
	//The first byte at PC uniquely determines the operation
	uint8_t opcode;
	uint16_t address1;
	uint16_t address2;

	opcode = read(cpu->PC_reg);

	//ADC
	if(opcode == 0x69 || opcode == 0x65 || opcode == 0x75 || opcode == 0x6D || opcode == 0x7D || opcode == 0x79 || opcode == 0x61 || opcode == 0x71){
		if(opcode == 0x69){//Immediate
			value1 = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0x65){//Zero page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0x75){//Zero page X
			value1 = read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0x6D){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0x7D){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0x79){//Absolute Y
			value1 = get_absolute(cpu, cpu->Y_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0x61){//Indirect X
			value1 = get_indirect_X(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0x71){//Indirect Y
			value1 = get_indirect_Y(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			if(CROSSED_PAGE){
				cpu->cycles += 6;
			} else {
				cpu->cycles += 5;
			}
		}

		prev = cpu->A_reg;
		//If the carry flag is set, add 1 more to result
		if(cpu->P_reg&(1<<CARRY)){
			value1++;
		}
		
		cpu->A_reg += value1;
		//If the decimal flag is set, correct the output
		if(cpu->P_reg&(1<<DECIMAL)){
			//Using double-dabble ish trick here
			if((cpu->A_reg&0xF) > 9){
				cpu->A_reg += 0x6;
			}
		}

		//Set the carry bit accordingly
		if(cpu->A_reg < prev){
			cpu->P_reg |= (1<<CARRY);
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}
		//Set overflow flag
		if(((value1&0x80) && (prev&0x80) && !(cpu->A_reg&0x80)) || (!(value1&0x80) && !(prev&0x80) && (cpu->A_reg&0x80))){
			cpu->P_reg |= (1<<OVERFLOW);
		} else {
			cpu->P_reg &= ~(1<<OVERFLOW);
		}
		//Set negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= (1<<NEGATIVE);
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
		//Set zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= (1<<ZERO);
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}
	//AND
	} else if(opcode == 0x29 || opcode == 0x25 || opcode == 0x35 || opcode == 0x2D || opcode == 0x3D || opcode == 0x39 || opcode == 0x21 || opcode == 0x31){
		if(opcode == 0x29){//Immediate
			value1 = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0x25){//Zero page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0x35){//Zero page X
			value1 = read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0x2D){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0x3D){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0x39){//Absolute Y
			value1 = get_absolute(cpu, cpu->Y_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0x21){//Indirect X
			value1 = get_indirect_X(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0x31){//Indirect Y
			value1 = get_indirect_Y(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			if(CROSSED_PAGE){
				cpu->cycles += 6;
			} else {
				cpu->cycles += 5;
			}
		}

		//Perform the AND
		cpu->A_reg &= value1;
		
		//Set the zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= (1<<ZERO);
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}
		//Set the negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= (1<<NEGATIVE);
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//ASL
	} else if(opcode == 0x0A || opcode == 0x06 || opcode == 0x16 || opcode == 0x0E || opcode == 0x1E){
		if(opcode == 0x0A){//Accumulator
			value1 = cpu->A_reg;
			cpu->cycles += 2;
		} else if(opcode == 0x06){//Zero page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->cycles += 5;
		} else if(opcode == 0x16){//Zero page X
			value1 = read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->cycles += 6;
		} else if(opcode == 0x0E){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->cycles += 6;
		} else if(opcode == 0x1E){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			cpu->cycles += 7;
		}

		//Set the carry flag
		if(value1&0x80){
			cpu->P_reg |= (1<<CARRY);
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}

		//Perform the shift
		value1 <<= 1;

		if(opcode == 0x0A){//Accumulator
			cpu->A_reg = value1;
			cpu->PC_reg += 1;
		} else if(opcode == 0x06){//Zero page
			write(read(cpu->PC_reg + 1), value1);
			cpu->PC_reg += 2;
		} else if(opcode == 0x16){//Zero page X
			write((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF, value1);
			cpu->PC_reg += 2;
		} else if(opcode == 0x0E){//Absolute
			set_absolute(cpu, 0, read, write, value1);
			cpu->PC_reg += 3;
		} else if(opcode == 0x1E){//Absolute X
			set_absolute(cpu, cpu->X_reg, read, write, value1);
			cpu->PC_reg += 3;
		}

		//Set the zero flag
		if(!value1){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value1&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//BCC
	} else if(opcode == 0x90){//Branch carry clear
		cpu->cycles += 2;
		
		if(!(cpu->P_reg&(1<<CARRY))){//If the carry is clear
			cpu->cycles++;
			value1 = read(cpu->PC_reg + 1);
			if(value1&0x80){//If it is a negative jump
				address1 = cpu->PC_reg - (((uint16_t) ((~value1) - 1))&0xFF);
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			} else {//If it is a positive jump
				address1 = cpu->PC_reg + value1 + 2;
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			}
		} else {
			cpu->PC_reg += 2;
		}
	//BCS
	} else if(opcode == 0xB0){//Branch carry set
		cpu->cycles += 2;

		if(cpu->P_reg&(1<<CARRY)){//If the carry is set
			cpu->cycles++;
			value1 = read(cpu->PC_reg + 1);
			if(value1&0x80){//If it is a negative jump
				address1 = cpu->PC_reg - (((uint16_t) ((~value1) - 1))&0xFF);
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			} else {//If it is a positive jump
				address1 = cpu->PC_reg + value1 + 2;
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			}
		} else {
			cpu->PC_reg += 2;
		}
	//BEQ
	} else if(opcode == 0xF0){//Branch equals
		cpu->cycles += 2;

		if(cpu->P_reg&(1<<ZERO)){//If the zero flag is set
			cpu->cycles++;
			value1 = read(cpu->PC_reg + 1);
			if(value1&0x80){//If it is a negative jump
				address1 = cpu->PC_reg - (((uint16_t) ((~value1) - 1))&0xFF);
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			} else {//If it is a postive jump
				address1 = cpu->PC_reg + value1 + 2;
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			}
		} else {
			cpu->PC_reg += 2;
		}
	//BIT
	} else if(opcode == 0x24 || opcode == 0x2C){
		if(opcode == 0x24){//Zero page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0x2C){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		}

		//Set the zero flag
		if(!(value1&cpu->A_reg)){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value1&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}

		//Set the overflow flag
		if(value1&0x40){
			cpu->P_reg |= 1<<OVERFLOW;
		} else {
			cpu->P_reg &= ~(1<<OVERFLOW);
		}
	//BMI
	} else if(opcode == 0x30){//Branch minus
		cpu->cycles += 2;

		if(cpu->P_reg&(1<<NEGATIVE)){//Test the negative flag
			cpu->cycles++;
			value1 = read(cpu->PC_reg + 1);
			if(value1&0x80){//If it is a negative jump
				address1 = cpu->PC_reg - (((uint16_t) ((~value1) - 1))&0xFF);
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			} else {//If it is a positive jump
				address1 = cpu->PC_reg + value1 + 2;
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			}
		} else {
			cpu->PC_reg += 2;
		}
	//BNE
	} else if(opcode == 0xD0){//Branch not equal
		cpu->cycles += 2;

		if(!(cpu->P_reg&(1<<ZERO))){
			cpu->cycles++;
			value1 = read(cpu->PC_reg + 1);
			if(value1&0x80){//If it is a negative jump
				address1 = cpu->PC_reg - (((uint16_t) ((~value1) - 1))&0xFF);
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			} else {//If it is a positive jump
				address1 = cpu->PC_reg + value1 + 2;
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			}
		} else {
			cpu->PC_reg += 2;
		}
	//BPL
	} else if(opcode == 0x10){//Branch positive
		cpu->cycles += 2;

		if(!(cpu->P_reg&(1<<NEGATIVE))){
			cpu->cycles++;
			value1 = read(cpu->PC_reg + 1);
			if(value1&0x80){//If it is a negative jump
				address1 = cpu->PC_reg - (((uint16_t) ((~value1) - 1))&0xFF);
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			} else {//If it is a positive jump
				address1 = cpu->PC_reg + value1 + 2;
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			}
		} else {
			cpu->PC_reg += 2;
		}
	//BRK
	} else if(opcode == 0x00){//Break (forced interrupt)
		//Set the break flag
		cpu->P_reg |= 1<<BREAK;
		cpu->PC_reg += 2;
		push(cpu, write, (cpu->PC_reg&0xFF00)>>8);
		push(cpu, write, cpu->PC_reg&0xFF);
		push(cpu, write, cpu->P_reg);
		cpu->P_reg |= 1<<INTERRUPT;
		cpu->PC_reg = (((uint16_t) read(0xFFFF))<<8) | read(0xFFFE);
		cpu->cycles += 7;
	//BVC
	} else if(opcode == 0x50){//Branch overflow clear
		cpu->cycles += 2;

		if(!(cpu->P_reg&(1<<OVERFLOW))){
			cpu->cycles++;
			value1 = read(cpu->PC_reg + 1);
			if(value1&0x80){//If it is a negative jump
				address1 = cpu->PC_reg - (((uint16_t) ((~value1) - 1))&0xFF);
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			} else {//If it is a positive jump
				address1 = cpu->PC_reg + value1 + 2;
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			}
		} else {
			cpu->PC_reg += 2;
		}
	//BVS
	} else if(opcode == 0x70){//Branch overflow set
		cpu->cycles += 2;

		if(cpu->P_reg&(1<<OVERFLOW)){
			cpu->cycles++;
			value1 = read(cpu->PC_reg + 1);
			if(value1&0x80){//If it is a negative jump
				address1 = cpu->PC_reg - (((uint16_t) ((~value1) - 1))&0xFF);
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			} else {//If it is a positive jump
				address1 = cpu->PC_reg + value1 + 2;
				if(different_page(cpu->PC_reg, address1)){
					cpu->cycles++;
				}
				cpu->PC_reg = address1;
			}
		} else {
			cpu->PC_reg += 2;
		}
	//CLC
	} else if(opcode == 0x18){//Clear carry flag
		cpu->P_reg &= ~(1<<CARRY);
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//CLD
	} else if(opcode == 0xD8){//Clear decimal flag
		cpu->P_reg &= ~(1<<DECIMAL);
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//CLI
	} else if(opcode == 0x58){//Clear interrupt flag
		cpu->P_reg &= ~(1<<INTERRUPT);
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//CLV
	} else if(opcode == 0xB8){//Clear overflow flag
		cpu->P_reg &= ~(1<<OVERFLOW);
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//CMP
	} else if(opcode == 0xC9 || opcode == 0xC5 || opcode == 0xD5 || opcode == 0xCD || opcode == 0xDD || opcode == 0xD9 || opcode == 0xC1 || opcode == 0xD1){
		if(opcode == 0xC9){//Immediate
			value1 = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0xC5){//Zero page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0xD5){//Zero page X
			value1 = read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0xCD){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0xDD){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0xD9){//Absolute Y
			value1 = get_absolute(cpu, cpu->Y_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0xC1){//Indirect X
			value1 = get_indirect_X(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0xD1){//Indirect Y
			value1 = get_indirect_Y(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			if(CROSSED_PAGE){
				cpu->cycles += 6;
			} else {
				cpu->cycles += 5;
			}
		}

		value2 = cpu->A_reg - value1;
		
		//Set the zero flag
		if(!value2){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value2&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}

		//Set the carry flag
		if(cpu->A_reg >= value1){
			cpu->P_reg |= 1<<CARRY;
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}
	//CPX
	} else if(opcode == 0xE0 || opcode == 0xE4 || opcode == 0xEC){
		if(opcode == 0xE0){//Immediate
			value1 = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0xE4){//Zero Page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0xEC){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		}

		value2 = cpu->X_reg - value1;

		//Set the zero flag
		if(!value2){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value2&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}

		//Set the carry flag
		if(cpu->X_reg >= value1){
			cpu->P_reg |= 1<<CARRY;
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}
	//CPY
	} else if(opcode == 0xC0 || opcode == 0xC4 || opcode == 0xCC){
		if(opcode == 0xC0){//Immediate
			value1 = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0xC4){//Zero Page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0xCC){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		}

		value2 = cpu->Y_reg - value1;

		//Set the zero flag
		if(!value2){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value2&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}

		//Set the carry flag
		if(cpu->Y_reg >= value1){
			cpu->P_reg |= 1<<CARRY;
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}
	//DEC
	} else if(opcode == 0xC6 || opcode == 0xD6 || opcode == 0xCE || opcode == 0xDE){
		if(opcode == 0xC6){//Zero page
			value2 = read(cpu->PC_reg + 1);
			value1 = read(value2);
			value1--;
			write(value2, value1);
			cpu->PC_reg += 2;
			cpu->cycles += 5;
		} else if(opcode == 0xD6){//Zero page X
			value2 = read(cpu->PC_reg + 1);
			value1 = read((value2 + cpu->X_reg)&0xFF);
			value1--;
			write((value2 + cpu->X_reg)&0xFF, value1);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0xCE){//Absolute
			value1 = get_absolute(cpu, 0, read) - 1;
			set_absolute(cpu, 0, read, write, value1);
			cpu->PC_reg += 3;
			cpu->cycles += 6;
		} else if(opcode == 0xDE){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read) - 1;
			set_absolute(cpu, cpu->X_reg, read, write, value1);
			cpu->PC_reg += 3;
			cpu->cycles += 7;
		}

		//Set the zero flag
		if(!value1){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value1&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//DEX
	} else if(opcode == 0xCA){
		cpu->X_reg--;
		cpu->PC_reg += 1;
		cpu->cycles += 2;

		//Set the zero flag
		if(!cpu->X_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->X_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//DEY
	} else if(opcode == 0x88){
		cpu->Y_reg--;
		cpu->PC_reg += 1;
		cpu->cycles += 2;

		//Set the zero flag
		if(!cpu->Y_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->Y_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//EOR
	} else if(opcode == 0x49 || opcode == 0x45 || opcode == 0x55 || opcode == 0x4D || opcode == 0x5D || opcode == 0x59 || opcode == 0x41 || opcode == 0x51){
		if(opcode == 0x49){//Immediate
			value1 = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0x45){//Zero page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0x55){//Zero page X
			value1 = read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0x4D){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0x5D){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0x59){//Absolute Y
			value1 = get_absolute(cpu, cpu->Y_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0x41){//Indirect X
			value1 = get_indirect_X(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0x51){//Indirect Y
			value1 = get_indirect_Y(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			if(CROSSED_PAGE){
				cpu->cycles += 6;
			} else {
				cpu->cycles += 5;
			}
		}

		cpu->A_reg ^= value1;

		//Set the zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//INC
	} else if(opcode == 0xE6 || opcode == 0xF6 || opcode == 0xEE || opcode == 0xFE){
		if(opcode == 0xE6){//Zero page
			value2 = read(cpu->PC_reg + 1);
			value1 = read(value2);
			value1++;
			write(value2, value1);
			cpu->PC_reg += 2;
			cpu->cycles += 5;
		} else if(opcode == 0xF6){//Zero page X
			value2 = read(cpu->PC_reg + 1);
			value1 = read((value2 + cpu->X_reg)&0xFF);
			value1++;
			write((value2 + cpu->X_reg)&0xFF, value1);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0xEE){//Absolute
			value1 = get_absolute(cpu, 0, read) + 1;
			set_absolute(cpu, 0, read, write, value1);
			cpu->PC_reg += 3;
			cpu->cycles += 6;
		} else if(opcode == 0xFE){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read) + 1;
			set_absolute(cpu, cpu->X_reg, read, write, value1);
			cpu->PC_reg += 3;
			cpu->cycles += 7;
		}

		//Set the zero flag
		if(!value1){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value1&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//INX
	} else if(opcode == 0xE8){
		cpu->X_reg++;
		cpu->PC_reg += 1;
		cpu->cycles += 2;

		//Set the zero flag
		if(!cpu->X_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->X_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//INY
	} else if(opcode == 0xC8){
		cpu->Y_reg++;
		cpu->PC_reg += 1;
		cpu->cycles += 2;

		//Set the zero flag
		if(!cpu->Y_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->Y_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//JMP
	} else if(opcode == 0x4C || opcode == 0x6C){
		if(opcode == 0x4C){//Absolute
			cpu->PC_reg = ((uint16_t) read(cpu->PC_reg + 1)) | (((uint16_t) read(cpu->PC_reg + 2))<<8);
			cpu->cycles += 3;
		} else if(opcode == 0x6C){//Indirect
			value_absolute = ((uint16_t) read(cpu->PC_reg + 1)) | (((uint16_t) read(cpu->PC_reg + 2))<<8);
			value1 = read(value_absolute);
			value2 = read((value_absolute&0xFF00) | ((value_absolute + 1)&0xFF));
			cpu->PC_reg = (((uint16_t) value2)<<8) | value1;
			cpu->cycles += 5;
		}
	//JSR
	} else if(opcode == 0x20){
		push(cpu, write, (cpu->PC_reg + 2)&0xFF);
		push(cpu, write, (cpu->PC_reg + 2)>>8);
		cpu->PC_reg = ((uint16_t) read(cpu->PC_reg + 1)) | (((uint16_t) read(cpu->PC_reg + 2))<<8);
		cpu->cycles += 6;
	//LDA
	} else if(opcode == 0xA9 || opcode == 0xA5 || opcode == 0xB5 || opcode == 0xAD || opcode == 0xBD || opcode == 0xB9 || opcode == 0xA1 || opcode == 0xB1){
		if(opcode == 0xA9){//Immediate
			cpu->A_reg = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0xA5){//Zero page
			cpu->A_reg = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0xB5){//Zero page X
			cpu->A_reg = read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0xAD){//Absolute
			cpu->A_reg = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0xBD){//Absolute X
			cpu->A_reg = get_absolute(cpu, cpu->X_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0xB9){//Absolute Y
			cpu->A_reg = get_absolute(cpu, cpu->Y_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0xA1){//Indirect X
			cpu->A_reg = get_indirect_X(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0xB1){//Indirect Y
			cpu->A_reg = get_indirect_Y(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			if(CROSSED_PAGE){
				cpu->cycles += 6;
			} else {
				cpu->cycles += 5;
			}
		}

		//Set the zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//LDX
	} else if(opcode == 0xA2 || opcode == 0xA6 || opcode == 0xB6 || opcode == 0xAE || opcode == 0xBE){
		if(opcode == 0xA2){//Immediate
			cpu->X_reg = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0xA6){//Zero page
			cpu->X_reg = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0xB6){//Zero page Y
			cpu->X_reg = read((read(cpu->PC_reg + 1) + cpu->Y_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0xAE){//Absolute
			cpu->X_reg = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0xBE){//Absolute Y
			cpu->X_reg = get_absolute(cpu, cpu->Y_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		}

		//Set the zero flag
		if(!cpu->X_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->X_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//LDY
	} else if(opcode == 0xA0 || opcode == 0xA4 || opcode == 0xB4 || opcode == 0xAC || opcode == 0xBC){
		if(opcode == 0xA0){//Immediate
			cpu->Y_reg = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0xA4){//Zero page
			cpu->Y_reg = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0xB4){//Zero page X
			cpu->Y_reg = read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0xAC){//Absolute
			cpu->Y_reg = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0xBC){//Absolute X
			cpu->Y_reg = get_absolute(cpu, cpu->X_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		}

		//Set the zero flag
		if(!cpu->Y_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->Y_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//LSR
	} else if(opcode == 0x4A || opcode == 0x46 || opcode == 0x56 || opcode == 0x4E || opcode == 0x5E){
		if(opcode == 0x4A){//Accumulator
			value1 = cpu->A_reg;
			cpu->A_reg >>= 1;
			cpu->PC_reg++;
			cpu->cycles += 2;
		} else if(opcode == 0x46){//Zero page
			value2 = read(cpu->PC_reg + 1);
			value1 = read(value2);
			write(value2, value1>>1);
			cpu->PC_reg += 2;
			cpu->cycles += 5;
		} else if(opcode == 0x56){//Zero page X
			value2 = read(cpu->PC_reg + 1);
			value1 = read((value2 + cpu->X_reg)&0xFF);
			write((value2 + cpu->X_reg)&0xFF, value1>>1);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0x4E){//Absolute
			value1 = get_absolute(cpu, 0, read);
			set_absolute(cpu, 0, read, write, value1>>1);
			cpu->PC_reg += 3;
			cpu->cycles += 6;
		} else if(opcode == 0x5E){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			set_absolute(cpu, cpu->X_reg, read, write, value1>>1);
			cpu->PC_reg += 3;
			cpu->cycles += 7;
		}

		//Set the carry flag
		if(value1&0x1){
			cpu->P_reg |= 1<<CARRY;
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}

		//Set the zero flag
		if(!(value1>>1)){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//The negative flag is always set to zero
		cpu->P_reg &= ~(1<<NEGATIVE);
	//ORA
	} else if(opcode == 0x09 || opcode == 0x05 || opcode == 0x15 || opcode == 0x0D || opcode == 0x1D || opcode == 0x19 || opcode == 0x01 || opcode == 0x11){
		if(opcode == 0x09){//Immediate
			cpu->A_reg |= read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0x05){//Zero page
			cpu->A_reg |= read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0x15){//Zero page X
			cpu->A_reg |= read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0x0D){//Absolute
			cpu->A_reg |= get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0x1D){//Absolute X
			cpu->A_reg |= get_absolute(cpu, cpu->X_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0x19){//Absolute Y
			cpu->A_reg |= get_absolute(cpu, cpu->Y_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0x01){//Indirect X
			cpu->A_reg |= get_indirect_X(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0x11){//Indirect Y
			cpu->A_reg |= get_indirect_Y(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			if(CROSSED_PAGE){
				cpu->cycles += 6;
			} else {
				cpu->cycles += 5;
			}
		}

		//Set zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//PHA
	} else if(opcode == 0x48){
		push(cpu, write, cpu->A_reg);
		cpu->PC_reg += 1;
		cpu->cycles += 3;
	//PHP (sucks)
	} else if(opcode == 0x08){
		push(cpu, write, cpu->P_reg);
		cpu->PC_reg += 1;
		cpu->cycles += 3;
	//PLA
	} else if(opcode == 0x68){
		cpu->A_reg = pop(cpu, read);
		cpu->PC_reg += 1;
		cpu->cycles += 4;

		//Set the zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}
		
		//Set the negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//PLP
	} else if(opcode == 0x28){
		cpu->P_reg = pop(cpu, read);
		cpu->PC_reg += 1;
		cpu->cycles += 4;
	//ROL
	} else if(opcode == 0x2A || opcode == 0x26 || opcode == 0x36 || opcode == 0x2E || opcode == 0x3E){
		//Determine whether to append a 1 or 0 for bit 0
		if(cpu->P_reg&(1<<CARRY)){
			value2 = 1;
		} else {
			value2 = 0;
		}
		if(opcode == 0x2A){//Accumulator
			value1 = cpu->A_reg;
			cpu->A_reg = (value1<<1) | value2;
			cpu->PC_reg += 1;
			cpu->cycles += 2;
		} else if(opcode == 0x26){//Zero page
			value3 = read(cpu->PC_reg + 1);
			value1 = read(value3);
			write(value3, (value1<<1) | value2);
			cpu->PC_reg += 2;
			cpu->cycles += 5;
		} else if(opcode == 0x36){//Zero page X
			value3 = read(cpu->PC_reg + 1);
			value1 = read((value3 + cpu->X_reg)&0xFF);
			write((value3 + cpu->X_reg)&0xFF, (value1<<1) | value2);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0x2E){//Absolute
			value1 = get_absolute(cpu, 0, read);
			set_absolute(cpu, 0, read, write, (value1<<1) | value2);
			cpu->PC_reg += 3;
			cpu->cycles += 6;
		} else if(opcode == 0x3E){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			set_absolute(cpu, cpu->X_reg, read, write, (value1<<1) | value2);
			cpu->PC_reg += 3;
			cpu->cycles += 7;
		}

		value2 = (value1<<1) | value2;
		//value1 stores the value before
		//value2 stores the value after

		//Set the carry flag
		if(value1&0x80){
			cpu->P_reg |= 1<<CARRY;
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}

		//Set the zero flag
		if(!value2){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value2&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//ROR
	} else if(opcode == 0x6A || opcode == 0x66 || opcode == 0x76 || opcode == 0x6E || opcode == 0x7E){
		//Determine whether to append a 1 or 0 for bit 7
		if(cpu->P_reg&(1<<CARRY)){
			value2 = 0x80;
		} else {
			value2 = 0;
		}
		if(opcode == 0x6A){//Accumulator
			value1 = cpu->A_reg;
			cpu->A_reg = (value1>>1) | value2;
			cpu->PC_reg += 1;
			cpu->cycles += 2;
		} else if(opcode == 0x66){//Zero page
			value3 = read(cpu->PC_reg + 1);
			value1 = read(value3);
			write(value3, (value1>>1) | value2);
			cpu->PC_reg += 2;
			cpu->cycles += 5;
		} else if(opcode == 0x76){//Zero page X
			value3 = read(cpu->PC_reg + 1);
			value1 = read((value3 + cpu->X_reg)&0xFF);
			write((value3 + cpu->X_reg)&0xFF, (value1>>1) | value2);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0x6E){//Absolute
			value1 = get_absolute(cpu, 0, read);
			set_absolute(cpu, 0, read, write, (value1>>1) | value2);
			cpu->PC_reg += 3;
			cpu->cycles += 6;
		} else if(opcode == 0x7E){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			set_absolute(cpu, cpu->X_reg, read, write, (value1>>1) | value2);
			cpu->PC_reg += 3;
			cpu->cycles += 7;
		}

		value2 = (value1>>1) | value2;
		//value1 stores the value before
		//value2 stores the value after

		//Set the carry flag
		if(value1&1){
			cpu->P_reg |= 1<<CARRY;
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}

		//Set the zero flag
		if(!value2){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(value2&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//RTI
	} else if(opcode == 0x40){
		cpu->P_reg = pop(cpu, read);
		value1 = pop(cpu, read);
		value2 = pop(cpu, read);
		cpu->PC_reg = (((uint16_t) value1)<<8) | value2;
		cpu->cycles += 6;
	//RTS
	} else if(opcode == 0x60){
		value1 = pop(cpu, read);
		value2 = pop(cpu, read);
		cpu->PC_reg = (((uint16_t) value1)<<8) | value2;
		cpu->PC_reg++;
		cpu->cycles += 6;
	//SBC
	} else if(opcode == 0xE9 || opcode == 0xE5 || opcode == 0xF5 || opcode == 0xED || opcode == 0xFD || opcode == 0xF9 || opcode == 0xE1 || opcode == 0xF1){
		if(opcode == 0xE9){//Immediate
			value1 = read(cpu->PC_reg + 1);
			cpu->PC_reg += 2;
			cpu->cycles += 2;
		} else if(opcode == 0xE5){//Zero page
			value1 = read(read(cpu->PC_reg + 1));
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0xF5){//Zero page X
			value1 = read((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0xED){//Absolute
			value1 = get_absolute(cpu, 0, read);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0xFD){//Absolute X
			value1 = get_absolute(cpu, cpu->X_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0xF9){//Absolute Y
			value1 = get_absolute(cpu, cpu->Y_reg, read);
			cpu->PC_reg += 3;
			if(CROSSED_PAGE){
				cpu->cycles += 5;
			} else {
				cpu->cycles += 4;
			}
		} else if(opcode == 0xE1){//Indirect X
			value1 = get_indirect_X(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0xF1){//Indirect Y
			value1 = get_indirect_Y(cpu, read(cpu->PC_reg + 1), read);
			cpu->PC_reg += 2;
			if(CROSSED_PAGE){
				cpu->cycles += 6;
			} else {
				cpu->cycles += 5;
			}
		}

		value2 = cpu->A_reg;
		value3 = (~value1);//One's complement
		if(cpu->P_reg&(1<<CARRY)){
			value3++;
			cpu->A_reg += value3;
		} else {
			cpu->A_reg += value3;
		}

		//If the CPU is in decimal state, correct output
		if((cpu->P_reg&(1<<DECIMAL)) && (cpu->A_reg&0xF) > 9){
			cpu->A_reg -= 6;
		}

		//Set the overflow flag
		if(((value2&0x80) && (value3&0x80) && !(cpu->A_reg&0x80)) || (!(value2&0x80) && !(value3&0x80) && (cpu->A_reg&0x80))){
			cpu->P_reg |= 1<<OVERFLOW;
		} else {
			cpu->P_reg &= ~(1<<OVERFLOW);
		}

		//Set the carry flag
		if(cpu->A_reg <= value2){
			cpu->P_reg |= 1<<CARRY;
		} else {
			cpu->P_reg &= ~(1<<CARRY);
		}
		
		//Set the zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//SEC
	} else if(opcode == 0x38){
		cpu->P_reg |= 1<<CARRY;
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//SED
	} else if(opcode == 0xF8){
		cpu->P_reg |= 1<<DECIMAL;
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//SEI
	} else if(opcode == 0x78){
		cpu->P_reg |= 1<<INTERRUPT;
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//STA
	} else if(opcode == 0x85 || opcode == 0x95 || opcode == 0x8D || opcode == 0x9D || opcode == 0x99 || opcode == 0x81 || opcode == 0x91){
		if(opcode == 0x85){//Zero page
			write(read(cpu->PC_reg + 1), cpu->A_reg);
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0x95){//Zero page X
			write((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF, cpu->A_reg);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0x8D){//Absolute
			set_absolute(cpu, 0, read, write, cpu->A_reg);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		} else if(opcode == 0x9D){//Absolute X
			set_absolute(cpu, cpu->X_reg, read, write, cpu->A_reg);
			cpu->PC_reg += 3;
			cpu->cycles += 5;
		} else if(opcode == 0x99){//Absolute Y
			set_absolute(cpu, cpu->Y_reg, read, write, cpu->A_reg);
			cpu->PC_reg += 3;
			cpu->cycles += 5;
		} else if(opcode == 0x81){//Indirect X
			set_indirect_X(cpu, read(cpu->PC_reg + 1), read, write, cpu->A_reg);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		} else if(opcode == 0x91){//Indirect Y
			set_indirect_Y(cpu, read(cpu->PC_reg + 1), read, write, cpu->A_reg);
			cpu->PC_reg += 2;
			cpu->cycles += 6;
		}
	//STX
	} else if(opcode == 0x86 || opcode == 0x96 || opcode == 0x8E){
		if(opcode == 0x86){//Zero page
			write(read(cpu->PC_reg + 1), cpu->X_reg);
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0x96){//Zero page Y
			write((read(cpu->PC_reg + 1) + cpu->Y_reg)&0xFF, cpu->X_reg);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0x8E){//Absolute
			set_absolute(cpu, 0, read, write, cpu->X_reg);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		}
	//STY
	} else if(opcode == 0x84 || opcode == 0x94 || opcode == 0x8C){
		if(opcode == 0x84){//Zero page
			write(read(cpu->PC_reg + 1), cpu->Y_reg);
			cpu->PC_reg += 2;
			cpu->cycles += 3;
		} else if(opcode == 0x94){//Zero page X
			write((read(cpu->PC_reg + 1) + cpu->X_reg)&0xFF, cpu->Y_reg);
			cpu->PC_reg += 2;
			cpu->cycles += 4;
		} else if(opcode == 0x8C){//Absolute
			set_absolute(cpu, 0, read, write, cpu->Y_reg);
			cpu->PC_reg += 3;
			cpu->cycles += 4;
		}
	//TAX
	} else if(opcode == 0xAA){
		cpu->X_reg = cpu->A_reg;
		cpu->PC_reg += 1;
		cpu->cycles += 2;
		
		//Set the zero flag
		if(!cpu->X_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->X_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//TAY
	} else if(opcode == 0xA8){
		cpu->Y_reg = cpu->A_reg;
		cpu->PC_reg += 1;
		cpu->cycles += 2;

		//Set the zero flag
		if(!cpu->Y_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->Y_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//TSX
	} else if(opcode == 0xBA){
		cpu->X_reg = cpu->SP_reg;
		cpu->PC_reg += 1;
		cpu->cycles += 2;

		//Set the zero flag
		if(!cpu->X_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->X_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//TXA
	} else if(opcode == 0x8A){
		cpu->A_reg = cpu->X_reg;
		cpu->PC_reg += 1;
		cpu->cycles += 2;

		//Set the zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//TXS
	} else if(opcode == 0x9A){
		cpu->SP_reg = cpu->X_reg;
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//TYA
	} else if(opcode == 0x98){
		cpu->A_reg = cpu->Y_reg;
		cpu->PC_reg += 1;
		cpu->cycles += 2;
		
		//Set the zero flag
		if(!cpu->A_reg){
			cpu->P_reg |= 1<<ZERO;
		} else {
			cpu->P_reg &= ~(1<<ZERO);
		}

		//Set the negative flag
		if(cpu->A_reg&0x80){
			cpu->P_reg |= 1<<NEGATIVE;
		} else {
			cpu->P_reg &= ~(1<<NEGATIVE);
		}
	//NOP
	} else if(opcode == 0xEA){
		cpu->PC_reg += 1;
		cpu->cycles += 2;
	//Unknown operation
	} else {
		printf("Error: Unknown operation 0x%x\n", (int) opcode);
		exit(1);
	}
}

void reset_6502(CPU_6502 *cpu, uint8_t (*read)(uint16_t)){
	cpu->SP_reg -= 3;//This actually happens on the chip
	cpu->PC_reg = ((uint16_t) read(0xFFFD))<<8 | read(0xFFFC);
}

