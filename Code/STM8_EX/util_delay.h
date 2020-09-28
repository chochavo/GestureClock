#include <intrinsics.h>
#if !defined(F_CPU)
#warning no F_CPU
#endif

#define delay_us(us) __delay_cycles((F_CPU  *(us)/4000000uL)