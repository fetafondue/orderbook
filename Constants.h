#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <limits>

#include "Alias.h"

struct Constants
{
    static const Price InvalidPrice = std::numeric_limits<Price>::quiet_NaN();
};

#endif