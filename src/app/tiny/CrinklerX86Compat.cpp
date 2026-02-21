#if defined(_M_IX86)
extern "C" int __cdecl _filter_x86_sse2_floating_point_exception() {
    return 0;
}

extern "C" int __cdecl __filter_x86_sse2_floating_point_exception() {
    return _filter_x86_sse2_floating_point_exception();
}
#endif
