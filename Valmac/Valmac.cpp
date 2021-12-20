// Valmac8.cpp : This file contains the 'main' function. Program execution begins and ends there.
// Emulation execution is separate, mostly controller by PC.
//
// NB students may be more familiar with uint16_t than unsigned short
// similarly   may be more familiar with uint8_t  than unsigned char
//

#include <iostream>
#include <ctime>

#include <SFML/Graphics.hpp>

#define MEMORY_SIZE	(4096)


struct Valmac
{
	//A boolean that can be referenced to check if the emulator is currently running.
	bool simulating{ false };

	//The Valmac has around 35 opcodes which are all two bytes long. 
	uint16_t opcode{ 0 };
	
	////4K memory in total
	// NB 1K is 1024 bytes (not 1000)
	uint8_t memory[MEMORY_SIZE];
	//unsigned short* opcode_memory = (unsigned short*)memory;

	//Returns the value stored at a given memory address
	inline uint8_t get_mem(unsigned int p_address) {
		_ASSERT(p_address < MEMORY_SIZE);
		return memory[p_address];
	}

	//Sets the value at a given memory address
	inline void set_mem(unsigned int p_address, uint8_t p_value) {
		_ASSERT(p_address < MEMORY_SIZE);
		memory[p_address] = p_value;
	}

	//Loops through memory and sets all values in it to 0
	inline void clear_mem() {
		for (size_t i = 0; i < MEMORY_SIZE; i++)
		{
			memory[i] = 0x00;
		}

		std::cout << "Memory reset" << std::endl;
	}

	//Loads the values for the font into memory
	inline void load_fontset() {
		for (int i = 0; i < 85; i++) {
			memory[i] = Valmac_fontset[i];
		}
	}

	//Sets all registers to 0
	inline void clear_registers() {
		for (size_t i = 0; i < 16; i++)
		{
			V[i] = 0;
		}
	}

	//Sets all stack values to 0 and resets the stack pointer to 0
	inline void clear_stack() {
		for (size_t i = 0; i < 16; i++)
		{
			stack[i] = 0;
		}

		SP = 0;
	}

	//Sets every item in the keypad to false so no key is recognized as pressed
	inline void clear_keypad() {
		for (size_t i = 0; i < 16; i++)
		{
			key[i] = false;
		}
	}

	// CPU registers :  15, 8 - bit general purpose registers named V0, V1 up to VE.
	// The 16th register, Vf, is used  for the ‘carry flag
	uint8_t  V[16]{ 0 };

	//There is an Index register I and a program counter (PC) which can have a value from 0x000 to 0xFFF
	uint16_t I{ 0 };
	uint16_t PC{ 0 };
	//The systems memory map :
	// 0x000 - 0x1FF - interpreter (contains font set in emu)
	// 0x050 - 0x0A0 - Used for the built in 4x5 pixel font set(0 - F)
	// 0x200 - 0xFFF - Program ROM and work RAM

	//The graphics system : There is one instruction that draws sprite to the screen. Drawing is done in XOR mode 
	//and if a pixel is turned off as a result of drawing, the VF register is set. 
	//This is used for collision detection.
	//The graphics are black and white and the screen has a total of 2048 pixels(64 x 32).
	//This can easily be implemented using an array that hold the pixel state(1 or 0) :
	uint8_t gfx[32][64]{ 0 };

	//The fontset to be used to represent the hex digits 0 to F
	//This will be loaded in the interpreter area of memory.

	uint8_t Valmac_fontset[85]{
		0xF0, 0x90, 0x90, 0x90, 0xF0, //0
		0x20, 0x60, 0x20, 0x20, 0x70, //1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
		0x90, 0x90, 0xF0, 0x10, 0x10, //4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
		0xF0, 0x10, 0x20, 0x40, 0x40, //7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
		0xF0, 0x90, 0xF0, 0x90, 0x90, //A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
		0xF0, 0x80, 0x80, 0x80, 0xF0, //C
		0xE0, 0x90, 0x90, 0x90, 0xE0, //D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
		0xF0, 0x80, 0xF0, 0x80, 0x80, //F
		0x80, 0x00, 0x00, 0x00, 0x00  //Single Pixel
	};

	//no Interupts or hardware registers.
	//but there are two timer registers that count at 60 Hz.
	//When set above zero they will count down to zero.
	uint8_t delay_timer{ 0 };
	uint8_t sound_timer{ 0 };

	/*
	It is important to know that the instruction set has opcodes that allow the program to jump
	to a certain address or call a subroutine. While the specification don’t mention a stack, you will need
	to implement one as part of the interpreter yourself. The stack is used to remember the current location
	before a jump is performed. So anytime you perform a jump or call a subroutine, store the program counter
	in the stack before proceeding. The system has 16 levels of stack and in order to remember which level of
	the stack is used, you need to implement a stack pointer (SP).
	*/
	uint16_t stack[16];
	uint16_t SP{ 0 };

	//Finally, the HEX based keypad(0x0 - 0xF), you can use an array to store the 
	//current state of the key.
	bool key[16]{ 0 };
	/*
		1	2	3	C
		4	5	6	D
		7	8	9	E
		A	0	B	F
	*/

	//Loads each opcode from a given program into memory. Each opcode is spread between two slots in memory
	bool load_program(uint16_t* pProgram, size_t bufferSize) {
		std::memcpy(memory+PC, pProgram, bufferSize);

		return true;
	}

	//Gets the opcode at the address held in the program counter
	//Does this by taking the value at memory[PC + 1] and peforming an 8 place left shift on it
	//It then peforms an OR bitwise operation between it and memory[PC] to combine them
	uint16_t get_program_opcode() {
		_ASSERT((PC & 1) == 0);
		_ASSERT(PC <= MEMORY_SIZE - sizeof(opcode));

		uint16_t l_opcode = memory[PC + 1] << 8 | memory[PC];

		return l_opcode;
	}

	//Steps the PC forward to point to the next opcode
	inline void step_PC() {
		PC += 2;
		_ASSERT(PC <= MEMORY_SIZE - sizeof(opcode));
	}

	//Uses XOR to toggle values in gfx which represent pixels on the screen
	void set_pixel(uint8_t x, uint8_t y, uint8_t pixelValue) {
		if (x >= 64) {
			while (x >= 64) {
				x -= 64;
			}
		}

		if (y >= 32) {
			while (y >= 32) {
				y -= 32;
			}
		}
		std::cout << "X: " << (unsigned)x << "Y: " << (unsigned)y << std::endl;
		std::cout << (unsigned)pixelValue << std::endl;
		if ((gfx[y][x] ^ pixelValue) == 0) {
			V[15] = 1;
		}
		else {
			V[15] = 0;
		}

		gfx[y][x] = gfx[y][x] ^ pixelValue;

		std::cout << (unsigned)gfx[y][x] << std::endl;
	}

	//Loops through gfx and draws an sfml rectangle for each true value
	void draw(sf::RenderWindow& w) {
		for (size_t i = 0; i < 32; i++)
		{
			for (size_t j = 0; j < 64; j++)
			{
				if (gfx[i][j] != 0) {
					sf::RectangleShape r(sf::Vector2f(20, 20));
					sf::Vector2f pos = sf::Vector2f((float)j * 20, (float)i * 20);
					//std::cout << "X: " << pos.x << "Y: " << pos.y << std::endl;
					r.setPosition(pos);
					r.setFillColor(sf::Color::White);
					w.draw(r);
				}
			}
		}

		w.display();
	}

	//Used to toggle values in the keypad on and off to figure out which buttons are being pressed
	//Can press Space to output the keypad to the console window
	void sample_input(sf::RenderWindow& window) {
		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::KeyPressed:
				switch (event.key.code)
				{
				case sf::Keyboard::Num1:
					key[1] = true;
					break;
				case sf::Keyboard::Num2:
					key[2] = true;
					break;
				case sf::Keyboard::Num3:
					key[3] = true;
					break;
				case sf::Keyboard::Num4:
					key[12] = true;
					break;
				case sf::Keyboard::Q:
					key[4] = true;
					break;
				case sf::Keyboard::W:
					key[5] = true;
					break;
				case sf::Keyboard::E:
					key[6] = true;
					break;
				case sf::Keyboard::R:
					key[13] = true;
					break;
				case sf::Keyboard::A:
					key[7] = true;
					break;
				case sf::Keyboard::S:
					key[8] = true;
					break;
				case sf::Keyboard::D:
					key[9] = true;
					break;
				case sf::Keyboard::F:
					key[14] = true;
					break;
				case sf::Keyboard::Z:
					key[10] = true;
					break;
				case sf::Keyboard::X:
					key[0] = true;
					break;
				case sf::Keyboard::C:
					key[11] = true;
					break;
				case sf::Keyboard::V:
					key[15] = true;
					break;
				default:
					break;
				}
				break;
			case sf::Event::KeyReleased:
				switch (event.key.code)
				{
				case sf::Keyboard::Num1:
					key[1] = false;
					break;
				case sf::Keyboard::Num2:
					key[2] = false;
					break;
				case sf::Keyboard::Num3:
					key[3] = false;
					break;
				case sf::Keyboard::Num4:
					key[12] = false;
					break;
				case sf::Keyboard::Q:
					key[4] = false;
					break;
				case sf::Keyboard::W:
					key[5] = false;
					break;
				case sf::Keyboard::E:
					key[6] = false;
					break;
				case sf::Keyboard::R:
					key[13] = false;
					break;
				case sf::Keyboard::A:
					key[7] = false;
					break;
				case sf::Keyboard::S:
					key[8] = false;
					break;
				case sf::Keyboard::D:
					key[9] = false;
					break;
				case sf::Keyboard::F:
					key[14] = false;
					break;
				case sf::Keyboard::Z:
					key[10] = false;
					break;
				case sf::Keyboard::X:
					key[0] = false;
					break;
				case sf::Keyboard::C:
					key[11] = false;
					break;
				case sf::Keyboard::V:
					key[15] = false;
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}
	}

	void initialize()
	{
		// Initialize registers and memory once
		PC = 0x200;  // Program counter starts at 0x200
		opcode = 0;      // Reset current opcode	
		I = 0;      // Reset index register
		SP = 0;      // Reset stack pointer

		simulating = true;  //Resets 'simulating' to show the emulator is running

		// Clear display	
		for (size_t i = 0; i < 32; i++)
		{
			for (size_t j = 0; j < 64; j++)
			{
				gfx[i][j] = 0;
			}
		}
		// Clear stack
		clear_stack();
		// Clear registers V0-VF
		clear_registers();
		// Clear keypad
		clear_keypad();
		// Clear memory
		clear_mem();

		//// Load fontset
		load_fontset();

		// Reset timers	
		delay_timer = 0;
		sound_timer = 0;
	}

	void emulateCycle(sf::RenderWindow& w)
	{
		// Fetch Opcode
		opcode = get_program_opcode();
		std::cout << std::hex << (unsigned)opcode << std::endl;

		// Decode Opcode
		switch (opcode & 0xF000)
		{
		case 0x0000:
			if (opcode == 0) { //0x0000 Spacer
				step_PC();
				std::cout << "Space" << std::endl;
				break;
			}
			else if (opcode == 0x00EE) { //Return opcode
				std::cout << "Return" << std::endl;
				if (SP == 0) {
					std::cout << "Return failed" << std::endl;
					step_PC();
					break;
				}
				PC = stack[--SP];
				stack[SP + 1] = 0;
				step_PC();
				break;
			}
			else if (opcode == 0x00E0) { //Clears the screen
				w.clear();
				for (size_t i = 0; i < 32; i++)
				{
					for (size_t j = 0; j < 64; j++)
					{
						gfx[i][j] = 0;
					}
				}
				step_PC();
				break;
			}
			else if (opcode == 0x00FE) { //Ends the program
				w.close();
				simulating = false;
				break;
			}
			step_PC();
			std::cout << "Invalid Opcode" << std::endl;
			break;
		case 0x1000: //1NNN Jumps to given address
			PC = opcode & 0x0FFF;
			std::cout << (unsigned)PC << std::endl;
			break;
		case 0x2000: //2NNN Call subroutine at given address
			if ((opcode & 0x0FFF) < MEMORY_SIZE) {
				stack[SP] = PC;
				PC = opcode & 0x0FFF;
				SP++;
			}
			break;
		case 0x3000: //3XNN skips if(Vx == NN)
			if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)) {
				std::cout << "We are the same" << std::endl;
				step_PC();
				step_PC();
			}
			else {
				std::cout << "We are not the same" << std::endl;
				step_PC();
			}
			break;
		case 0x4000: //4XNN skips if(Vx != NN)
			if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF)) {
				std::cout << "We are not the same" << std::endl;
				step_PC();
				step_PC();
			}
			else {
				std::cout << "We are the same" << std::endl;
				step_PC();
			}
			break;
		case 0x5000: //5XY0 skips if(Vx == Vy)
			if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4]) {
				std::cout << "We are the same" << std::endl;
				step_PC();
				step_PC();
			}
			else {
				std::cout << "We are not the same" << std::endl;
				step_PC();
			}
			break;
		case 0x6000: //6XNN Sets Vx to NN
			std::cout << std::hex << (unsigned)V[(opcode & 0x0F00) >> 8] << std::endl;
			V[(opcode & 0x0F00) >> 8] = (opcode & 0x00FF);
			std::cout << std::hex << (unsigned)V[(opcode & 0x0F00) >> 8] << std::endl;
			step_PC();
			break;
		case 0x7000: //7XNN Adds given value to Vx
			V[(opcode & 0x0F00) >> 8] += (opcode & 0x00FF);
			step_PC();
			break;
		case 0x8000:
			switch (opcode & 0x000F)
			{
			case 0x0000:
				//8XY0
				V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
				break;
			case 0x0001:
				//8XY1
				V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] | V[(opcode & 0x00F0) >> 4];
				break;
			case 0x0002:
				//8XY2
				V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] & V[(opcode & 0x00F0) >> 4];
				break;
			case 0x0003:
				//8XY3
				V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] ^ V[(opcode & 0x00F0) >> 4];
				break;
			case 0x0004:
				//8XY4
				if (V[(opcode & 0x00F0) >> 4] > 255 - V[(opcode & 0x0F00) >> 8]) {
					V[15] = 1;
				}
				else {
					V[15] = 0;
				}

				V[(opcode & 0x0F00) >> 8] += V[(opcode & 0x00F0) >> 4];
				std::cout << "Addition Result: " << std::hex << (unsigned)V[(opcode & 0x0F00) >> 8] << "   " << (unsigned)V[15] << std::endl;
				break;
			case 0x0005:
				//8XY5
				if (V[(opcode & 0x0F00) >> 8] < V[(opcode & 0x00F0) >> 4]) {
					V[15] = 0;
				}
				else {
					V[15] = 1;
				}
				V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];
				std::cout << "Subtraction Result: " << std::hex << (unsigned)V[(opcode & 0x0F00) >> 8] << "   " << (unsigned)V[15] << std::endl;
				break;
			case 0x0006:
				//8X06
				V[15] = (V[(opcode & 0x0F00) >> 8] % 2);
				V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] >> 1;
				std::cout << (unsigned)V[15] << std::endl;
				break;
			case 0x0007:
				//8XY7
				if (V[(opcode & 0x00F0) >> 4] < V[(opcode & 0x0F00) >> 8]) {
					V[15] = 0;
				}
				else {
					V[15] = 1;
				}
				V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
				break;
			case 0x000E:
				//8X0E
				std::cout << "Left Shift" << std::endl;
				std::cout << (int)std::log2(V[(opcode & 0x0F00) >> 8]) + 1 << std::endl;
				if ((int)std::log2(V[(opcode & 0x0F00) >> 8]) + 1 < 8) {
					std::cout << "Less than 8 bits" << std::endl;
					V[15] = 0;
				}
				else {
					V[15] = (V[(opcode & 0x0F00) >> 8] >> 7);
				}
				V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] << 1;
				std::cout << (unsigned)V[(opcode & 0x0F00) >> 8] << std::endl;
				std::cout << (unsigned)V[15] << std::endl;
				break;
			default:
				std::cout << "Error: Illegal Opcode" << std::endl;
				break;
			}
			step_PC();
			break;
		case 0x9000: //9XY0 skips if(Vx != Vy)
			if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4]) {
				step_PC();
			}
			step_PC();
			break;
		case 0xA000: //ANNN Sets I to given address
			I = (opcode & 0x0FFF);
			step_PC();
			break;
		case 0xB000: //BNNN Sets PC to V0 + NNN
			PC = V[0] + (opcode & 0x0FFF);
			step_PC();
			break;
		case 0xC000: //CXNN Sets Vx to the result of a bitwise AND between NN and a random number
			V[(opcode & 0x0F00) >> 8] = ((rand() % 256) & (opcode & 0x00FF));
			step_PC();
			break;
		case 0xD000: //DXYN Draws a sprite at position X Y with a height of N + 1 and a width of 8
			std::cout << "Draw" << std::endl;
			for (int i = 0; i <= (opcode & 0x000F); i++)
			{
				std::cout << "Drawing row " << i << std::endl;
				set_pixel((V[(opcode & 0x0F00) >> 8]), (V[(opcode & 0x00F0) >> 4]) + i, ((memory[I + i] & 0b10000000) >> 7));
				set_pixel((V[(opcode & 0x0F00) >> 8]) + 0x01, (V[(opcode & 0x00F0) >> 4]) + i, ((memory[I + i] & 0b01000000) >> 6));
				set_pixel((V[(opcode & 0x0F00) >> 8]) + 0x02, (V[(opcode & 0x00F0) >> 4]) + i, ((memory[I + i] & 0b00100000) >> 5));
				set_pixel((V[(opcode & 0x0F00) >> 8]) + 0x03, (V[(opcode & 0x00F0) >> 4]) + i, ((memory[I + i] & 0b00010000) >> 4));
				set_pixel((V[(opcode & 0x0F00) >> 8]) + 0x04, (V[(opcode & 0x00F0) >> 4]) + i, ((memory[I + i] & 0b00001000) >> 3));
				set_pixel((V[(opcode & 0x0F00) >> 8]) + 0x05, (V[(opcode & 0x00F0) >> 4]) + i, ((memory[I + i] & 0b00000100) >> 2));
				set_pixel((V[(opcode & 0x0F00) >> 8]) + 0x06, (V[(opcode & 0x00F0) >> 4]) + i, ((memory[I + i] & 0b00000010) >> 1));
				set_pixel((V[(opcode & 0x0F00) >> 8]) + 0x07, (V[(opcode & 0x00F0) >> 4]) + i, (memory[I + i] & 0b00000001));
			}

			step_PC();
			break;
		case 0xE000:
			switch (opcode & 0x00FF) {
			case 0x009E: //EX9E
				if (key[V[(opcode & 0x0F00) >> 8]] == true) {
					std::cout << "Key Pressed" << std::endl;
					step_PC();
				}
				break;
			case 0x00A1: //EXA1
				if (key[V[(opcode & 0x0F00) >> 8]] == false) {
					std::cout << "Key Not Pressed" << std::endl;
					step_PC();
				}
				break;
			default:
				break;
			}

			step_PC();
			break;
		case 0xF000:
			if (opcode == 0xFF18) {
				std::cout << "\7" << std::endl;
				std::cout << "PING!" << std::endl;
			}

			switch (opcode & 0x00FF)
			{
			case 0x0007: //FX07
				V[(opcode & 0x0F00) >> 8] = delay_timer;
				break;
			case 0x000A: //FX0A
				do
				{
					sample_input(w);
					for (int i = 0; i < 16; i++)
					{
						if (key[i] == true) {
							V[(opcode & 0x0F00) >> 8] = i;
							std::cout << "Key Pressed: " << i << std::endl;
							break;
						}
					}
				} while (key[0] == false && key[1] == false && key[2] == false && key[3] == false && key[4] == false
					&& key[5] == false && key[6] == false && key[7] == false && key[8] == false && key[9] == false
					&& key[10] == false && key[11] == false && key[12] == false && key[13] == false && key[14] == false && key[15] == false);
				break;
			case 0x0015: //FX15
				delay_timer = V[(opcode & 0x0F00) >> 8];
				break;
			case 0x0018: //FX18
				sound_timer = V[(opcode & 0x0F00) >> 8];
				break;
			case 0x001E: //FX1E
				I += V[(opcode & 0x0F00) >> 8];
				std::cout << (unsigned)I << std::endl;
				break;
			case 0x0029: //FX29
				std::cout << "Sprite To Load: " << std::hex << (unsigned)V[(opcode & 0x0F00) >> 8] << std::endl;
				switch (V[(opcode & 0x0F00) >> 8])
				{
				case 0x0:
					I = 0x00;
					break;
				case 0x1:
					I = 0x05;
					break;
				case 0x2:
					I = 0x0A;
					break;
				case 0x3:
					I = 0x0F;
					break;
				case 0x4:
					I = 0x14;
					break;
				case 0x5:
					I = 0x19;
					break;
				case 0x6:
					I = 0x1E;
					break;
				case 0x7:
					I = 0x23;
					break;
				case 0x8:
					I = 0x28;
					break;
				case 0x9:
					I = 0x2D;
					break;
				case 0xA:
					I = 0x32;
					break;
				case 0xB:
					I = 0x37;
					break;
				case 0xC:
					I = 0x3C;
					break;
				case 0xD:
					I = 0x41;
					break;
				case 0xE:
					I = 0x46;
					break;
				case 0xF:
					I = 0x4B;
					break;
				case 0x10:
					I = 0x50;
				default:
					break;
				}
				break;
			case 0x0033: //FX33
				std::cout << "Converting" << std::endl;
				std::cout << (unsigned)V[(opcode & 0x0F00) >> 8] << std::endl;
				std::cout << V[(opcode & 0x0F00) >> 8] / 0x64 << std::endl;
				std::cout << (V[(opcode & 0x0F00) >> 8] % 0x64) / 0xA << std::endl;
				std::cout << ((V[(opcode & 0x0F00) >> 8] % 0x64) % 0xA) / 0x1 << std::endl;

				memory[I] = V[(opcode & 0x0F00) >> 8] / 100;
				memory[I + 1] = (V[(opcode & 0x0F00) >> 8] % 100) / 10;
				memory[I + 2] = ((V[(opcode & 0x0F00) >> 8] % 100) % 10) / 1;
				break;
			case 0x0055: //FX55
				for (size_t i = 0; i <= (opcode & 0x0F00) >> 8; i++)
				{
					if (I + i <= MEMORY_SIZE) {
						memory[I + i] = V[i];
						std::cout << "Memory[" << I + i << "] = " << (unsigned)memory[I + i] << std::endl;
					}
					else {
						std::cout << "Memory limit reached" << std::endl;
						break;
					}
				}
				break;
			case 0x0065: //FX65
				for (size_t i = 0; i <= (opcode & 0x0F00) >> 8; i++) {
					if (I + i <= MEMORY_SIZE) {
						V[i] = memory[I + i];
						std::cout << "V[" << i << "] = " << (unsigned)V[i] << std::endl;
					}
					else {
						std::cout << "Memory limit reached" << std::endl;
						break;
					}
				}
				break;
			default:
				std::cout << "Invalid Opcode" << std::endl;
				break;
			}

			step_PC();
			break;
		default:
			step_PC();
			std::cout << "Error: Illegal Opcode" << std::endl;
			break;
		}
		// Execute Opcode

		// Update timers
		if (delay_timer > 0) {
			std::cout << "Delayed" << std::endl;
			delay_timer--;
		}

		if (sound_timer > 0) {
			std::cout << "Sound played" << std::endl;
			std::cout << "\7" << std::endl;
			sound_timer--;
		}
	}
};

//#include   // OpenGL graphics and input
//#include "Valmac.h" // Your cpu core implementation

Valmac myValmac;

int main(int argc, char** argv)
{
	std::srand((unsigned int)time(NULL));
	//Short test program for the emulator
	//Lets the user enter a single hex value
	//The program will count from 0 to that number
	//The program renders the current number on screen
	uint16_t countingProgram[] = {
		0x630A, //Sets X position
		0x640A, //Sets Y position
		0xF10A, //Lets user input number to count to
		0x6200, //Sets V[2] to 0
		0xFF18, //PING
		0xF229, //Sets I to the position of the sprite for the current number
		0x00E0, //Clears screen
		0xD344, //Draws the sprite
		0x9120, //Skips if the target value isn't reached
		0x00FE, //Ends program
		0x7201, //Increments V[2]
		0x1208 //Jumps back to instruction 4
	};

	uint16_t randomNumber[] = {
		0xC10F, //Sets V[1] to random single digit hex value
		0xC20F, //Sets V[2] to random single digit hex value
		0x630A, //Sets X position for first sprite
		0x640A, //Sets Y position for first sprite
		0x6528, //Sets X position for second sprite
		0x6614, //Sets Y position for second sprite
		0x00E0, //Clears screen
		0xF129, //Sets I to the memory location of sprite 1
		0xD344, //Draws sprite 1
		0xF229, //Sets I to the memory location of sprite 2
		0xD564, //Draws sprite 2
		0x9120, //Skips a step if the two values are different
		0x00FE, //Ends program
		0x2222, //Starts subroutine
		0x120C, //Jumps back to the opcode at address 20C
		0x0000, //Spacer
		0x0000, //Spacer
		0xC20F, //Sets value 2 to a new value
		0xFF18, //PING
		0x00EE  //Return
	};

	uint16_t shiftRight[] = {
		0x630A, //Sets X position
		0x640A, //Sets Y position
		0xF10A, //Lets user input a number to shift
		0x8106, //Binary shift one place right
		0xF129, //Sets I to the space in memory where the sprite of the character in V[1] is
		0x00E0, //Clears display
		0xD344, //Draws the sprite
		0x0000, //Spacer
		0x0000, //Spacer
		0x0000, //Spacer
		0xFF18, //PING
		0x00FE  //Ends program
	};

	uint16_t guessTheNumber[]{
		0xC10F, //Sets V[1] to random single digit hex value
		0xC20F, //Sets V[2] to random single digit hex value
		0x630A, //Sets X position for first sprite
		0x640A, //Sets Y position for first sprite
		0x6528, //Sets X position for second sprite
		0x6614, //Sets Y position for second sprite
		0x00E0, //Clears display
		0xF129, //Sets I to the space in memory where the sprite of the character in V[1] is
		0xD344, //Draws the sprite
		0xF229, //Sets I to the memory location of sprite 2
		0xD564, //Draws sprite 2
		0x4101, //Skips if V[1] isn't equal to 1
		0xFF18, //PING
		0xFF18, //PING
		0x3205, //Skips if V[2] is equal to 5
		0xFF18, //PING
		0xFF18, //PING
		0x00FE  //Ends program
	};

	uint16_t calculator[]{
		0xF10A, //Lets user input a number
		0xF20A, //Lets user input a number
		0xF30A, //Lets user input a number
		0x4301, //Skips if V[3] isn't equal to 1
		0x120E, //Skips over next instruction
		0x8124, //Adds V[2] to V[1]
		0x1210, //Skips over next instruction
		0x8125, //Takes V[2] from V[1]
		0x00FE  //Ends program
	};

	uint16_t simpleGameMovement[]{
		0x600A, //Sets start X position
		0x610A, //Sets start Y position
		0x6210, //Sets V[2] to 0x10 which represents a single pixel sprite
		0xF229, //Sets I to the space in memory where the pixel sprite is
		0x6401, //Sets V[4] to 1, which is the distance the user can move in a direction
		0x00E0, //Clears display
		0xD014, //Draws the sprite
		0xF30A, //Lets user input a number
		0x3305, //Skips if the user pressed W
		0x1216, //Skips the next instruction
		0x8145, //Reduces the Y position of the sprite by V[4]
		0x3307, //Skips if the user pressed A
		0x121C, //Skips the next instruction
		0x8045, //Reduces the X position of the sprite by V[4]
		0x3308, //Skips if the user pressed S
		0x1222, //Skips the next instruction
		0x8144, //Increases the Y position of the sprite by V[4]
		0x3309, //Skips if the user pressed D
		0x1228, //Skips the next instruction
		0x8044, //Increases the X position of the sprite by V[4]
		0x120A  //Jumps back to the instruction at address 0x20A
	};

	//Tests the various bitwise operations and shifts
	//You should hear 5 pings if every operations works as intended
	uint16_t bitwiseTest[]{
		0x60FF, //Sets V[0] to 0xFF
		0x61AA, //Sets V[1] to 0xAA
		0x6255, //Sets V[2] to 0x55
		0x63D9, //Sets V[3] to 0xD9
		0x8006, //Shifts V[0] to the right
		0x407F, //Will skip if V[0] != 0x7F. If the shift has worked, the next instruction won't be skipped
		0xFF18, //PING
		0x800E, //Shifts V[0] to the left
		0x40FE, //Will skip if V[0] != 0xFE. If the shift has worked, the next instruction won't be skipped
		0xFF18, //PING
		0x8122, //Performs bitwise AND on V[1] and V[2] and saves it in V[1]
		0x4100, //Will skip if V[1] != 0x00. If the AND has worked, the next instruction won't be skipped
		0xFF18, //PING
		0x8121, //Performs bitwise OR on V[1] and V[2] and saves it in V[1]
		0x4155, //Will skip if V[1] != 0x55. If the OR has worked, the next instruction won't be skipped
		0xFF18, //PING
		0x8133, //Performs bitwise XOR on V[1] and V[3] and saves it in V[1]
		0x418C, //Will skip if V[1] != 0x8C. If the XOR has worked, the next instruction won't be skipped
		0xFF18, //PING
		0x00FE  //Ends program
	};

	uint16_t miscTests[]{
		0x6001, //Sets V[0] to 1
		0x8100, //Sets V[1] to V[0] which is 1
		0x5010, //Skips if V[0] == V[1]. This should be true
		0x120A, //Hops forward past the next instruction
		0xFF18, //PING
		0x65AA, //Sets V[5] to 0xAA
		0xF515, //Sets delay timer to V[5]
		0xFE07, //Sets V[E] to the value of the delay timer
		0x3E00, //Skips if V[E] is 0, meaning the delay timer is 0
		0x120E, //Jumps back up 2 lines
		0xFF18, //PING after delay has passed
		0x628D, //Sets V[2] to 0x8D
		0xA300, //Sets I to memory location 0x300
		0xF233, //Sets I to the BCD of V[2]
		0xF265, //Fills up to V[2] with values from memory starting at I
		0xB227, //Steps PC to V[0] (1) + 0x227 = 0x228
		0x0000, //Spacer
		0x0000, //Spacer
		0x0000, //Spacer
		0x0000, //Spacer
		0xFF18, //PING
		0x6055, //Sets V[0] to 0x55
		0x610D, //Sets V[1] to 0xD
		0x6213, //Sets V[2] to 0x13
		0x63F3, //Sets V[3] to 0xF3
		0xF355, //Stores values up to V[3] in memory starting at I
		0xF11E, //Adds V[1] to I
		0x6805, //Sets V[8] to 5
		0x6903, //Sets V[9] to 3
		0x8987, //V[9] = V[8] - V[9]
		0x5010, //Skips if V[0] == V[1]. This should be true
		0x1242, //Hops forward past the next instruction
		0xFF18, //PING
		0x6502, //Sets V[5] to 5
		0xF518, //Sets sound timer to the value in V[5]
		0x0000, //Spacer
		0x0000, //Spacer
		0x0000, //Spacer
		0x0000, //Spacer
		0x0000, //Spacer
		0x0000, //Spacer
		0x6303, //Sets V[3] to 3
		0xE39E, //Skips if a key pressed is equal to V[3] (3)
		0x1252, //Jumps back one line
		0xF518, //Sets the sound timer to V[5]
		0xE3A1, //Skips if no key pressed is equal to V[3] (3)
		0x1258, //Jumps back one line
		0xF518, //Sets the sound timer to V[5]
		0x00FE  //Ends program
	};

	myValmac.initialize();

	myValmac.load_program(countingProgram, sizeof(countingProgram));

	sf::RenderWindow window(sf::VideoMode(1280, 640), "SFML works!");

	myValmac.draw(window);
	std::clock_t lastUpdate = clock();
	std::clock_t current = clock();

	while (window.isOpen() && myValmac.simulating == true)
	{
		current = clock();
		float timeElapsed = float(current - lastUpdate) / CLOCKS_PER_SEC;
		myValmac.sample_input(window);
		if (timeElapsed >= 0.2f) {
			lastUpdate = clock();
			window.clear();
			myValmac.emulateCycle(window);
			myValmac.draw(window);
		}
	}

	system("pause");
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
