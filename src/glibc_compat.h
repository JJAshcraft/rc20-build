/* Force-included in the aarch64 build: binds math functions to their original
 * glibc 2.17 versions so the binary runs on handheld OS images with an older
 * glibc than the build machine. */
#include <features.h>
#if defined(__aarch64__) && defined(__GLIBC__)
__asm__(".symver exp,exp@GLIBC_2.17");
__asm__(".symver log,log@GLIBC_2.17");
__asm__(".symver pow,pow@GLIBC_2.17");
__asm__(".symver fmod,fmod@GLIBC_2.17");
__asm__(".symver fmodf,fmodf@GLIBC_2.17");
__asm__(".symver expf,expf@GLIBC_2.17");
__asm__(".symver exp2f,exp2f@GLIBC_2.17");
__asm__(".symver logf,logf@GLIBC_2.17");
__asm__(".symver powf,powf@GLIBC_2.17");
__asm__(".symver log10f,log10f@GLIBC_2.17");
#endif
