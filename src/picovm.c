#include <memory.h>
#include "picovm.h"

// #include <stddef.h>
// void *memcpy(void *__restrict, const void *__restrict, size_t);

#define READ8(x)  (*(uint8_t *)(vm->mem + (x)))
#define READ16(x) (*(uint16_t *)(vm->mem + (x)))
#define READ32(x) (*(uint32_t *)(vm->mem + (x)))

static inline int16_t signext(uint16_t val, uint16_t mask)
{
	val = val & mask;
	if ((val & ~(mask >> 1)) != 0)
		val |= ~mask;
	return val;
}

static inline void read_mem(struct picovm_s *vm, uint16_t addr, uint8_t size, void *value)
{
	memcpy(value, vm->mem + addr, size);
}

static inline void write_mem(struct picovm_s *vm, uint16_t addr, void *value, uint8_t size)
{
	memcpy(vm->mem + addr, value, size);
}

static inline void push_stack(struct picovm_s *vm, void *value, uint8_t size)
{
    vm->sp -= size;
	memcpy(vm->mem + vm->sp, value, size);
}

static inline void pop_stack(struct picovm_s *vm, uint8_t size, void *value)
{
	memcpy(value, vm->mem + vm->sp, size);
	vm->sp += size;
}

int8_t picovm_exec(struct picovm_s *vm)
{
    uint8_t opcode = READ8(vm->ip);
    uint32_t a, b, addr;

	float fa, fb;

    switch (opcode)
	{
        case 0x00 ... 0x00 + 3: // LOAD addr
        {
            uint16_t addr = READ16(vm->ip + 1);
			uint8_t size = 1 << (opcode & 0x02);
			uint32_t value;
			read_mem(vm, addr, size, &value);
			push_stack(vm, &value, size);            
            vm->ip += 3;
            break;
        }
		
        case 0x08 ... 0x08 + 3: // STORE addr
        {
			uint16_t addr = READ16(vm->ip + 1);
			uint8_t size = 1 << (opcode & 0x02);
			uint32_t value;
			pop_stack(vm, size, &value);
			write_mem(vm, addr, &value, size);
            vm->ip += 3;
            break;
        }

		case 0x80+0 ... 0x80+43:
		{
			uint8_t cmd = (opcode & 0x3c) >> 2;
			uint8_t size = 1 << (opcode & 0x02);
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

				case 5: a &= b; break;
				case 6: a |= b; break;
				case 7: a ^= b; break;
				case 8: a = !a; break;
			}
			push_stack(vm, &a, size);
			vm->ip++;
			break;
		}
		case 0xac ... 0xac + 4:
		{
			uint8_t cmd = (opcode & 0x3c) >> 2;
			pop_stack(vm, sizeof(float), &fb);
			pop_stack(vm, sizeof(float), &fa);
			switch (cmd)
			{
				case 11: fa += fb; break;
				case 12: fa -= fb; break;
				case 13: fa *= fb; break;
				case 14: if (fb != 0.0f)fa /= fb; else fa = 0.0f; break;
			}
			push_stack(vm, &fa, sizeof(float));
			vm->ip++;
			break;
		}
		// JMP addr
		case 0xc0 ... 0xc0+16:
		{
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

			int32_t top;
			pop_stack(vm, 4, &top);

			switch (jump_type)
			{
			case 1:
				if (top == 0)
					vm->ip = addr;
				break;
			case 2:
				if (top != 0)
					vm->ip = addr;
				break;
			case 3:
				if (top < 0)
					vm->ip = addr;
				break;
			case 4:
				if (top > 0)
					vm->ip = addr;
				break;
			case 5:
				if (top <= 0)
					vm->ip = addr;
				break;
			case 6:
				if (top >= 0)
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
