#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

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

static u8 peek_bits(ByteBuffer *buffer, u8 bits)
{
	assert((buffer->bits_read + bits) <= 8);
	
	u8 relevant_bits = buffer->data >> (8 - (buffer->bits_read + bits));
	u8 mask = (1 << bits) - 1;
	u8 result = relevant_bits & mask;
	
	return result;
}

static u8 read_bits(ByteBuffer *buffer, u8 bits)
{
	u8 result = peek_bits(buffer, bits);
	buffer->bits_read += bits;
	return result;
}

static void read_next_byte(FILE *f, ByteBuffer *buffer)
{
	assert(buffer->bits_read == 8);
	read_byte(f, buffer);
}

static char *registers_table_mod11[][8] = 
{
	{"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"}, // w == 0
	{"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"}, // w == 1
};

static char *registers_table_modxx[8] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"};

static u16 read_immediate(FILE *f, ByteBuffer *buffer, u8 w, u8 s)
{
	u8 data_lo = read_bits(buffer, 8);
	u16 imm = data_lo;
	
	if(s)
	{
		u8 data_hi = data_lo >> 7 ? -1 : 0;
		imm = ((u16)data_hi << 8) | imm;
	}
	else if(w)
	{
		read_next_byte(f, buffer);
		
		u8 data_hi = read_bits(buffer, 8);
		imm += (u16)data_hi << 8;
	}
	
	return imm;
}

static void handle_dw_modregrm(FILE *f, ByteBuffer *buffer, char *print_buf, u8 d, u8 w, u8 mod, u8 reg, u8 rm, b8 print_src = true)
{
	if(mod == 0b11)
	{
		// register mode, no displacement
		char *dst = registers_table_mod11[w][d ? reg : rm];
		char *src = registers_table_mod11[w][d ? rm : reg];
		
		if(!print_src) { src = ""; }
		sprintf(print_buf, "%s, %s", dst, src);
	}
	else if(mod == 0)
	{
		// memory mode, no displacement UNLESS rm == 110, then 16-bit
		if(rm != 0b110)
		{
			char *r = registers_table_mod11[w][reg];
			char m[32]; sprintf(m, "[%s]", registers_table_modxx[rm]);
			
			char *dst = d ? r : m;
			char *src = d ? m : r;
		
			if(!print_src) { src = ""; }
			sprintf(print_buf, "%s, %s", dst, src);
		}
		else
		{
			read_next_byte(f, buffer);
			
			u16 disp = read_immediate(f, buffer, 1, 0);
			sprintf(print_buf, "[%hu], ", disp);
		}
	}
	else
	{
		// 8/16 bit displacement/dispatch
		read_next_byte(f, buffer);
		
		u16 disp = read_immediate(f, buffer, mod == 0b10, 0);
		
		char *r = registers_table_mod11[w][reg];
		char m[32]; sprintf(m, "[%s + %hu]", registers_table_modxx[rm], disp);
		
		char *dst = d ? r : m;
		char *src = d ? m : r;

		if(!print_src) { src = ""; }
		sprintf(print_buf, "%s, %s", dst, src);
	}
}

static void print_add_sub_cmp(char *print_buf, u8 op)
{
	if(op == 0)
	{
		sprintf(print_buf, "add ");
	}
	else if(op == 0b101)
	{
		sprintf(print_buf, "sub ");
	}
	else if(op == 0b111)
	{
		sprintf(print_buf, "cmp ");
	}
	else
	{
		assert(false);
	}
}

struct jump_entry
{
	u8 k;
	char *v;
};
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		printf("file not supplied\n");
		return 0;
	}
	
	jump_entry jump_codes[] =
	{
		{0b01110100, "je"},
		{0b01111100, "jl"},
		{0b01111110, "jle"},
		{0b01110010, "jb"},
		{0b01110110, "jbe"},
		{0b01111010, "jp"},
		{0b01110000, "jo"},
		{0b01111000, "js"},
		{0b01110101, "jne"},
		{0b01111101, "jnl"},
		{0b01111111, "jnle"},
		{0b01110011, "jnb"},
		{0b01110111, "jnbe"},
		{0b01111011, "jnp"},
		{0b01110001, "jno"},
		{0b01111001, "jns"},
		{0b11100010, "loop"},
		{0b11100001, "loopz"},
		{0b11100000, "loopnz"},
		{0b11100011, "jcxz"},
	};
	
	char *filename = argv[1];
	FILE *f = fopen(filename, "rb");
	
	ByteBuffer buffer_ = {};
	buffer_.bits_read = 8;
	ByteBuffer *buffer = &buffer_; // for debugging purposes
	u16 i = 0;
	
	while(1)
	{
		read_next_byte(f, buffer);
		if(buffer->eof)
		{
			break;
		}
		i += 1;
		
		char print_buf[64]; 
		print_buf[0] = 0;
		
		u8 byte = peek_bits(buffer, 8);
		b8 handled = false;
		for(int jump_i = 0; jump_i < ARRAY_COUNT(jump_codes); ++jump_i)
		{
			if(jump_codes[jump_i].k == byte)
			{
				read_bits(buffer, 8);
				read_next_byte(f, buffer);
				
				u16 imm = read_immediate(f, buffer, 0, 0);
				
				sprintf(print_buf, "%s %hhd", jump_codes[jump_i].v, imm);
				
				handled = true;
				break;
			}
		}

		if(!handled)
		{
			u8 bits = read_bits(buffer, 1); // 7
			u8 peeked = peek_bits(buffer, 1);
			if(bits == 0 || (bits == 1 && peeked == 0))
			{
				// add, sub, cmp
				u8 zero = read_bits(buffer, 1);
				assert(zero == 0);
				
				if(bits == 1)
				{
					bits = read_bits(buffer, 4);
					assert(bits == 0);
					
					u8 s = read_bits(buffer, 1);
					u8 w = read_bits(buffer, 1);
					
					read_next_byte(f, buffer);
					
					u8 mod = read_bits(buffer, 2);
					u8 op = read_bits(buffer, 3);
					u8 rm = read_bits(buffer, 3);

					print_add_sub_cmp(print_buf, op);
					handle_dw_modregrm(f, buffer, print_buf + 4, 0, w, mod, 0, rm, false);
					
					read_next_byte(f, buffer);
					u16 imm = read_immediate(f, buffer, 0, s);
					
					sprintf(print_buf + strlen(print_buf), "%hu", imm);
				}
				else
				{
					u8 op = read_bits(buffer, 3);
					print_add_sub_cmp(print_buf, op);
					
					bits = read_bits(buffer, 1);
					if(bits == 0)
					{
						u8 d = read_bits(buffer, 1);
						u8 w = read_bits(buffer, 1);
						
						read_next_byte(f, buffer);
						
						u8 mod = read_bits(buffer, 2);
						u8 reg = read_bits(buffer, 3);
						u8 rm = read_bits(buffer, 3);

						handle_dw_modregrm(f, buffer, print_buf + 4, d, w, mod, reg, rm);
					}
					else
					{
						u8 zero = read_bits(buffer, 1);
						assert(zero == 0);
						u8 w = read_bits(buffer, 1);
						
						read_next_byte(f, buffer);
						u16 imm = read_immediate(f, buffer, w, 0);
						
						char *dst = w ? "ax" : "al";
						sprintf(print_buf + 4, "%s, %hu", dst, imm);
					}
					
				}
			}
			else
			{
				// mov
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
						
							sprintf(print_buf, "mov ");
							handle_dw_modregrm(f, buffer, print_buf + 4, d, w, mod, reg, rm);
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
							sprintf(print_buf, "mov %s, %hu", dst, imm);
						}
					}
				}
			}
		}
		
		assert(print_buf[0] != 0);
		printf("%s\n", print_buf);
	}
	
	return 0;
}