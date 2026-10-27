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
