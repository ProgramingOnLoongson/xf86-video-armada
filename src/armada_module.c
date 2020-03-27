/*
 * Marvell Armada DRM-based driver
 *
 * Written by Russell King, 2012, derived in part from the
 * Intel xorg X server driver.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "xf86.h"
#ifdef XSERVER_PLATFORM_BUS
#include "xf86platformBus.h"
#endif

#include "armada_drm.h"
#include "armada_accel.h"
#include "common_drm.h"
#include "utils.h"

#define ARMADA_VERSION		4000
#define ARMADA_NAME		"armada"
#define ARMADA_DRIVER_NAME	"armada"

#define DRM_MODULE_NAMES	"armada-drm", "imx-drm", "loongson-drm"
#define DRM_DEFAULT_BUS_ID	NULL

static const char *drm_module_names[] = { DRM_MODULE_NAMES };

/* Supported "chipsets" */
static SymTabRec armada_chipsets[] = {
//	{  0, "88AP16x" },
	{  0, "88AP510" },
	{ -1, NULL }
};

static SymTabRec ipu_chipsets[] = {
	{  0, "i.MX6" },
	{ -1, NULL }
};

static SymTabRec loongson7a_chipsets[] = {
	{  0, "7A1000" },
	{ -1, NULL }
};

static const OptionInfoRec * const options[] = {
	armada_drm_options,
	common_drm_options,
};

static const char *armada_drm_accelerators[] = {
#ifdef HAVE_ACCEL_ETNADRM
	"etnadrm_gpu",
#endif
#ifdef HAVE_ACCEL_ETNAVIV
	"etnaviv_gpu",
#endif
#ifdef HAVE_ACCEL_GALCORE
	"vivante_gpu",
#endif
	NULL,
};

struct armada_accel_module {
	const char *name;
	const struct armada_accel_ops *ops;
	pointer module;
};

static struct armada_accel_module *armada_accel_modules;
static unsigned int armada_num_accel_modules;

Bool armada_load_accelerator(ScrnInfoPtr pScrn, const char *module)
{
	unsigned int i;

	if (NULL == module)
	{
		for (i = 0; armada_drm_accelerators[i]; ++i)
			if (xf86LoadSubModule(pScrn, armada_drm_accelerators[i]))
				break;
	}
	else
	{
		if (!xf86LoadSubModule(pScrn, module))
			return FALSE;

		if (armada_num_accel_modules == 0)
			return FALSE;
	}

	return TRUE;
}

const struct armada_accel_ops *armada_get_accelerator(void)
{
	return armada_accel_modules ? armada_accel_modules[0].ops : NULL;
}

_X_EXPORT
void armada_register_accel(const struct armada_accel_ops *ops, pointer module,
	const char *name)
{
	unsigned int n = armada_num_accel_modules++;

	armada_accel_modules = xnfrealloc(armada_accel_modules,
			armada_num_accel_modules * sizeof(*armada_accel_modules));

	armada_accel_modules[n].name = name;
	armada_accel_modules[n].ops = ops;
	armada_accel_modules[n].module = module;
}


static void armada_identify(int flags)
{
	xf86PrintChipsets(ARMADA_NAME, "Support for Marvell LCD Controller",
			  armada_chipsets);
	xf86PrintChipsets(ARMADA_NAME, "Support for Freescale IPU",
			  ipu_chipsets);
	xf86PrintChipsets(ARMADA_NAME, "Support for Loongson 7a1000 display controller",
			  loongson7a_chipsets);
}

static void LS_SetupScrnHooks(ScrnInfoPtr pScrn)
{
	pScrn->driverVersion = ARMADA_VERSION;
	pScrn->driverName    = ARMADA_DRIVER_NAME;
	pScrn->name          = ARMADA_NAME;
	pScrn->Probe         = NULL;

	armada_drm_init_screen(pScrn);
}

static Bool armada_probe(DriverPtr drv, int flags)
{
	GDevPtr *devSections;
	int i, numDevSections;
	Bool foundScreen = FALSE;

	xf86Msg(X_INFO, "Try probe:\n");

	if (flags & PROBE_DETECT)
		return FALSE;

	numDevSections = xf86MatchDevice(ARMADA_DRIVER_NAME, &devSections);
	if (numDevSections <= 0)
	{
		return FALSE;
	}
	else
	{
		xf86Msg(X_INFO, "Number of DevSections: %d\n", numDevSections);
	}

	for (i = 0; i < numDevSections; ++i)
	{
		ScrnInfoPtr pScrn;
		const char *busid = DRM_DEFAULT_BUS_ID;
		int entity, fd;
		unsigned int j;

		if (devSections[i]->busID)
			busid = devSections[i]->busID;

		for (j = 0; j < ARRAY_SIZE(drm_module_names); ++j)
		{
			fd = drmOpen(drm_module_names[j], busid);
			if (fd >= 0)
				break;
		}

		xf86Msg(X_INFO, "(%s, %d) opened.\n", drm_module_names[j], fd);

		if (fd < 0)
			continue;

		if (!common_drm_fd_is_master(fd))
			continue;

		entity = xf86ClaimNoSlot(drv, 0, devSections[i], TRUE);
		common_alloc_dev(entity, fd, NULL, TRUE);

		pScrn = xf86ConfigFbEntity(NULL, 0, entity, NULL, NULL, NULL, NULL);
		if (!pScrn)
			continue;

		if (busid)
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using BusID \"%s\"\n", busid);

		foundScreen = TRUE;
		LS_SetupScrnHooks(pScrn);
	}

	free(devSections);

	return foundScreen;
}

static const OptionInfoRec *armada_available_options(int chipid, int busid)
{
	static OptionInfoRec opts[32];
	unsigned int i, j, k;

	for (i = k = 0; i < ARRAY_SIZE(options); i++) {
		for (j = 0; options[i][j].token != -1; j++) {
			if (k >= ARRAY_SIZE(opts) - 1)
				return NULL;
			opts[k++] = options[i][j];
		}
	}
		
	opts[k].token = -1;
	return opts;
}

static Bool armada_driver_func(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
{
	xorgHWFlags *flag;
    
	switch (op)
	{
	case GET_REQUIRED_HW_INTERFACES:
		flag = (CARD32*)ptr;
		(*flag) = 0;
		return TRUE;
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,15,99,902,0)
	case SUPPORTS_SERVER_FDS:
		return TRUE;
#endif
	default:
		return FALSE;
	}
}

#ifdef XSERVER_PLATFORM_BUS
static Bool armada_is_kms(int fd)
{
	drmVersionPtr version;
	drmModeResPtr res;
	Bool has_connectors;

	version = drmGetVersion(fd);
	if (!version)
		return FALSE;
	drmFreeVersion(version);

	res = drmModeGetResources(fd);
	if (!res)
		return FALSE;

	has_connectors = res->count_connectors > 0;
	drmModeFreeResources(res);

	return has_connectors;
}


static int open_hw(const char *dev)
{
    int fd;

    if (dev)
        fd = open(dev, O_RDWR | O_CLOEXEC, 0);
    else {
        dev = getenv("KMSDEVICE");
        if ((NULL == dev) || ((fd = open(dev, O_RDWR | O_CLOEXEC, 0)) == -1)) {
            dev = "/dev/dri/card0";
            fd = open(dev, O_RDWR | O_CLOEXEC, 0);
        }
    }
    if (fd == -1)
        xf86DrvMsg(-1, X_ERROR, "open %s: %s\n", dev, strerror(errno));

    return fd;
}


static int check_outputs(int fd, int *count)
{
    drmModeResPtr res = drmModeGetResources(fd);
    int ret;

    if (!res)
        return FALSE;

    if (count)
        *count = res->count_connectors;

    ret = res->count_connectors > 0;

    if (ret == FALSE) {
        uint64_t value = 0;
        if (drmGetCap(fd, DRM_CAP_PRIME, &value) == 0 &&
                (value & DRM_PRIME_CAP_EXPORT))
            ret = TRUE;
    }
    drmModeFreeResources(res);
    return ret;
}

static Bool probe_hw(const char *dev, struct xf86_platform_device *platform_dev)
{
    int fd;

#ifdef XF86_PDEV_SERVER_FD
    if (platform_dev && (platform_dev->flags & XF86_PDEV_SERVER_FD))
    {
        fd = xf86_platform_device_odev_attributes(platform_dev)->fd;
        if (fd == -1)
            return FALSE;
        return check_outputs(fd, NULL);
    }
#endif


    fd = open_hw(dev);
    if (fd != -1) {
        int ret = check_outputs(fd, NULL);

        close(fd);
        return ret;
    }
    return FALSE;

}

static struct common_drm_device *armada_create_dev(int entity_num,
	struct xf86_platform_device *platform_dev)
{
	struct common_drm_device *drm_dev;
	const char *path;
	Bool ddx_managed_master;
	int fd, our_fd = -1;

	path = xf86_platform_device_odev_attributes(platform_dev)->path;
	if (NULL == path)
		goto err_free;
	else
	{
		xf86Msg( X_INFO, " path: %s\n", path);
	}

#ifdef ODEV_ATTRIB_FD
	fd = xf86_get_platform_device_int_attrib(platform_dev, ODEV_ATTRIB_FD, -1);
#else
	fd = -1;
#endif

#ifdef XF86_PDEV_SERVER_FD
	if (platform_dev && (platform_dev->flags & XF86_PDEV_SERVER_FD))
	{
		fd = xf86_platform_device_odev_attributes(platform_dev)->fd;
		if (fd != -1)
		{
			check_outputs(fd, NULL);
			xf86Msg( X_INFO, " SERVER MANAGED FD\n");
		}
	}
#endif

	if (fd != -1)
	{
		ddx_managed_master = FALSE;
		if (!armada_is_kms(fd))
			goto err_free;
	}
	else
	{
		ddx_managed_master = TRUE;
		our_fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
		xf86Msg( X_INFO, " Opening %s\n", path);
		if (our_fd == -1)
			goto err_free;

		if (!armada_is_kms(our_fd))
		{
			xf86Msg( X_INFO, " %s is not a KMS device, closing.\n", path);
			close(our_fd);
			goto err_free;
		}

		if (!common_drm_fd_is_master(our_fd))
		{
			xf86Msg( X_INFO, " %s is not a master, closing.\n", path);
			close(our_fd);
			goto err_free;
		}
		fd = our_fd;
	}

	/* If we're running unprivileged, don't drop master status */
	if (geteuid())
	{
		ddx_managed_master = FALSE;
		xf86Msg(X_INFO, "Running unprivileged, don't drop master status.\n");
	}
	drm_dev = common_alloc_dev(entity_num, fd, path, ddx_managed_master);
	if (!drm_dev && our_fd != -1)
		close(our_fd);

	return drm_dev;

 err_free:
	return NULL;
}


static Bool armada_platform_probe(DriverPtr driver, 
		int entity_num, int flags,
		struct xf86_platform_device *dev, 
		intptr_t match_data)
{
	struct common_drm_device *drm_dev;
	ScrnInfoPtr pScrn = NULL;
	int scr_flags = 0;

	if (flags & PLATFORM_PROBE_GPU_SCREEN)
		scr_flags = XF86_ALLOCATE_GPU_SCREEN;
	xf86Msg(X_INFO, "Try platform probe: entity_num=%d\n", entity_num);

	drm_dev = common_entity_get_dev(entity_num);
	if (NULL == drm_dev)
	{
		xf86Msg(X_INFO, "drm_dev = NULL, try create.\n");
		drm_dev = armada_create_dev(entity_num, dev);
	}

	if (!drm_dev)
		return FALSE;

	pScrn = xf86AllocateScreen(driver, scr_flags);
	if (!pScrn)
		return FALSE;
	if (xf86IsEntitySharable(entity_num))
		xf86SetEntityShared(entity_num);

	xf86AddEntityToScreen(pScrn, entity_num);

	LS_SetupScrnHooks(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"Added screen for KMS device %s\n", drm_dev->kms_path);

	return TRUE;
}
#endif


#ifdef XSERVER_LIBPCIACCESS
static const struct pci_id_match armada_device_match[] = {
    {
     PCI_MATCH_ANY, PCI_MATCH_ANY, PCI_MATCH_ANY, PCI_MATCH_ANY,
     0x00030000, 0x00ff0000, 0},

    {0, 0, 0},
};
#endif


_X_EXPORT DriverRec armada_driver = {
	.driverVersion = ARMADA_VERSION,
	.driverName = ARMADA_DRIVER_NAME,
	.Identify = armada_identify,
	.Probe = armada_probe,
	.AvailableOptions = armada_available_options,
	.module = NULL,
	.refCount = 0,
	.driverFunc = armada_driver_func,
#ifdef XSERVER_LIBPCIACCESS
	.supported_devices = armada_device_match,
#endif
#ifdef XSERVER_PLATFORM_BUS
	.platformProbe = armada_platform_probe,
#endif
};

#ifdef XFree86LOADER

static pointer armada_setup(pointer module, pointer opts, int *errmaj,
	int *errmin)
{
	static Bool setupDone = FALSE;

	if (setupDone) {
		if (errmaj)
			*errmaj = LDR_ONCEONLY;
		return NULL;
	}

	setupDone = TRUE;

	xf86AddDriver(&armada_driver, module, HaveDriverFuncs);

	return (pointer) 1;
}

static XF86ModuleVersionInfo armada_version = {
	.modname = "armada",
	.vendor = MODULEVENDORSTRING,
	._modinfo1_ = MODINFOSTRING1,
	._modinfo2_ = MODINFOSTRING2,
	.xf86version = XORG_VERSION_CURRENT,
	.majorversion = PACKAGE_VERSION_MAJOR,
	.minorversion = PACKAGE_VERSION_MINOR,
	.patchlevel = PACKAGE_VERSION_PATCHLEVEL,
	.abiclass = ABI_CLASS_VIDEODRV,
	.abiversion = ABI_VIDEODRV_VERSION,
	.moduleclass = MOD_CLASS_VIDEODRV,
	.checksum = { 0, 0, 0, 0 },
};

_X_EXPORT XF86ModuleData armadaModuleData = {
	.vers = &armada_version,
	.setup = armada_setup,
};

#endif /* XFree86LOADER */
