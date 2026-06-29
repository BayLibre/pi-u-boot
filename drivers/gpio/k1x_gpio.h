#ifndef __K1X_GPIO_H__
#define __K1X_GPIO_H__

#include <linux/types.h>

/*
 * GPIO Register offsets structure for different platforms
 */
struct spacemit_gpio_reg_offsets {
	u32 gplr;	/* Pin Level Register */
	u32 gpdr;	/* Pin Direction Register */
	u32 gpsr;	/* Pin Output Set Register */
	u32 gpcr;	/* Pin Output Clear Register */
	u32 grer;	/* Rising-Edge Detect Enable Register */
	u32 gfer;	/* Falling-Edge Detect Enable Register */
	u32 gedr;	/* Edge Detect Status Register */
	u32 gsdr;	/* Bitwise Set of GPIO Direction Register */
	u32 gcdr;	/* Bitwise Clear of GPIO Direction Register */
	u32 gsrer;	/* Bitwise Set of Rising-Edge Detect Enable Register */
	u32 gcrer;	/* Bitwise Clear of Rising-Edge Detect Enable Register */
	u32 gsfer;	/* Bitwise Set of Falling-Edge Detect Enable Register */
	u32 gcfer;	/* Bitwise Clear of Falling-Edge Detect Enable Register */
	u32 gapmask;	/* Bitwise Mask of Edge Detect Register */
	u32 gcpmask;	/* Bitwise Clear Mask of Edge Detect Register */
};

/* Chip-specific data structure */
struct spacemit_gpio_chip_data {
	const struct spacemit_gpio_reg_offsets *regs;
	const unsigned long *bank_offsets;
	unsigned int num_banks;
	unsigned int max_gpio;
};



#endif /* __K1X_GPIO_H__ */
