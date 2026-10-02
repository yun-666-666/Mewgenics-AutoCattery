#pragma once
#include <stdint.h>

/* Current executable 274360 maps Beanies/Butch/Tink/Frank/babyjack/Tracy/
   OrganGrinder to 0/1/2/3/4/5/6. The mask comes from native 276600. */
static inline int AcMewSelectPopulationRecipient(uint32_t accepts, int dead) {
    static const int order[] = {1, 3, 0, 2, 5, 4};
    unsigned int index;
    if (dead) return (accepts & (1U << 6)) ? 6 : -1;
    /* Frank only accepts retired cats. Butch precedes him to implement the
       user's exception for a retired cat that also meets Butch's requirements. */
    for (index = 0; index < sizeof(order) / sizeof(order[0]); ++index)
        if (accepts & (1U << order[index])) return order[index];
    return (accepts & (1U << 7)) ? 7 : -1;
}
