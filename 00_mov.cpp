#include <stdio.h>
#include <stdint.h>
#include <assert.h>

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		printf("file not supplied\n");
		return 0;
	}

	char *registers_table_w0[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
	char *registers_table_w1[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
	
	char *filename = argv[1];
	FILE *f = fopen(filename, "rb");
	uint16_t buffer = 0;
	while(fread(&buffer, sizeof(buffer), 1, f) == 1)
	{
		uint8_t fb = (uint8_t)buffer;
		uint8_t opcode = fb >> 2;
		uint8_t direction = (fb & 0x2) >> 1; // 1 = reg dest, 0 = reg source
		uint8_t wide = fb & 0x1; // 0 = byte (8 bit), 1 = word (16 bit)

		uint8_t sb = (uint8_t)(buffer >> 8);
		uint8_t mod = sb >> 6;
		uint8_t reg = (sb >> 3) & 0x7;
		uint8_t rm = sb & 0x7;
		
		assert(opcode == 34);
		assert(mod == 0x3);
		
		char **table_to_use = wide ? registers_table_w1 : registers_table_w0;
		char *dst = table_to_use[direction ? reg : rm];
		char *src = table_to_use[direction ? rm : reg];
		
		printf("mov %s, %s\n", dst, src);
	}
	
	return 0;
}