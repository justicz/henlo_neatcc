/* architecture-dependent header for x86 */
#define LONGSZ		2	/* word size */
#define I_ARCH		"__henlo__"

#define N_REGS		8	/* number of registers */
#define N_TMPS		3	/* number of tmp registers */
#define N_ARGS		0	/* number of arg registers */
#define R_TMPS		0x0007	/* mask of tmp registers */
#define R_ARGS		0x0000	/* mask of arg registers */
#define R_PERM		0x0000	/* mask of callee-saved registers */

#define REG_FP		4	/* frame pointer register */
#define REG_SP		5	/* stack pointer register */

#define I_ARG0		(-8)	/* offset of the first argument from FP */
#define I_LOC0		0	/* offset of the first local from FP */
