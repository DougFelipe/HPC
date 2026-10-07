#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
static void feature(const char *name, int hw, int usable) {
    printf("%s hardware=%d os_usable=%d\n", name, !!hw, !!usable);
}
#endif
int main(void) {
#if defined(__i386__) || defined(__x86_64__)
    unsigned a=0,b=0,c=0,d=0, ecx=0,edx=0,ebx7=0;
    __get_cpuid(1,&a,&b,&c,&d); ecx=c; edx=d;
    if(__get_cpuid_max(0,0)>=7) { __cpuid_count(7,0,a,b,c,d); ebx7=b; }
    uint64_t xcr0=0;
    if(ecx & (1u<<27)) { unsigned lo,hi; __asm__ volatile("xgetbv":"=a"(lo),"=d"(hi):"c"(0)); xcr0=((uint64_t)hi<<32)|lo; }
    printf("source=CPUID + OSXSAVE + XGETBV; XCR0=0x%llx\n",(unsigned long long)xcr0);
    int avx_os=(ecx&(1u<<27)) && ((xcr0&6)==6);
    int zmm_os=avx_os && ((xcr0&0xe0)==0xe0);
    /* On x86 Windows, query SSE OS support independently of CPUID. */
    int sse_os=0;
#ifdef _WIN32
    sse_os=IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE) && IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE);
#else
    /* SSE ABI baseline in a running x86_64 process; unknown on 32-bit Linux. */
    sse_os=(sizeof(void*)==8);
#endif
    feature("SSE",edx&(1u<<25),(edx&(1u<<25))&&sse_os);
    feature("SSE2",edx&(1u<<26),(edx&(1u<<26))&&sse_os);
    feature("SSE3",ecx&1,(ecx&1)&&sse_os);
    feature("SSSE3",ecx&(1u<<9),(ecx&(1u<<9))&&sse_os);
    feature("SSE4.1",ecx&(1u<<19),(ecx&(1u<<19))&&sse_os);
    feature("SSE4.2",ecx&(1u<<20),(ecx&(1u<<20))&&sse_os);
    feature("AVX",ecx&(1u<<28),(ecx&(1u<<28))&&avx_os);
    feature("AVX2",ebx7&(1u<<5),(ebx7&(1u<<5))&&avx_os);
    feature("AVX512F",ebx7&(1u<<16),(ebx7&(1u<<16))&&zmm_os);
    feature("FMA",ecx&(1u<<12),(ecx&(1u<<12))&&avx_os);
    feature("AES",ecx&(1u<<25),(ecx&(1u<<25))&&sse_os);
    printf("CPUID.1.ECX=0x%08x EDX=0x%08x CPUID.7.0.EBX=0x%08x\n",ecx,edx,ebx7);
    if(__get_cpuid_max(0,0)>=0x16) { __cpuid(0x16,a,b,c,d); printf("CPUID.16 base_MHz=%u max_MHz=%u bus_MHz=%u (zero=unreported)\n",a&65535,b&65535,c&65535); }
#else
    puts("SIMD: nao identificado (probe requires x86)");
#endif
#ifdef _WIN32
    HMODULE lib=LoadLibraryA("nvcuda.dll");
    if(lib) {
        typedef int (__stdcall *Init)(unsigned);
        typedef int (__stdcall *Count)(int*);
        typedef int (__stdcall *Device)(int*,int);
        typedef int (__stdcall *Attr)(int*,int,int);
        Init init=(Init)GetProcAddress(lib,"cuInit");
        Count count=(Count)GetProcAddress(lib,"cuDeviceGetCount");
        Device device=(Device)GetProcAddress(lib,"cuDeviceGet");
        Attr attr=(Attr)GetProcAddress(lib,"cuDeviceGetAttribute");
        if(init && count && device && attr) {
            int status=init(0),n=0; printf("CUDA Driver API cuInit_status=%d\n",status);
            if(status==0 && count(&n)==0) for(int i=0;i<n;i++) {
                int dev,sm,major,minor;
                if(device(&dev,i)==0 && attr(&sm,16,dev)==0 && attr(&major,75,dev)==0 && attr(&minor,76,dev)==0)
                    printf("CUDA GPU %d SMs=%d compute_capability=%d.%d (cuDeviceGetAttribute)\n",i,sm,major,minor);
            }
        } else puts("CUDA Driver API symbols: nao identificado");
        FreeLibrary(lib);
    } else puts("CUDA Driver API: nao identificado");
#endif
    return 0;
}
