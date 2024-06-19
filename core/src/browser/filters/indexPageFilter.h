#pragma once

#include "include/capi/cef_response_filter_capi.h"

typedef struct indexPageFilter {
    cef_response_filter_t base;
    int count;

    indexPageFilter();
};
