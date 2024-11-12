
#include "libfabric_mocks.h"

DEFINE_FFF_GLOBALS;

DEFINE_FAKE_VALUE_FUNC(int, fi_getinfo, uint32_t, const char *, const char *, uint64_t,
                       const struct fi_info *, struct fi_info **);
DEFINE_FAKE_VOID_FUNC(fi_freeinfo, struct fi_info *);
DEFINE_FAKE_VALUE_FUNC(int, custom_close, struct fid *);
DEFINE_FAKE_VALUE_FUNC(int, custom_bind, struct fid *, struct fid *, uint64_t);
DEFINE_FAKE_VALUE_FUNC(int, control, struct fid *, int, void *); // fi_enable
DEFINE_FAKE_VALUE_FUNC(ssize_t, cq_read, struct fid_cq *, void *, size_t);

int fi_getinfo_custom_fake(uint32_t version, const char *node, const char *service, uint64_t flags,
                           const struct fi_info *hints, struct fi_info **fi)
{
    *fi = fi_dupinfo(hints);
    if (!flags)
        (*fi)->dest_addr = (void *)0xdeadbeef;
    return 0;
}
