#ifndef SRC_DEBUG_ASSERT_H
#define SRC_DEBUG_ASSERT_H

// #define NDEBUG // uncomment to disable assert()

#include <exception>

#ifdef NDEBUG
#define debug_assert(expression, message) ((void)0)
#else
#define debug_assert(x, message) dbg_assert((x), (message))

template <typename T, typename E = std::exception>
inline void dbg_assert(T &&assertion, const E throwing = {})
{
    if (!std::forward<T>(assertion))
    {
        throw throwing;
    }
}
#endif

#endif
