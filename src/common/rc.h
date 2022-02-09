#pragma once

#include <common/defines.h>
#include <common/lutil.h>

typedef struct _REF_COUNT {
    isize count;
} RefCount;

#define REF_COUNT struct _REF_COUNT

// initialize reference count to zero.
void init_rc(RefCount *rc);

// atomic increment reference count by one.
BOOL increment_rc(RefCount *rc);

// atomic decrement reference count by one. Returns true if reference
// count becomes zero or below.
BOOL decrement_rc(RefCount *rc);
