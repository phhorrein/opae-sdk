/*++

  INTEL CONFIDENTIAL
  Copyright 2016 - 2017 Intel Corporation

  The source code contained or described  herein and all documents related to
  the  source  code  ("Material")  are  owned  by  Intel  Corporation  or its
  suppliers  or  licensors.  Title   to  the  Material   remains  with  Intel
  Corporation or  its suppliers  and licensors.  The Material  contains trade
  secrets  and  proprietary  and  confidential  information  of Intel  or its
  suppliers and licensors.  The Material is protected  by worldwide copyright
  and trade secret laws and treaty provisions. No part of the Material may be
  used,   copied,   reproduced,   modified,   published,   uploaded,  posted,
  transmitted,  distributed, or  disclosed in  any way  without Intel's prior
  express written permission.

  No license under any patent, copyright,  trade secret or other intellectual
  property  right  is  granted to  or conferred  upon  you by  disclosure  or
  delivery of the  Materials, either  expressly, by  implication, inducement,
  estoppel or otherwise. Any license  under such intellectual property rights
  must be express and approved by Intel in writing.

  --*/

/*
 * Mock up driver interactions for testing
 *
 * Involves redefining ioctl(), open(), close(), others?
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <opae/types.h>
#include "common_int.h"
#include "intel-fpga.h"
#include <assert.h>
#include <safe_string/safe_string.h>

#define __USE_GNU
#include <dlfcn.h>

#define MAX_FD 1024
#define MAX_STRLEN 256
#define FPGA_MOCK_IOVA 0xDECAFBADDEADBEEF
#define FPGA_MOCK_NUM_UMSGS 8
#define FPGA_MOCK_DEV_PATH "/tmp"
#define MOCK_SYSFS_FPGA_CLASS_PATH "/tmp/class/fpga"
#define FPGA_FME_DEV_PREFIX "intel-fpga-fme."
#define FPGA_PORT_DEV_PREFIX "intel-fpga-port."
#define HASH_SUFFIX ".gbshash"

#undef FPGA_MSG
#define FPGA_MSG(fmt, ...) \
	printf("MOCK " fmt "\n", ## __VA_ARGS__)

#undef FPGA_ERR
#define FPGA_ERR(fmt, ...) \
	printf("MOCK ERROR " fmt "\n", ## __VA_ARGS__)

#undef FPGA_DBG
#ifdef LIBFPGA_DEBUG
#define FPGA_DBG(fmt, ...) \
	printf("MOCK DEBUG " fmt "\n", ## __VA_ARGS__)
#else
#define FPGA_DBG(fmt, ...) {}
#endif

/* TODO: track mock devices with dynamic data structure */
static struct mock_dev {
	int valid;
	fpga_objtype objtype;
	char pathname[MAX_STRLEN];
} mock_devs[MAX_FD] = {0};

typedef int (*open_func)(const char *pathname, int flags);
typedef int (*open_mode_func)(const char *pathname, int flags, mode_t m);

typedef int (*close_func)(int fd);
typedef int (*ioctl_func)(int fd, unsigned long request, char *argp);
typedef DIR * (*opendir_func)(const char *name);
typedef ssize_t (*readlink_func)(const char *pathname, char *buf, size_t bufsiz);
typedef int (*__xstat_func)(int ver, const char *pathname, struct stat *buf);


uint32_t stupid_hash(uint32_t *buf, uint32_t len_in_words) {

	uint32_t i;
	uint32_t hash = 0;

	for (i = 0; i < len_in_words; ++i)
		hash ^= buf[i];

	return hash;
}

static char* rewrite_sysfs_path(const char* src, char* dst, int len) {
    int prefix_len = strlen(SYSFS_FPGA_CLASS_PATH);

    if (strncmp(SYSFS_FPGA_CLASS_PATH, src, prefix_len) == 0) {
        strncpy_s(dst, len, MOCK_SYSFS_FPGA_CLASS_PATH, strlen(MOCK_SYSFS_FPGA_CLASS_PATH));
        strncpy_s(dst + prefix_len, len - prefix_len, src + prefix_len, len - prefix_len);
    } else {
        strncpy_s(dst, len, src, len);
    }

    return dst;
}

int ioctl(int fd, unsigned long request, ...)
{
	va_list argp;
	int retval = -1;
	char *err;
	errno = EINVAL;
	uint32_t hash;
	char hashfilename[MAX_STRLEN];

	va_start(argp, request);

	/* check where ioctl is going */
	if (fd >= MAX_FD || !mock_devs[fd].valid) {
		FPGA_DBG("real ioctl() called");
		dlerror(); /* clear errors */
		ioctl_func real_ioctl = (ioctl_func)dlsym(RTLD_NEXT, "ioctl");
		err = dlerror();

		if (NULL != err){
			FPGA_ERR("dlsym() failed: %s", err);
			goto out_EINVAL;
		}
		char *arg = va_arg(argp, char *);

        if (NULL != real_ioctl) {
            return real_ioctl(fd, request, arg);
        }
    }

	if (fd >= MAX_FD)
		return -1;

	FPGA_DBG("mock ioctl() called");
	switch (mock_devs[fd].objtype) {
	case FPGA_DEVICE: /* FME */
		switch (request) {
		case FPGA_FME_PORT_RELEASE:
			FPGA_DBG("got FPGA_FME_PORT_RELEASE");
			struct fpga_fme_port_release *port_release =
				va_arg(argp, struct fpga_fme_port_release *);
			if (!port_release) {
				FPGA_MSG("port_release is NULL");
				goto out_EINVAL;
			}
			if (port_release->argsz != sizeof(*port_release)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			if (port_release->flags != 0) {
				FPGA_MSG("unexpected flags %u", port_release->flags);
				goto out_EINVAL;
			}
			if (port_release->port_id != 0) {
				FPGA_MSG("unexpected port ID %u", port_release->port_id);
				goto out_EINVAL;
			}
			retval = 0;
			errno = 0;
			break;
		case FPGA_FME_PORT_PR:
			FPGA_DBG("got FPGA_FME_PORT_PR");
			struct fpga_fme_port_pr *pr = va_arg(argp, struct fpga_fme_port_pr *);
			if (!pr) {
				FPGA_MSG("pr is NULL");
				goto out_EINVAL;
			}
			if (pr->argsz != sizeof(*pr)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			if (pr->flags != 0) {
				FPGA_MSG("unexpected flags %u", pr->flags);
				goto out_EINVAL;
			}
			if (pr->port_id != 0) {
				FPGA_MSG("unexpected port ID %u", pr->port_id);
				goto out_EINVAL;
			}
			if (pr->buffer_size == 0) {
				FPGA_MSG("buffer size is 0");
				goto out_EINVAL;
			}
			if (!pr->buffer_address) {
				FPGA_MSG("buffer address is NULL");
				goto out_EINVAL;
			}
			pr->status = 0; /* return success */
			/* TODO: reflect reconfiguration (chagne afu_id?) */
			/* generate hash for bitstream data */
            hash = stupid_hash((uint32_t*)pr->buffer_address, pr->buffer_size / 4);
            /* write hash to file in tmp */
            strncpy_s(hashfilename, MAX_STRLEN, mock_devs[fd].pathname, strlen(mock_devs[fd].pathname) + 1);
            strncat_s(hashfilename, MAX_STRLEN, HASH_SUFFIX, sizeof(HASH_SUFFIX));

            FILE* hashfile = fopen(hashfilename, "w");
            if (hashfile) {
                fwrite(&hash, sizeof(hash), 1, hashfile);
                fclose(hashfile);
            }
            retval = 0;
            errno = 0;
            break;
        case FPGA_FME_PORT_ASSIGN:
			FPGA_DBG("got FPGA_FME_PORT_ASSIGN");
			struct fpga_fme_port_assign *port_assign =
				va_arg(argp, struct fpga_fme_port_assign *);
			if (!port_assign) {
				FPGA_MSG("port_assign is NULL");
				goto out_EINVAL;
			}
			if (port_assign->argsz != sizeof(*port_assign)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			if (port_assign->flags != 0) {
				FPGA_MSG("unexpected flags %u", port_assign->flags);
				goto out_EINVAL;
			}
			if (port_assign->port_id != 0) {
				FPGA_MSG("unexpected port ID %u", port_assign->port_id);
				goto out_EINVAL;
			}
			retval = 0;
			errno = 0;
			break;
		default:
			FPGA_DBG("Unknown FME IOCTL request %lu", request);
			break;
		}
		break;
	case FPGA_ACCELERATOR: /* PORT */
		switch (request) {
		case FPGA_PORT_DMA_MAP:
			FPGA_DBG("got FPGA_PORT_DMA_MAP");
			struct fpga_port_dma_map *dma_map = va_arg(argp, struct fpga_port_dma_map *);
			if (!dma_map) {
				FPGA_MSG("dma_map is NULL");
				goto out_EINVAL;
			}
			if (dma_map->argsz != sizeof(*dma_map)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			if (!dma_map->user_addr) {
				FPGA_MSG("mapping address is NULL");
				goto out_EINVAL;
			}
			/* TODO: check alignment */
			if (dma_map->length == 0) {
				FPGA_MSG("mapping size is 0");
				goto out_EINVAL;
			}
			dma_map->iova = FPGA_MOCK_IOVA; /* return something */
			retval = 0;
			errno = 0;
			break;
		case FPGA_PORT_DMA_UNMAP:
			FPGA_DBG("got FPGA_PORT_DMA_UNMAP");
			struct fpga_port_dma_unmap *dma_unmap = va_arg(argp, struct fpga_port_dma_unmap *);
			if (!dma_unmap) {
				FPGA_MSG("dma_unmap is NULL");
				goto out_EINVAL;
			}
			if (dma_unmap->argsz != sizeof(*dma_unmap)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			if (dma_unmap->iova != FPGA_MOCK_IOVA) {
				FPGA_MSG("unexpected IOVA (0x%llx)", dma_unmap->iova);
				goto out_EINVAL;
			}
			retval = 0;
			errno = 0;
			break;
		case FPGA_PORT_RESET:
			FPGA_DBG("got FPGA_PORT_RESET");
			retval = 0;
			break;
		case FPGA_PORT_GET_REGION_INFO:
			FPGA_DBG("got FPGA_PORT_GET_REGION_INFO");
			struct fpga_port_region_info *rinfo = va_arg(argp, struct fpga_port_region_info *);
			if (!rinfo) {
				FPGA_MSG("rinfo is NULL");
				goto out_EINVAL;
			}
			if (rinfo->argsz != sizeof(*rinfo)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			if (rinfo->index != 0) {
				FPGA_MSG("unsupported MMIO index");
				goto out_EINVAL;
			}
			if (rinfo->padding != 0) {
				FPGA_MSG("unsupported padding");
				goto out_EINVAL;
			}
			rinfo->flags = FPGA_REGION_READ | FPGA_REGION_WRITE | FPGA_REGION_MMAP;
			rinfo->size = 0x40000;
			rinfo->offset = 0;
			retval = 0;
			errno = 0;
			break;
		case FPGA_PORT_GET_INFO:
			FPGA_DBG("got FPGA_PORT_GET_INFO");
			struct fpga_port_info *pinfo = va_arg(argp, struct fpga_port_info *);
			if (!pinfo) {
				FPGA_MSG("pinfo is NULL");
				goto out_EINVAL;
			}
			if (pinfo->argsz != sizeof(*pinfo)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			pinfo->flags = 0;
			pinfo->num_regions = 1;
			pinfo->num_umsgs = 8;
			retval = 0;
			errno = 0;
			break;
		case FPGA_PORT_UMSG_SET_MODE:
			FPGA_DBG("got FPGA_PORT_UMSG_SET_MODE");
			struct fpga_port_umsg_cfg *ucfg = va_arg(argp, struct fpga_port_umsg_cfg *);
			if (!ucfg) {
				FPGA_MSG("ucfg is NULL");
				goto out_EINVAL;
			}
			if (ucfg->argsz != sizeof(*ucfg)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			if (ucfg->flags != 0) {
				FPGA_MSG("unexpected flags %u", ucfg->flags);
				goto out_EINVAL;
			}
			/* TODO: check hint_bitmap */
			if (ucfg->hint_bitmap >> FPGA_MOCK_NUM_UMSGS) {
				FPGA_MSG("invalid hint_bitmap 0x%x", ucfg->hint_bitmap);
				goto out_EINVAL;
			}
			retval = 0;
			errno = 0;
			break;
		case FPGA_PORT_UMSG_SET_BASE_ADDR:
			FPGA_DBG("got FPGA_PORT_UMSG_SET_BASE_ADDR");
			struct fpga_port_umsg_base_addr *ubase = va_arg(argp, struct fpga_port_umsg_base_addr *);
			if (!ubase) {
				FPGA_MSG("ubase is NULL");
				goto out_EINVAL;
			}
			if (ubase->argsz != sizeof(*ubase)) {
				FPGA_MSG("wrong structure size");
				goto out_EINVAL;
			}
			if (ubase->flags != 0) {
				FPGA_MSG("unexpected flags %u", ubase->flags);
				goto out_EINVAL;
			}
			/* TODO: check iova */
			retval = 0;
			errno = 0;
			break;
		case FPGA_PORT_UMSG_ENABLE:
			FPGA_DBG("got FPGA_PORT_UMSG_ENABLE");
			retval = 0;
			break;
		case FPGA_PORT_UMSG_DISABLE:
			FPGA_DBG("got FPGA_PORT_UMSG_DISABLE");
			retval = 0;
			break;
		default:
			FPGA_DBG("Unknown PORT IOCTL request %lu", request);
			break;
		}
		break;
	}

out:
	va_end(argp);
	return retval;

out_EINVAL:
	retval = -1;
	errno = EINVAL;
	goto out;
}

int open(const char* pathname, int flags, ...) {
    int fd;
    char path[MAX_STRLEN];
    char* err;
    int prefix_len = strlen(FPGA_DEV_PATH);
    va_list argp;

    dlerror(); /* clear errors */
    open_func real_open = (open_func)dlsym(RTLD_NEXT, "open");
    assert(real_open);
    err = dlerror();
    if (err) {
        FPGA_ERR("dlsym() failed: %s", err);
        errno = EINVAL;
        return -1;
    }

    FPGA_DBG("open(\"%s\", %i)", pathname, flags);

    if (strncmp(FPGA_DEV_PATH "/" FPGA_FME_DEV_PREFIX, pathname, prefix_len + strlen(FPGA_FME_DEV_PREFIX) - 2) == 0 ) {
        FPGA_DBG("accessing FME device");
        /* rewrite path */
        strncpy_s(path, sizeof(path), FPGA_MOCK_DEV_PATH, prefix_len);
        strncpy_s(path + prefix_len, sizeof(path) - prefix_len,
                  pathname + prefix_len, (MAX_STRLEN - 1 - prefix_len));
        /* call real open */
        FPGA_DBG("-> open(\"%s\", %i)", path, flags);
        fd = real_open(path, flags);
        /* store info */
        strncpy_s(mock_devs[fd].pathname, strlen(mock_devs[fd].pathname), path, MAX_STRLEN);
        mock_devs[fd].objtype = FPGA_DEVICE;
        mock_devs[fd].valid = 1;

    } else if (strncmp(FPGA_DEV_PATH "/" FPGA_PORT_DEV_PREFIX, pathname, prefix_len + 1 + strlen(FPGA_PORT_DEV_PREFIX)) == 0 ) {
        FPGA_DBG("accessing PORT device");
        /* rewrite path */
        strncpy_s(path, sizeof(path), FPGA_MOCK_DEV_PATH, prefix_len);
        strncpy_s(path + prefix_len, sizeof(path) - prefix_len, pathname + prefix_len, MAX_STRLEN - prefix_len);
        /* call real open */
        FPGA_DBG("-> open(\"%s\", %i)", path, flags);
        fd = real_open(path, flags);
        if (fd < 0)
            return fd;
        /* store info */
        strncpy_s(mock_devs[fd].pathname, sizeof(mock_devs[fd].pathname), path, MAX_STRLEN - 1);
        mock_devs[fd].objtype = FPGA_ACCELERATOR;
        mock_devs[fd].valid = 1;

    } else if (strncmp(SYSFS_FPGA_CLASS_PATH, pathname, strlen(SYSFS_FPGA_CLASS_PATH)) == 0 ) {

        /* rewrite path */
        rewrite_sysfs_path(pathname, path, MAX_STRLEN);
        /* call real open */
        FPGA_DBG("-> open(\"%s\", %i)", path, flags);
        fd = real_open(path, flags);

    } else {
        FPGA_DBG("-> open(\"%s\", %i)", pathname, flags);
        if (flags & O_CREAT){
            va_start(argp, flags);
            mode_t arg = va_arg(argp, mode_t);
            fd = ((open_mode_func)real_open)(pathname, flags, arg);
            va_end(argp);
        }else{
            fd = real_open(pathname, flags);
        }
    }

    return fd;
}

int close(int fd)
{
	int retval;
	char *err;

	dlerror(); /* clear errors */
	close_func real_close = (close_func)dlsym(RTLD_NEXT, "close");
	assert(real_close);
	err = dlerror();
	if (err) {
		FPGA_ERR("dlsym() failed: %s", err);
		errno = EINVAL;
		return -1;
	}

	FPGA_DBG("close(%i)", fd);
	retval = real_close(fd);
	if (retval >= 0 && fd < MAX_FD && mock_devs[fd].valid) {
		/* drop mock device */
		mock_devs[fd].valid = 0;
	}
	return retval;
}

DIR *opendir(const char *name)
{
	char *err;
	char s[MAX_STRLEN];

	dlerror(); /* clear errors */
	opendir_func real_opendir = (opendir_func)dlsym(RTLD_NEXT, "opendir");
	assert(real_opendir);
	err = dlerror();
	if (err) {
		FPGA_ERR("dlsym() failed: %s", err);
		errno = EINVAL;
		return NULL;
	}

	FPGA_DBG("opendir(%s)", name);
	rewrite_sysfs_path(name, s, MAX_STRLEN);
	FPGA_DBG("-> opendir(%s)", s);
	return real_opendir(s);
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz)
{
	char *err;
	char s[MAX_STRLEN];

	dlerror(); /* clear errors */
	readlink_func real_readlink = (readlink_func)dlsym(RTLD_NEXT, "readlink");
	assert(real_readlink);
	err = dlerror();
	if (err) {
		FPGA_ERR("dlsym() failed: %s", err);
		errno = EINVAL;
		return -1;
	}

	FPGA_DBG("readlink(%s)", pathname);
	rewrite_sysfs_path(pathname, s, MAX_STRLEN);
	FPGA_DBG("-> readlink(%s)", s);
	return real_readlink(s, buf, bufsiz);
}

/* stat() redirects to __xstat() */
int __xstat(int ver, const char *pathname, struct stat *buf)
{
	char *err;
	char s[MAX_STRLEN];

	dlerror(); /* clear errors */
	__xstat_func real_xstat = (__xstat_func)dlsym(RTLD_NEXT, "__xstat");
	assert(real_xstat);
	err = dlerror();
	if (err) {
		FPGA_ERR("dlsym() failed: %s", err);
		errno = EINVAL;
		return -1;
	}

	FPGA_DBG("stat(%s)", pathname);
	rewrite_sysfs_path(pathname, s, MAX_STRLEN);
	FPGA_DBG("-> stat(%s)", s);
	return real_xstat(ver, s, buf);
}

fpga_result fpgaReconfigureSlot(fpga_handle fpga,
				uint32_t slot,
				const uint8_t *bitstream,
				size_t bitstream_len,
				int flags)
{
	if (!fpga ||
		(((struct _fpga_handle *)fpga)->magic != FPGA_HANDLE_MAGIC) ||
		(((struct _fpga_handle *)fpga)->fddev < 0)) {
		FPGA_MSG("Invalid handle object");
		return FPGA_INVALID_PARAM;
	}

	if (slot > 2) {
		FPGA_MSG("Invalid slot: %d", slot);
		return FPGA_INVALID_PARAM;
	}

	if (!bitstream) {
		FPGA_MSG("NULL bitstream pointer");
		return FPGA_INVALID_PARAM;
	}

	if (!bitstream_len) {
		FPGA_MSG("bitstream length is 0");
		return FPGA_INVALID_PARAM;
	}

    uint32_t hash = stupid_hash((uint32_t*)bitstream, bitstream_len / 4);

    char* hashfilename  = "/tmp/intel-fpga-fme.0.gbshash";
    FILE* hashfile = fopen(hashfilename, "w");
    if (hashfile) {
        fwrite(&hash, sizeof(hash), 1, hashfile);
        fclose(hashfile);
    }

    return FPGA_OK;
}
