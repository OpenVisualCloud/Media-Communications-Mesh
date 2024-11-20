#ifndef LIBFABRIC_MOCKS_H
#define LIBFABRIC_MOCKS_H

#include <fff.h>
#include <rdma/fabric.h>

DECLARE_FAKE_VALUE_FUNC(int, fi_getinfo, uint32_t, const char *, const char *, uint64_t,
                        const struct fi_info *, struct fi_info **);
DECLARE_FAKE_VOID_FUNC(fi_freeinfo, struct fi_info *);
DECLARE_FAKE_VALUE_FUNC(int, custom_close, struct fid *);
DECLARE_FAKE_VALUE_FUNC(int, custom_bind, struct fid *, struct fid *, uint64_t);
DECLARE_FAKE_VALUE_FUNC(int, control, struct fid *, int, void *); // fi_enable
DECLARE_FAKE_VALUE_FUNC(ssize_t, cq_read, struct fid_cq *, void *, size_t);

int fi_getinfo_custom_fake(uint32_t version, const char *node, const char *service, uint64_t flags,
                           const struct fi_info *hints, struct fi_info **fi);

#endif /*LIBFABRIC_MOCKS_H*/
