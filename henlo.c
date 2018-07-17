/* architecture-dependent code generation for Henlo */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "ncc.h"

/* Hardware registers */
#define R_R0		0x00
#define R_R1		0x01
#define R_R2		0x02
#define R_R3		0x03
#define R_FP		0x04
#define R_SP		0x05
#define R_AC		0x06
#define R_PC		0x07

#define R_CMP		R_R3

#define REG_RET		R_R0

/* Henlo opcodes */
#define I_ADD		0x0
#define I_ADDI	0x1
#define I_MUL		0x2
#define I_MULI	0x3
#define I_AND		0x4
#define I_OR		0x5
#define I_XOR		0x6
#define I_MOV		0x7
#define I_NEG		0x8
#define I_LD		0x9
#define I_ST		0xa
#define I_JMP		0xb
#define I_BZ		0xc
#define I_BNZ		0xd
#define I_STO		0xe

/* Henlo flags */
#define JMP_ABS	0
#define JMP_REL	1
#define JMP_Z		0
#define JMP_NZ	1

// oooo||r1||r2||r3||xxx
#define OP1(op, r1, r2, r3)	((((op) & 0xf) << 12) | (((r1) & 0x7) << 9) | (((r2) & 0x7) << 6) | (((r3) & 0x7) << 3)) 

// oooo||r1||imm
#define OP2(op, r1, imm)	((((op) & 0xf) << 12) | (((r1) & 0x7) << 9) |((imm) & 0x1ff))

// oooo||r1||r2||flag||xxxxx
#define OP3(op, r1, r2, fl)	((((op) & 0xf) << 12) | (((r1) & 0x7) << 9) | (((r2) & 0x7) << 6) | (((fl) & 0x1) << 5)) 

#define OP4(op, r1, r2, r3, fl1, fl2, fl3)	((((op) & 0xf) << 12) | (((r1) & 0x7) << 9) | (((r2) & 0x7) << 6) | (((r3) & 0x7) << 3) | (((fl1) & 0x1) << 2) | (((fl2) & 0x1) << 1) | ((fl3) & 0x1))

#define HIGH(i) ((i >> 8) & 0xFF)
#define LOW(i) ((i) & 0xFF)

int tmpregs[] = {0, 1, 2};
int argregs[] = {};

static struct mem cs;				/* generated code */
static long *rel_sym;				/* relocation symbols */
static long *rel_flg;				/* relocation flags */
static long *rel_off;				/* relocation offsets */
static long rel_n, rel_sz;	/* relocation count */

static long lab_sz;					/* label count */
static long *lab_loc;				/* label offsets in cs */
static long jmp_n, jmp_sz;	/* jump count */
static long *jmp_off;				/* jump offsets */
static long *jmp_dst;				/* jump destinations */
static long *jmp_op;				/* jump opcode */

static long i_load_acc_imm(uint16_t n);

/* code generation functions */
static void os(void *s, int n)
{
	mem_put(&cs, s, n);
}

static char *ointbuf(long n, int l)
{
	static char buf[16];
	int i;
	printf("I: ");
	for (i = 0; i < l; i++) {
		buf[i] = (n >> (8 * (l - i - 1))) & 0xff;
		printf("%02x ", buf[i] & 0xff);
	}
	printf("\n");
	return buf;
}

static void oi(long n, int l)
{
	mem_put(&cs, ointbuf(n, l), l);
}

static void oi_at(long pos, long n, int l)
{
	mem_cpy(&cs, pos, ointbuf(n, l), l);
}

static long opos(void)
{
	return mem_len(&cs);
}

static long op_typ(int op, int r1, int r2, int r3) {
	oi(OP1(op, r1, r2, r3), 2);
	return 2;
}

static long op_add(int op, int r1, int r2, int r3, int fl1, int fl2) {
	oi(OP4(op, r1, r2, r3, fl1, fl2, 0), 2);
	return 2;
}

static long op_rrf(int op, int r1, int r2, int fl1) {
	oi(OP3(op, r1, r2, fl1), 2);
	return 2;
}

static long op_imm(int op, int r1, int imm) {
	oi(OP2(op, r1, imm), 2);
	return 2;
}

static long i_cpy_reg(int src, int dst) {
	oi(OP1(I_MOV, src, dst, 0), 2);
}

/* generate a jump instruction and return its displacement from the start */
static long i_jmp(long op, long r1, long r2)
{
	// jump-type specific offset so we know where to insert
	long offset = 0;

	if (op & O_JZ) {
		offset = opos();

		// We will fill in instructions later that load our jump address into acc
		oi(0, 8);

		// jmp to R_AC if r1 == 0 or r1 != 0, depending on op
		int jmp_instruction = O_C(op) == O_JZ ? I_BZ : I_BNZ;
		oi(OP3(jmp_instruction, R_AC, r1, JMP_REL), 2);

	} else if (op & O_JCC) {
		// lt, ge, eq, ne, le, gt
		int comparison_type = op & 0x0f;
		int is_signed = O_T(op) & T_MSIGN ? 1 : 0;
		long jmp_instruction = 0;

		// r2/const -> R_AC
		if (op & O_NUM) {
			i_load_acc_imm(r2);
		} else {
			i_cpy_reg(r2, R_AC);
		}

		int overflow_jmp = OP3(I_BNZ, R_CMP, R_AC, JMP_REL);
		int no_overflow_jmp = OP3(I_BZ, R_CMP, R_AC, JMP_REL);

		// do the comparison and store the result into R_CMP
		if (comparison_type == 0) { // lt
			// branch iff a < b
			// compute c = a + (-b)
			// if c is negative, we should branch
			// hardware deals with 0x8000 edge cases
			op_rrf(I_NEG, R_AC, R_CMP, is_signed);
			op_add(I_ADD, r1, R_CMP, R_CMP, is_signed, 1); // R_CMP = r1 - r2. branch if R_CMP < 0
			op_typ(I_STO, R_CMP, 0, 0);
			jmp_instruction = overflow_jmp;
		} else if (comparison_type == 1) { // ge
			op_rrf(I_NEG, R_AC, R_CMP, is_signed);
			op_add(I_ADD, r1, R_CMP, R_CMP, is_signed, 1); // R_CMP = r1 - r2. branch if R_CMP >= 0
			op_typ(I_STO, R_CMP, 0, 0);
			jmp_instruction = no_overflow_jmp;
		}	else if (comparison_type == 2) { // eq
			op_typ(I_XOR, r1, R_AC, R_CMP);
			jmp_instruction = OP3(I_BZ, R_CMP, R_AC, JMP_REL);
		} else if (comparison_type == 3) { // ne
			op_typ(I_XOR, r1, R_AC, R_CMP);
			jmp_instruction = OP3(I_BNZ, R_CMP, R_AC, JMP_REL);
		} else if (comparison_type == 4) { // le
			// branch iff a <= b
			// equivalently, DON'T branch if b < a
			// compute c = b + (-a)
			// branch if c NOT negative
			// hardware deals with 0x8000 edge cases
			op_rrf(I_NEG, r1, R_CMP, is_signed);
			op_add(I_ADD, R_AC, R_CMP, R_CMP, is_signed, 1); // R_CMP = r2 - r1. branch if R_CMP >= 0
			op_typ(I_STO, R_CMP, 0, 0);
			jmp_instruction = no_overflow_jmp;
		} else if (comparison_type == 5) { // gt
			op_rrf(I_NEG, r1, R_CMP, is_signed);
			op_add(I_ADD, R_AC, R_CMP, R_CMP, is_signed, 1); // R_CMP = r2 - r1. branch if R_CMP < 0
			op_typ(I_STO, R_CMP, 0, 0);
			jmp_instruction = overflow_jmp;
		} else {
			die("unsupported signed jcc type %d", comparison_type);
		}

		// Load jump offset into R_AC
		offset = opos();
		oi(0, 8);

		// perform the jmp
		oi(jmp_instruction, 2);
	} else {
		offset = opos();
		oi(0, 8);
		oi(OP3(I_JMP, R_AC, 0, JMP_REL), 2);
	}

	return offset;
}

/* the length of a jump instruction opcode */
static int i_jlen(long op, int nb)
{
}

/* zero extend */
static void i_zx(int rd, int r1, int bits)
{
}

/* sign extend */
static void i_sx(int rd, int r1, int bits)
{
}

static void i_cast(int rd, int rn, int bt)
{
}

// Load 16 bit constant into R_AC
static long i_load_acc_imm(uint16_t n) {
	long len = 2;

	int byte_0 = n & 0xFF;
	int byte_1 = (n >> 8) & 0xFF;

	op_typ(I_XOR, R_AC, R_AC, R_AC);

	// shift immediate into accumulator
	if (n > 255) {
		op_imm(I_ADDI, R_AC, byte_1);
		op_imm(I_MULI, R_AC, 256);
		len += 4;
	}

	if (n > 0) {
		op_imm(I_ADDI, R_AC, byte_0);
		len += 2;
	}

	return len;
}

static void i_ld_num(long rd, long r1, long r2)
{
	i_load_acc_imm(r2);
	op_typ(I_ADD, r1, R_AC, R_AC);
	op_typ(I_LD, R_AC, rd, 0);
}

static void i_st_num(long r1, long r2, long r3)
{
	i_load_acc_imm(r3);
	op_typ(I_ADD, r2, R_AC, R_AC);
	op_typ(I_ST, r1, R_AC, 0);
}

static void i_mul(long rd, long r1, long r2)
{
	op_typ(I_MUL, r1, r2, rd);
}

static void i_pop(long r1)
{
	i_load_acc_imm(1);
	op_typ(I_ADD, R_SP, R_AC, R_SP);
	op_typ(I_LD, R_SP, r1, 0);
}

static void i_push(long r1)
{
	op_typ(I_ST, r1, R_SP, 0);
	i_load_acc_imm(1);
	op_typ(I_NEG, R_AC, R_AC, 0);
	op_typ(I_ADD, R_SP, R_AC, R_SP);
}

static void regs_save(long sregs)
{
	int i;
	for (i = 0; i < N_REGS; i++)
		if (((1 << i) & R_TMPS) & sregs)
			i_push(i);
}

static void regs_load(long sregs)
{
	int i;
	for (i = N_REGS - 1; i >= 0; --i)
		if (((1 << i) & R_TMPS) & sregs)
			i_pop(i);
}

static void i_add(long op, long rd, long r1, long r2)
{
	if (op & O_SUB) {
		op_typ(I_NEG, r2, R_AC, 0);
		op_typ(I_ADD, r1, R_AC, rd);
	} else if (op & O_AND) {
		op_typ(I_AND, r1, r2, rd);
	} else if (op & O_OR) {
		op_typ(I_OR, r1, r2, rd);
	} else if (op & O_XOR) {
		op_typ(I_XOR, r1, r2, rd);
	} else {
		op_typ(I_ADD, r1, r2, rd);
	}
}

static void i_add_anyimm(long op, int rd, int r1, long imm)
{
	i_load_acc_imm(imm);
	i_add(op, rd, r1, R_AC);
}

static void lab_add(long id)
{
	printf("LABEL %ld\n", id);
	while (id >= lab_sz) {
		int lab_n = lab_sz;
		lab_sz = MAX(128, lab_sz * 2);
		lab_loc = mextend(lab_loc, lab_n, lab_sz, sizeof(*lab_loc));
	}
	lab_loc[id] = opos() - 1;
}

static void jmp_add(long op, long off, long dst)
{
	if (jmp_n == jmp_sz) {
		jmp_sz = MAX(128, jmp_sz * 2);
		jmp_off = mextend(jmp_off, jmp_n, jmp_sz, sizeof(*jmp_off));
		jmp_dst = mextend(jmp_dst, jmp_n, jmp_sz, sizeof(*jmp_dst));
		jmp_op = mextend(jmp_op, jmp_n, jmp_sz, sizeof(*jmp_op));
	}
	jmp_off[jmp_n] = off;
	jmp_dst[jmp_n] = dst;
	jmp_op[jmp_n] = op;
	jmp_n++;
}

void i_label(long id)
{
	lab_add(id + 1);
}

static void i_rel(long sym, long flg, long off)
{
	if (rel_n == rel_sz) {
		rel_sz = MAX(128, rel_sz * 2);
		rel_sym = mextend(rel_sym, rel_n, rel_sz, sizeof(*rel_sym));
		rel_flg = mextend(rel_flg, rel_n, rel_sz, sizeof(*rel_flg));
		rel_off = mextend(rel_off, rel_n, rel_sz, sizeof(*rel_off));
	}
	rel_sym[rel_n] = sym;
	rel_flg[rel_n] = flg;
	rel_off[rel_n] = off;
	rel_n++;
}

static void i_sym(int rd, int sym, int off)
{
}

static void i_subsp(long val)
{
}

static int regs_count(long regs)
{
	int cnt = 0;
	int i;
	for (i = 0; i < N_REGS; i++)
		if (((1 << i) & R_TMPS) & regs)
			cnt++;
	return cnt;
}

void i_wrap(int argc, long sargs, long spsub, int initfp, long sregs, long sregs_pos)
{
	void *old_body;
	long old_body_len;
	long diff;
	int i;

	// Add return label to end of function so ret can jump to epilogue
	lab_add(0);

	old_body_len = mem_len(&cs);

	// mem_get zeroes out the cs struct, but does not free old_body
	old_body = mem_get(&cs);

	if (initfp) {
		i_push(R_FP);
		i_cpy_reg(R_SP, R_FP);
	}

	if (sregs) {
		regs_save(sregs);
	}

	diff = mem_len(&cs);
	mem_put(&cs, old_body, old_body_len);
	free(old_body);

	if (sregs) {
		regs_load(sregs);
	}

	if (initfp) {
		i_cpy_reg(R_FP, R_SP);
		i_pop(R_FP);
	}
	i_pop(R_PC);

	// Now that we've added a prologue, offsets should be bumped
	for (i = 0; i < rel_n; i++) {
		rel_off[i] += diff;
	}
	for (i = 0; i < jmp_n; i++) {
		jmp_off[i] += diff;
	}
	for (i = 0; i < lab_sz; i++) {
		lab_loc[i] += diff;
	}
}

void i_code(char **c, long *c_len, long **rsym, long **rflg, long **roff, long *rcnt)
{
	int i;

	// Fill in all of the relative jump offsets
	for (i = 0; i < jmp_n; i++) {
		uint16_t rel_insert_len = 8;

		uint16_t lab_loc_word = lab_loc[jmp_dst[i]]/2;
		uint16_t jmp_loc_word = (jmp_off[i] + rel_insert_len)/2;
		uint16_t relative_jmp = lab_loc_word - jmp_loc_word;
		uint16_t jmp_start = jmp_off[i];

		uint8_t byte_0 = LOW(relative_jmp);
		uint8_t byte_1 = HIGH(relative_jmp);

		char jmp_instr[] = {
			HIGH(OP1(I_XOR, R_AC, R_AC, R_AC)),
			LOW(OP1(I_XOR, R_AC, R_AC, R_AC)),
			HIGH(OP2(I_ADDI, R_AC, byte_1)),
			LOW(OP2(I_ADDI, R_AC, byte_1)),
			HIGH(OP2(I_MULI, R_AC, 256)),
			LOW(OP2(I_MULI, R_AC, 256)),
			HIGH(OP2(I_ADDI, R_AC, byte_0)),
			LOW(OP2(I_ADDI, R_AC, byte_0))
		};

		mem_cpy(&cs, jmp_start, jmp_instr, rel_insert_len);
	}

	*c_len = mem_len(&cs);
	*c = mem_get(&cs);
	*rsym = rel_sym;
	*rflg = rel_flg;
	*roff = rel_off;
	*rcnt = rel_n;
	rel_sym = NULL;
	rel_flg = NULL;
	rel_off = NULL;
	rel_n = 0;
	rel_sz = 0;
	jmp_n = 0;
}

void i_done(void)
{
	free(jmp_off);
	free(jmp_dst);
	free(jmp_op);
	free(lab_loc);
}

long i_reg(long op, long *rd, long *r1, long *r2, long *r3, long *tmp)
{
	long oc = O_C(op);
	long bt = O_T(op);
	*rd = 0;
	*r1 = 0;
	*r2 = 0;
	*r3 = 0;
	*tmp = 0;
	if (oc & O_MOV) {
		*rd = R_TMPS;
		if (oc & (O_NUM | O_SYM))
			*r1 = 16;
		else
			*r1 = R_TMPS;
		return 0;
	}
	if (oc & O_ADD) {
		*r1 = R_TMPS;
		*r2 = oc & O_NUM ? 16 : R_TMPS;
		return 0;
	}
	if (oc & O_SHL) {
		if (oc & O_NUM) {
			*r1 = R_TMPS;
			*r2 = 8;
		} else {
			*r1 = R_TMPS;
			*r2 = R_TMPS;
		}
		return 0;
	}
	if (oc & O_MUL) {
		if (oc & O_NUM)
			return 1;
		*rd = R_TMPS;
		*r1 = R_TMPS;
		*r2 = R_TMPS;
		*tmp = 0;
		return 0;
	}
	if (oc & O_CMP) {
		*rd = R_TMPS;
		*r1 = R_TMPS;
		*r2 = oc & O_NUM ? 16 : R_TMPS;
		return 0;
	}
	if (oc & O_UOP) {
		if (oc == O_LNOT)
			*r1 = R_TMPS;
		else
			*r1 = R_TMPS;
		return 0;
	}
	if (oc == O_MSET) {
		*r1 = R_TMPS;
		*r2 = R_TMPS;
		*r3 = R_TMPS;
		*tmp = 0;
		return 0;
	}
	if (oc == O_MCPY) {
		*r1 = R_TMPS;
		*r2 = R_TMPS;
		*r3 = R_TMPS;
		*tmp = 0;
		return 0;
	}
	if (oc == O_RET) {
		*r1 = (1 << REG_RET);
		return 0;
	}
	if (oc & O_CALL) {
		*rd = (1 << REG_RET);
		*r1 = oc & O_SYM ? 0 : R_TMPS;
		*tmp = R_TMPS & ~R_PERM;
		return 0;
	}
	if (oc & O_LD) {
		*rd = R_TMPS;
		*r1 = R_TMPS;
		*r2 = oc & O_NUM ? 8 : R_TMPS;
		return 0;
	}
	if (oc & O_ST) {
		*r1 = R_TMPS;
		*r2 = R_TMPS;
		*r3 = oc & O_NUM ? 8 : R_TMPS;
		return 0;
	}
	if (oc & O_JZ) {
		*r1 = R_TMPS;
		return 0;
	}
	if (oc & O_JCC) {
		*r1 = R_TMPS;
		*r2 = oc & O_NUM ? 8 : R_TMPS;
		return 0;
	}
	if (oc == O_JMP) {
		return 0;
	}
	return 1;
}

int i_imm(long lim, long n)
{
	long max = (1 << (lim - 1)) - 1;
	return n <= max && n + 1 >= -max;
}

void print_op(long oc, long rd, long r1, long r2, long r3, long bt) {
	if (oc & O_ADD) {
		if (oc & O_SUB) {
			printf("O_SUB");
		}
		else if (oc & O_AND) {
			printf("O_AND");
		}
		else if (oc & O_OR) {
			printf("O_OR");
		}
		else if (oc & O_XOR) {
			printf("O_XOR");
		}
		else {
			printf("O_ADD");
		}
	}
	else if (oc & O_SHL) {
		if (oc == O_SHR) {
			printf("O_SHR");
		}
		else {
			printf("O_SHL");
		}
	}
	else if (oc & O_MUL) {
		if (oc == O_DIV) {
			printf("O_DIV");
		}
		else if (oc == O_MOD){
			printf("O_MOD");
		}
		else {
			printf("O_MUL");
		}
	}
	else if (oc & O_CMP) {
		if (oc == O_LT) {
			printf("O_LT");
		}
		else if (oc == O_GE) {
			printf("O_GE");
		}
		else if (oc == O_EQ) {
			printf("O_EQ");
		}
		else if (oc == O_NE) {
			printf("O_NE");
		}
		else if (oc == O_LE) {
			printf("O_LE");
		}
		else if (oc == O_GT) {
			printf("O_GT");
		}
		else {
			printf("O_CMP");
		}
	}
	else if (oc & O_UOP) {
		if (oc == O_NEG) {
			printf("O_NEG");
		}
		else if (oc == O_NOT) {
			printf("O_NOT");
		}
		else if (oc == O_LNOT) {
			printf("O_LNOT");
		}
		else {
			printf("O_UOP");
		}
	}
	else if (oc & O_CALL) {
		printf("O_CALL");
	}
	else if (oc & O_MOV) {
		printf("O_MOV");
	}
	else if (oc & O_MEM) {
		if (oc == O_MSET) {
			printf("O_MSET");
		}
		else if (oc == O_MCPY) {
			printf("O_MCPY");
		}
		else {
			printf("O_MEM");
		}
	}
	else if (oc & O_JMP) {
		printf("O_JMP");
	}
	else if (oc & O_JZ) {
		if (oc == O_JNZ) {
			printf("O_JNZ");
		}
		else {
			printf("O_JZ");
		}
	}
	else if (oc & O_JCC) {
		printf("O_JCC");
	}
	else if (oc & O_RET) {
		printf("O_RET");
	}
	else if (oc & O_LD) {
		printf("O_LD");
	}
	else if (oc & O_ST) {
		printf("O_ST");
	}

	if (oc & O_NUM) {
		printf("\tO_NUM");
	} else if (oc & O_LOC) {
		printf("\tO_LOC");
	} else if (oc & O_SYM) {
		printf("\tO_SYM");
	} else {
		printf("\tO_REG");
	}

	if (bt == T_MSIZE) {
		printf("\tT_MSIZE");
	} else if (bt == T_MSIGN) {
		printf("\tO_MSIGN");
	}

	printf("\t| rd: %ld, r1: %ld, r2: %ld,	r3: %ld\t\t| %x\n", rd, r1, r2, r3, oc);
}

long i_ins(long op, long rd, long r1, long r2, long r3)
{
	long oc = O_C(op);
	long t = op & 0xf;
	long bt = O_T(op);
	long offset = 0;

	print_op(oc, rd, r1, r2, r3, bt);

	if (oc & O_ADD) {
		if (oc & O_NUM) {
			i_add_anyimm(t, rd, r1, r2);
		} else {
			i_add(t, rd, r1, r2);
		}
		return 0;
	}

	if (oc & O_SHL) {
		if (oc & O_NUM) {
			if (oc == O_SHR) {
				printf("O_SHR");
				die("Shift right not yet supported");
			}
			else if (r2 < 16) {
				int mul = 1;
				for (int i = 0; i < r2; i++) {
					mul *= 2;
				}
				i_load_acc_imm(mul);
				op_typ(I_MUL, r1, R_AC, rd);
				return 0;
			}
		}
		die("Shift not yet supported");
		return 0;
	}

	if (oc & O_MUL) {
		if (oc == O_MUL) {
			i_mul(rd, r1, r2);
		}
		if (oc == O_DIV) {
			die("div");
		}
		if (oc == O_MOD) {
			die("mod");
		}
		return 0;
	}

	if (oc & O_CMP) {
		die("cmp");
		if (oc & O_NUM) {}
		else {}
		return 0;
	}

	if (oc & O_UOP) {
		if (oc == O_NEG){
			op_typ(I_NEG, r1, r1, 0);
		}
		if (oc == O_NOT || oc == O_LNOT){
			i_load_acc_imm(1);
			op_typ(I_NEG, R_AC, R_AC, 0);
			op_typ(I_XOR, r1, R_AC, r1);
		}
		return 0;
	}

	if (oc == O_CALL) {
		die("nonsymbolic call");
		return 0;
	}

	if (oc == (O_CALL | O_SYM)) {
		// We want to store PC + 11 on the stack
		op_typ(I_XOR, R_AC, R_AC, R_AC);
		op_imm(I_ADDI, R_AC, 11);
		op_typ(I_ADD, R_PC, R_AC, R_CMP); // R_CMP contains PC to save

		// increment RSP and store PC to save
		op_typ(I_XOR, R_AC, R_AC, R_AC);
		op_imm(I_ADDI, R_AC, 1);
		op_typ(I_NEG, R_AC, R_AC, 0);
		op_typ(I_ADD, R_SP, R_AC, R_SP);
		op_typ(I_ST, R_CMP, R_SP, 0);
		op_typ(I_ADD, R_SP, R_AC, R_SP);

		i_rel(r1, OUT_CS | OUT_RLREL, opos());
		oi(0, 8);
		oi(OP3(I_JMP, R_AC, 0, JMP_ABS), 2);
		return 0;
	}

	if (oc == (O_MOV | O_SYM)) {
		die("symbolic move");
		return 0;
	}

	if (oc == (O_MOV | O_NUM)) {
		i_load_acc_imm(r1);
		op_typ(I_MOV, R_AC, rd, 0);
		return 0;
	}

	if (oc == O_MOV) {
		i_cpy_reg(r1, rd);
		return 0;
	}

	if (oc == O_MSET) {
		die("mset");
		return 0;
	}

	if (oc == O_MCPY) {
		die("mcpy");
		return 0;
	}

	if (oc == O_RET) {
		offset = i_jmp(op, 0, 0);
		jmp_add(O_JMP, offset, 0);
		return 0;
	}

	if (oc == (O_LD | O_NUM)) {
		i_ld_num(rd, r1, r2);
		return 0;
	}

	if (oc == (O_ST | O_NUM)) {
		i_st_num(r1, r2, r3);
		return 0;
	}

	if (oc & O_JXX) {
		offset = i_jmp(op, r1, r2);
		jmp_add(op, offset, r3 + 1);
		return 0;
	}

	return 1;
}
