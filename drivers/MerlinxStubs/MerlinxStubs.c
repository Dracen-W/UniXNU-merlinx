#include <sys/types.h>

/* Temporary link stubs for the first merlinx kernel build. */
__attribute__((weak)) void
native_legacy_display_probe (
  void
  )
{
}

/* Merlnx uses the eMMC block driver. NativeRamBlock persistence is UFS-only, so keep it volatile. */
unsigned
native_ufs_store_mode(void)
{
	return 0;
}

int
native_ufs_store_open(unsigned long long offset, unsigned long long *first, const char **why)
{
	(void)offset; (void)first; (void)why;
	return 0;
}

int
native_ufs_store_read(unsigned long long block, unsigned int count, void *data)
{
	(void)block; (void)count; (void)data;
	return 0;
}

int
native_ufs_store_write(unsigned long long block, unsigned int count, const void *data)
{
	(void)block; (void)count; (void)data;
	return 0;
}

int
native_ufs_store_write_cached(unsigned long long block, unsigned int count, const void *data)
{
	(void)block; (void)count; (void)data;
	return 0;
}

/* Weak fallback for RamBlock >= 00a6e5dd's optional boot-payload-release=1 hook.
 * Returns false so the loader seed is left untouched unless XNU provides the real symbol. */
__attribute__((weak)) _Bool
ml_static_mfree_prepare(unsigned long vaddr, unsigned long size)
{
	(void)vaddr; (void)size;
	return 0;
}

int
native_ufs_store_sync(void)
{
	return 0;
}

unsigned
native_ufs_store_gpt_check(void)
{
	return 0;
}

/* Bring-up stub: NativeEmmc has no verified-manifest backend yet. */
int
native_verified_block_manifest_sha384(dev_t device, const char *uuid, unsigned char digest[48])
{
	unsigned int i;
	(void)device;
	(void)uuid;
	for (i = 0; i < 48; i++) digest[i] = 0;
	return 1;
}

/* Weak link stubs for optional upstream GopFramebuffer (b0d812d5) and ArmCpu (5f370269)
 * weak_import hooks. Static XNU kernel linking rejects unresolved weak_import symbols. */
__attribute__((weak)) _Bool
native_adreno_fences(unsigned int *submitted, unsigned int *retired)
{
	(void)submitted; (void)retired;
	return 0;
}

__attribute__((weak)) _Bool
native_adreno_fences_detail(unsigned int *a, unsigned int *b, unsigned int *c)
{
	(void)a; (void)b; (void)c;
	return 0;
}

__attribute__((weak)) int
native_gpu_trace_enabled(void)
{
	return 0;
}

__attribute__((weak)) void
native_gpu_trace_record(unsigned int event, unsigned int pid,
    const unsigned long long *values, unsigned int count)
{
	(void)event; (void)pid; (void)values; (void)count;
}

__attribute__((weak)) void (*arm64_platform_sleep_hook)(int boot_cpu) = 0;
