/* architecture-dependent code generation for Henlo */
#include <stdlib.h>
#include <stdio.h>
#include "ncc.h"

/* Hardware registers */
#define R_R0		0x00
#define R_R1		0x01
#define R_FP		0x02
#define R_ACC		0x03
#define R_SP		0x04

#define REG_RET		R_R0

/* Henlo opcodes */
#define I_ADD		0x0
#define I_ADDI	0x1
#define I_MUL		0x2
#define I_MULI	0x3
#define I_LD		0x4
#define I_ST		0x5
#define I_ASP   0x6
#define I_RSP   0x7
#define I_BNEZ	0x8
#define I_JMP		0x9
#define I_SH48  0xa
#define I_AND   0xb
#define I_OR    0xc
#define I_XOR   0xd
#define I_NEG   0xe

#define MIN(a, b)		((a) < (b) ? (a) : (b))
#define ALIGN(x, a)		(((x) + (a) - 1) & ~((a) - 1))

// oooo||r1||r2
#define OP1(op, r1, r2)	(((op) & 0xf) << 4) | (((r1) & 0x3) << 2) | ((r2) & 0x3)

// oooo||imm (R_ACC implicit)
#define OP2(op, imm)	(((op) & 0xf) << 4) | ((imm) & 0xf)

// oooo||r1||00
#define OP3(op, r1)	(((op) & 0xf) << 4) | (((r1) & 3) << 2)

int tmpregs[] = {0, 1, 2};
int argregs[] = {};

static struct mem cs;		/* generated code */

/* code generation functions */
static void os(void *s, int n)
{
	mem_put(&cs, s, n);
}

static char *ointbuf(long n, int l)
{
	static char buf[16];
	int i;
	for (i = 0; i < l; i++) {
		buf[i] = n & 0xff;
    printf("I: %02x\n", buf[i] & 0xff);
		n >>= 8;
	}
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

static void op_typ(int op, int r1, int r2) {
	oi(OP1(op, r1, r2), 1);
}

static void op_imm(int op, int imm) {
	oi(OP2(op, imm), 1);
}

static void op_uny(int op, int r1) {
	oi(OP3(op, r1), 1);
}

static void op_shl(int n) {
  if (n == 4) {
    oi(OP2(I_SH48, 12), 1);
  } else if (n == 8) {
    oi(OP2(I_SH48, 14), 1);
  }
}

static void op_shr(int n) {
  if (n == 4) {
    oi(OP2(I_SH48, 4), 1);
  } else if (n == 8) {
    oi(OP2(I_SH48, 6), 1);
  }
}

static void mov_r2r(int rd, int r1, unsigned bt)
{
}

static void i_push(int reg)
{
}

static void i_pop(int reg)
{
}

static void i_mov(int rd, int rn)
{
}

static void i_shl_imm(int op, int rd, int rn, long n)
{
}

static void i_not(int rd)
{
}

static void i_set(long op, int rd)
{
}

static void i_lnot(int rd)
{
}

static void jx(int x, int nbytes)
{
}

static void i_cpy_reg(int src, int dst) {
  if (src == dst) {
    return;
  }
  if (src == R_ACC) {
    op_typ(I_XOR, R_ACC, dst);
    op_typ(I_XOR, dst, dst);
  } else {
    op_imm(I_MULI, 0);
    op_typ(I_XOR, src, dst);
  }
}

/* generate cmp or tst before a conditional jump */
static void i_jcmp(long op, long rn, long rm)
{
}

/* generate a jump instruction and return the of its displacement */
static long i_jmp(long op, int nb)
{
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

// Load 16 bit constant into r_acc.
static void i_load_acc_imm(int n) {
  if (n > 65535) {
    printf("immediate larger than 16 bits!\n");
    exit(-1);
  }

  int nibble_0 = n & 0xF;
	int nibble_1 = (n >> 4) & 0xF;
	int nibble_2 = (n >> 8) & 0xF;
	int nibble_3 = (n >> 12) & 0xF;

	// 0 out accumulator
	op_imm(I_MULI, 0);

  // shift immediate into accumulator
  if (n > 4095) {
    op_imm(I_ADDI, nibble_3);
    op_shl(4);
  }

  if (n > 255) {
    op_imm(I_ADDI, nibble_2);
    op_shl(4);
  }

  if (n > 15) {
    op_imm(I_ADDI, nibble_1);
    op_shl(4);
  }

  if (n > 0) {
    op_imm(I_ADDI, nibble_0);
  }
}

static i_ld_num(long rd, long r1, long r2)
{
  i_cpy_reg(r1, R_ACC);
  op_typ(I_ADD, r2, R_ACC);
  op_typ(I_LD, R_ACC, rd);
}

static i_st_num(long r1, long r2, long r3)
{
  i_cpy_reg(r2, R_ACC);
  op_typ(I_ADD, r3, R_ACC);
  op_typ(I_ST, r1, R_ACC);
}

static void i_mul(long rd, long r1, long r2)
{
  i_cpy_reg(r2, R_ACC);
  op_typ(I_MUL, r1, rd);
}

static void i_add(long op, long rd, long r1, long r2)
{
  if (op == O_SUB) {
    i_cpy_reg(r2, R_ACC);
    op_uny(I_NEG, R_ACC);
    op_typ(I_ADD, r1, rd);
  } else if (op == O_AND) {
    i_cpy_reg(r2, R_ACC);
    op_typ(I_AND, r1, rd);
  } else if (op == O_OR) {
    i_cpy_reg(r2, R_ACC);
    op_typ(I_OR, r1, rd);
  } else if (op == O_XOR) {
    i_cpy_reg(r2, R_ACC);
    op_typ(I_XOR, r1, rd);
  } else {
    i_cpy_reg(r2, R_ACC);
    op_typ(I_ADD, r1, rd);
  }
}

static void i_add_anyimm(long op, int rd, int rn, long n)
{
  // copy rn into result register
  i_cpy_reg(rn, rd);

  // load imm into accumulator
  i_load_acc_imm(n);

  if (op == O_SUB) {
    op_uny(I_NEG, R_ACC);
  }

  // add accumulator to destination
	op_typ(I_ADD, rd, rd);
}

static long *rel_sym;		/* relocation symbols */
static long *rel_flg;		/* relocation flags */
static long *rel_off;		/* relocation offsets */
static long rel_n, rel_sz;	/* relocation count */

static long lab_sz;		/* label count */
static long *lab_loc;		/* label offsets in cs */
static long jmp_n, jmp_sz;	/* jump count */
static long *jmp_off;		/* jump offsets */
static long *jmp_dst;		/* jump destinations */
static long *jmp_op;		/* jump opcode */
static long jmp_ret;		/* the position of the last return jmp */

static void lab_add(long id)
{
	while (id >= lab_sz) {
		int lab_n = lab_sz;
		lab_sz = MAX(128, lab_sz * 2);
		lab_loc = mextend(lab_loc, lab_n, lab_sz, sizeof(*lab_loc));
	}
	lab_loc[id] = opos();
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

static void regs_save(long sregs, long dis)
{
	int i;
	for (i = 0; i < N_REGS; i++)
		if (((1 << i) & R_TMPS) & sregs)
			i_push(i);
	if (dis)
		i_subsp(dis);
}

static void regs_load(long sregs, long dis)
{
	int i;
	if (dis)
		i_subsp(-dis);
	for (i = N_REGS - 1; i >= 0; --i)
		if (((1 << i) & R_TMPS) & sregs)
			i_pop(i);
}

void i_wrap(int argc, long sargs, long spsub, int initfp, long sregs, long sregs_pos)
{
}

/* introduce shorter jumps, if possible */
static void i_shortjumps(int *nb)
{
}

void i_code(char **c, long *c_len, long **rsym, long **rflg, long **roff, long *rcnt)
{
  int *nb;	/* number of bytes necessary for jump displacements */
	int i;
	/* more compact jmp instructions */
	nb = malloc(jmp_n * sizeof(nb[0]));
	for (i = 0; i < jmp_n; i++)
		nb[i] = 4;
	i_shortjumps(nb);
  printf("filling\n");
  fflush(stdout);
	for (i = 0; i < jmp_n; i++) /* filling jmp destinations */ {
		// oi_at(jmp_off[i], 0xff, 9);
  }
	free(nb);
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
		*r2 = R_TMPS;
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
		*r1 = R_TMPS;
		return 0;
	}
	if (oc & O_CALL) {
		*rd = (1 << REG_RET);
		*r1 = oc & O_SYM ? 0 : R_TMPS;
		*tmp = 0;
		return 0;
	}
	if (oc & O_LD) {
		*rd = R_TMPS;
		*r1 = R_TMPS;
		*r2 = oc & O_NUM ? 16 : R_TMPS;
		return 0;
	}
	if (oc & O_ST) {
		*r1 = R_TMPS;
		*r2 = R_TMPS;
		*r3 = oc & O_NUM ? 16 : R_TMPS;
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
	if (oc == O_JMP)
		return 0;
	return 1;
}

int i_imm(long lim, long n)
{
	long max = (1 << (lim - 1)) - 1;
	return n <= max && n + 1 >= -max;
}

void print_op(long oc, long rd, long r1, long r2, long r3, long bt) {
  if (oc & O_ADD) {
    if (oc == O_SUB) {
      printf("O_SUB");
    }
    else if (oc == O_AND) {
      printf("O_SUB");
    }
    else if (oc == O_OR) {
      printf("O_OR");
    }
    else if (oc == O_XOR) {
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

  printf("\t| rd: %ld, r1: %ld, r2: %ld,  r3: %ld\t\t| %x\n", rd, r1, r2, r3, oc);
}

long i_ins(long op, long rd, long r1, long r2, long r3)
{
	long oc = O_C(op);
  long t = op & 0xf;
	long bt = O_T(op);

  print_op(oc, rd, r1, r2, r3, bt);

	if (oc & O_ADD) {
		if (oc & O_NUM) {
			i_add_anyimm(t, rd, r1, r2);
		} else {
			i_add(t, r1, r1, r2);
		}
    return 0;
	}

	if (oc & O_SHL) {
		if (oc & O_NUM) {
      if (r2 != 4 && r2 != 8) {
        printf("Shift by !(4 || 8) not supported");
        exit(-1);
      }

      i_cpy_reg(r1, R_ACC);

      if (op == O_SHR) {
        op_shr(r2);
      } else {
        op_shl(r2);
      }

      i_cpy_reg(R_ACC, rd);
    }
		else {}
    return 0;
	}

	if (oc & O_MUL) {
		if (oc == O_MUL) {
      i_mul(rd, r1, r2);
    }
		if (oc == O_DIV) {}
		if (oc == O_MOD) {}
		return 0;
	}

  if (oc & O_CMP) {
		if (oc & O_NUM) {}
		else {}
		return 0;
	}

  if (oc & O_UOP) {
		if (oc == O_NEG){
      i_cpy_reg(r1, R_ACC);
      op_uny(I_NEG, rd);
    }
		if (oc == O_NOT || oc == O_LNOT){
      i_load_acc_imm(1);
      op_uny(I_NEG, R_ACC);
      op_typ(I_XOR, r1, rd);
    }
		return 0;
	}

  if (oc == O_CALL) {
		return 0;
	}

  if (oc == (O_CALL | O_SYM)) {
		i_rel(r1, OUT_CS | OUT_RLREL, opos());
    oi(0, 9); // Filler. Takes 9 instructions to load address and jump
		return 0;
	}

  if (oc == (O_MOV | O_SYM)) {
		return 0;
	}

  if (oc == (O_MOV | O_NUM)) {
    op_imm(I_MULI, 0);
    op_typ(I_MUL, R_ACC, rd);
    i_load_acc_imm(r1);
    op_typ(I_ADD, rd, rd);
    return 0;
	}

  if (oc == O_MOV) {
    i_cpy_reg(r1, rd);
    return 0;
  }

  if (oc == O_MSET) {
		return 0;
	}

  if (oc == O_MCPY) {
		return 0;
	}

  if (oc == O_RET) {
		jmp_ret = opos();
		jmp_add(O_JMP, 9, 0);
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
		return 0;
	}

  return 1;
}
