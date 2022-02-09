#include <common/rc.h>
#include <mod/bug.h>

void init_rc(RefCount *rc) {
    rc->count = 0;
}

BOOL increment_rc(RefCount *rc) {
    i64 r = __atomic_fetch_add(&rc->count, 1, __ATOMIC_ACQ_REL);
    if (r > OBJECT_MAX_REFERENCE)
    {
        decrement_rc(rc);
        return FALSE;
    }
    return TRUE;
}

BOOL decrement_rc(RefCount *rc) {
    i64 r = __atomic_sub_fetch(&rc->count, 1, __ATOMIC_ACQ_REL);
    #ifndef USE_LAGACY
    ASSERT(r >= 0, BUG_BADREF);
    #endif
    return r <= 0;
}
