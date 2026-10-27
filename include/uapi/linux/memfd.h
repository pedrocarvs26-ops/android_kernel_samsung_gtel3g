#ifndef _UAPI_LINUX_MEMFD_H
#define _UAPI_LINUX_MEMFD_H

/* flags for memfd_create(2) (unsigned int) */
#define MFD_CLOEXEC		0x0001U
#define MFD_ALLOW_SEALING	0x0002U
#define MFD_HUGETLB		0x0004U

#endif /* _UAPI_LINUX_MEMFD_H */
