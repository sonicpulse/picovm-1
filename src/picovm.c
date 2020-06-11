#include <memory.h>
#include "config.h"
#include "picovm.h"

#if USE_SINGLE_DATA_TYPE == 8
	#define SINGLE_DATA_TYPE uint8_t
#elif USE_SINGLE_DATA_TYPE == 16
	#define SINGLE_DATA_TYPE uint16_t
#elif USE_SINGLE_DATA_TYPE == 32
	#define SINGLE_DATA_TYPE uint32_t
#else
	#undef SINGLE_DATA_TYPE
#endif

#define READ8(x)  (*(uint8_t *)(vm->mem + (x)))
#define READ16(x) (*(uint16_t *)(vm->mem + (x)))
#define READ32(x) (*(uint32_t *)(vm->mem + (x)))

#ifdef SINGLE_DATA_TYPE
	#define MEMCPY_SIZE(d, s) *(SINGLE_DATA_TYPE *)(d) = *(SINGLE_DATA_TYPE *)(s)
#else
	#define MEMCPY_SIZE(d, s) memcpy((d), (s), size)
#endif

static void push_stack(struct picovm_s *vm, void *value, uint8_t size)
{
    vm->sp -= size;
	memcpy(vm->sp, value, size);
}

static void pop_stack(struct picovm_s *vm, uint8_t size, void *value)
{
	memcpy(value, vm->sp, size);
	vm->sp += size;
}

int8_t picovm_exec(struct picovm_s *vm)
{
	uint16_t addr = 0;
    uint8_t opcode = READ8(vm->ip);

	uint8_t size = 1 << (opcode & 0x03);

    switch (opcode)
	{
        case 0x00 ... 0x00 + 15: // LOAD addr
        {
			uint8_t cmd = (opcode & 0xc) >> 2;

			switch (cmd)
			{
				case 0:
				{
					addr = READ16(vm->ip + 1);
					vm->sp -= size;
					MEMCPY_SIZE(vm->sp, vm->mem + addr);
					vm->ip += 3;	
					break;
				}
#ifdef INCLUDE_OPTIONAL_INSTRUCTIONS
				case 1:
				{
					uint8_t offset = READ8(vm->ip + 1);
					memcpy(&addr, vm->sp, sizeof(uint16_t));
					vm->sp += sizeof(uint16_t);
					vm->sp -= size;
					MEMCPY_SIZE(vm->sp, vm->mem + addr + offset);
					vm->ip += 2;
					break;
				}
#endif
				case 2:
				{
					struct {
						int16_t offset;
						uint16_t addr;
					} data;

					memcpy(&data, vm->sp, sizeof(uint16_t) * 2);
					vm->sp += sizeof(uint16_t) * 2;
					vm->sp -= size;
					MEMCPY_SIZE(vm->sp, vm->mem + data.addr + data.offset);
					vm->ip += 1;
					break;
				}
#ifndef INCLUDE_OPTIONAL_INSTRUCTIONS
				case 3:
				{
					vm->sp -= size;
					MEMCPY_SIZE(vm->sp, vm->mem + vm->ip + 1);
					vm->ip += 1 + size;	
					break;
				}
#endif
			}
            
            break;
        }
		
        case 0x10 ... 0x10 + 9: // STORE addr
        {
			uint8_t cmd = (opcode & 0xc) >> 2;

			switch (cmd)
			{
				case 0:
				{
					addr = READ16(vm->ip + 1);
					MEMCPY_SIZE(vm->mem + addr, vm->sp);
					vm->sp += size;
					vm->ip += 3;
					break;
				}
#ifdef INCLUDE_OPTIONAL_INSTRUCTIONS
				case 1:
				{
					uint8_t offset = READ8(vm->ip + 1);
					memcpy(&addr, vm->sp + size, 2);
					MEMCPY_SIZE(vm->mem + addr + offset, vm->sp);
					vm->sp += size + 2;
					vm->ip += 2;
					break;
				}
#endif
				case 2:
				{
					struct {
						int16_t offset;
						uint16_t addr;
					} data;

					memcpy(&data, vm->sp + size, sizeof(uint16_t)*2);
					MEMCPY_SIZE(vm->mem + data.addr + data.offset, vm->sp);
					vm->sp += size + sizeof(uint16_t)*2;
					vm->ip += 1;
					break;
				}
			}
            
            break;
        }

		case 0x1c ... 0x1c + 3: // POP
        {
			vm->sp += size;
            vm->ip += 1;
            break;
        }

		case 0x20 ... 0x20 + 15: // DUP
        {
			uint8_t k = (opcode & 0xc) >> 2;
			if (k == 3) {
				k = (uint8_t)READ8(vm->ip+1);
				vm->ip += 1;
			}

			vm->sp -= size;
			memcpy(vm->sp, vm->sp + size, k + size);
			MEMCPY_SIZE(vm->sp + size + k, vm->sp);

            vm->ip += 1;
            break;
        }

		case 0x30 ... 0x30 + 15: // DIG
        {
			uint8_t k = (opcode & 0xc) >> 2;
			if (k == 3) {
				k = (uint8_t)READ8(vm->ip+1);
				vm->ip += 1;
			}

			vm->sp -= size;
			MEMCPY_SIZE(vm->sp, vm->sp + size + k);

            vm->ip += 1;
            break;
        }

		case 0x40: // CALL
			addr = (int16_t)READ16(vm->ip+1);
			vm->ip += 3;
			if (0) {
		case 0x41: // CALL [ref]
				memcpy(&addr, vm->sp, 2);
				vm->sp += 2;
			}
			vm->sp -= 2;
			memcpy(vm->sp, &vm->ip, 2);
			vm->ip = addr;
			break;
		case 0x42: // RET
		{
			memcpy(&vm->ip, vm->sp, 2);
			vm->sp += 2;
			break;
		}
		case 0x80+0 ... 0x80+43: // ARITHMETIC OPERATIONS
		{
			uint32_t a, b;
			uint8_t cmd = (opcode & 0x3c) >> 2;
			if (cmd != 7) {
				pop_stack(vm, size, &b);
			}
			pop_stack(vm, size, &a);
			switch (cmd)
			{
				case 0: a += b; break;
				case 1: a -= b; break;
				case 2: a *= b; break;
				case 3: a /= b; break;
				case 4: a %= b; break;
#ifdef INCLUDE_BITWISE_INSTRUCTIONS
				case 5: a &= b; break;
				case 6: a |= b; break;
				case 7: a ^= b; break;
				case 8: a = !a; break;
#endif
			}
			uint8_t sign_bit = size * 8 - 1;
			uint32_t mask = (1 << size*8) - 1;
			
			vm->flags = (a & mask) == 0 ? PICOVM_FLAG_Z : 0;
			if (a & (1 << sign_bit)) vm->flags |= PICOVM_FLAG_N;

			push_stack(vm, &a, size);
			vm->ip++;
			break;
		}
#ifdef INCLUDE_FLOATING_POINT_INSTRUCTIONS
		case 0xac ... 0xac + 4: // FLOATING POINT OPERATIONS
		{
			struct { float b, a; } fops;
			uint8_t cmd = (opcode & 0x3c) >> 2;
			
			memcpy(&fops, vm->sp, sizeof(float) * 2);
			vm->sp += sizeof(float) * 2;
			switch (cmd)
			{
				case 11: fops.a += fops.b; break;
				case 12: fops.a -= fops.b; break;
				case 13: fops.a *= fops.b; break;
				case 14: if (fops.b != 0.0f)fops.a /= fops.b; else fops.a = 0.0f; break;
			}

			vm->flags = fops.a == 0.0f ? PICOVM_FLAG_Z : 0;
			if (fops.a < 0.0f) vm->flags |= PICOVM_FLAG_N;
			
			push_stack(vm, &fops.a, sizeof(float));
			vm->ip++;
			break;
		}
		case 0xbc: // CONVI
		{
			float f_value = *(float *)(vm->sp);
			*(uint32_t *)(vm->sp) = f_value;
			vm->ip++;
			break;
		}
		case 0xbc + 1: // CONVF
		{
			uint32_t i_value = *(uint32_t *)(vm->sp);
			*(float *)(vm->sp) = i_value;
			vm->ip++;
			break;
		}
#endif
		// JMP addr
		case 0xc0 ... 0xc0+16: // Branching
		{			
			uint32_t addr;

			// Get jump address
			if ((opcode & 1) == 0) {
				addr = vm->ip + (int8_t)READ8(vm->ip+1);
				vm->ip += 2;
			} else {
				addr = vm->ip + (int16_t)READ16(vm->ip+1);
				vm->ip += 3;
			}

			uint16_t jump_type = (opcode & 0xE) >> 1;
			if (jump_type == 0) {
				vm->ip = addr;
				break;
			}

			uint8_t flags = vm->flags;
			switch (jump_type)
			{
			case 1:
				if (flags & PICOVM_FLAG_Z)
					vm->ip = addr;
				break;
			case 2:
				if (~flags & PICOVM_FLAG_Z)
					vm->ip = addr;
				break;
			case 3:
				if (flags & PICOVM_FLAG_N)
					vm->ip = addr;
				break;
			case 4:
				if (~flags & PICOVM_FLAG_N)
					vm->ip = addr;
				break;
			case 5:
				if ( (flags & PICOVM_FLAG_N) && (flags & PICOVM_FLAG_Z) )
					vm->ip = addr;
				break;
			case 6:
				if ( ~(flags & PICOVM_FLAG_N) && (flags & PICOVM_FLAG_Z) )
					vm->ip = addr;
				break;
			}
			break;
			
		}
		// HLT
		case 0xff:
			return -1;
		// YIELD
		case 0xfe:
			vm->ip++;
			return -2;
	}
	
	return 0;
}
