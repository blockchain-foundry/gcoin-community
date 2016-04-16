// The definition of 'error' macro is moved here to prevent problems.
// Other macros that don't cause problems are in util.h.
#define error(...) errorWithLocation(__FILE__, STRINGIFY(__LINE__), \
    __func__, BOOST_CURRENT_FUNCTION, __VA_ARGS__)
