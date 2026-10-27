#include <sys/types.h>

/* Temporary link stubs for the first merlinx kernel build. */
void
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
