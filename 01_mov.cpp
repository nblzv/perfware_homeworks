#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint8_t b8;

struct ByteBuffer
{
	b8 eof;
	u8 data;
	u8 bits_read;
};

static void read_byte(FILE *f, ByteBuffer *buffer)
{
	buffer->eof = fread(&buffer->data, 1, 1, f) != 1;
	buffer->bits_read = 0;
}

static uint8_t read_bits(ByteBuffer *buffer, u8 bits = 1)
{
	assert((buffer->bits_read + bits) <= 8);
	
	u8 relevant_bits = buffer->data >> (8 - (buffer->bits_read + bits));
	u8 mask = (1 << bits) - 1;
	u8 result = relevant_bits & mask;
	
	buffer->bits_read += bits;
	
	return result;
}

static void read_next_byte(FILE *f, ByteBuffer *buffer)
{
	assert(buffer->bits_read == 8);
	read_byte(f, buffer);
}

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		printf("file not supplied\n");
		return 0;
	}

	char *registers_table_mod11[][8] = 
	{
		{"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"}, // w == 0
		{"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"}, // w == 1
	};

	char *registers_table_modxx[8] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"};

	
	char *filename = argv[1];
	FILE *f = fopen(filename, "rb");
	
	ByteBuffer buffer_ = {};
	buffer_.bits_read = 8;
	
	ByteBuffer *buffer = &buffer_; // for debugging purposes
	
	while(1)
	{
		read_next_byte(f, buffer);
		if(buffer->eof)
		{
			break;
		}
		
		u8 bits = read_bits(buffer, 1); // 7
		assert(bits == 1);
		
		bits = read_bits(buffer, 1); // 6
		if(bits == 0)
		{
			bits = read_bits(buffer, 1); // 5
			if(bits == 0)
			{
				bits = read_bits(buffer, 1); // 4
				assert(bits == 0);
				
				bits = read_bits(buffer, 2); // 3, 2
				assert(bits != 1);
				if(bits == 0b10)
				{
					// register/memory to/from register
					u8 d = read_bits(buffer, 1); // 1
					u8 w = read_bits(buffer, 1); // 0
					
					read_next_byte(f, buffer);
					
					u8 mod = read_bits(buffer, 2); // 7, 6
					u8 reg = read_bits(buffer, 3); // 5, 4, 3
					u8 rm = read_bits(buffer, 3); // 2, 1, 0
					
					if(mod == 0b11)
					{
						// register mode, no displacement
						char *dst = registers_table_mod11[w][d ? reg : rm];
						char *src = registers_table_mod11[w][d ? rm : reg];
					
						printf("mov %s, %s", dst, src);
					}
					else if(mod == 0)
					{
						// memory mode, no displacement UNLESS rm == 110, then 16-bit
						if(rm == 0b110)
						{
							assert(false);
						}
						else
						{
							char *r = registers_table_mod11[w][reg];
							char m[32]; sprintf(m, "[%s]", registers_table_modxx[rm]);
							
							char *dst = d ? r : m;
							char *src = d ? m : r;
						
							printf("mov %s, %s", dst, src);
						}
					}
					else
					{
						read_next_byte(f, buffer);
						
						u8 disp_lo = read_bits(buffer, 8);
						u16 disp = disp_lo;
						
						if(mod == 0b10)
						{
							read_next_byte(f, buffer);
							
							u8 disp_hi = read_bits(buffer, 8);
							disp += (u16)disp_hi << 8;
						}

						char *r = registers_table_mod11[w][reg];
						char m[32]; sprintf(m, "[%s + %hu]", registers_table_modxx[rm], disp);
						
						char *dst = d ? r : m;
						char *src = d ? m : r;

						printf("mov %s, %s", dst, src);
					}
				}
				else
				{
					// accum/segment
					assert(false);
				}
			}
			else
			{
				bits = read_bits(buffer, 1);
				
				if(bits == 1)
				{
					// immediate to register
					u8 w = read_bits(buffer, 1);
					u8 reg = read_bits(buffer, 3);
					
					read_next_byte(f, buffer);
					
					u8 data_lo = read_bits(buffer, 8);
					u16 imm = data_lo;
					if(w)
					{
						read_next_byte(f, buffer);
						
						u8 data_hi = read_bits(buffer, 8);
						imm += (u16)data_hi << 8;
					}
					
					char *dst = registers_table_mod11[w][reg];
					printf("mov %s, %hu", dst, imm);
				}
				else
				{
					// memory to accumulator OR accumulator to memory
					assert(false);
				}
			}
		}
		else
		{
			// immediate to register/memory
			assert(false);
		}
		
		printf("\n");
	}
	
	return 0;
}