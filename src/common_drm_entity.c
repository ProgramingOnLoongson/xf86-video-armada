#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_DIX_CONFIG_H
#include "dix-config.h"
#endif

#include <xf86drm.h>
#include <xf86.h>

#include "common_drm.h"

static int common_entity_key = -1;

struct common_drm_device *common_entity_get_dev(int entity_num)
{
	if (common_entity_key == -1)
		common_entity_key = xf86AllocateEntityPrivateIndex();
	if (common_entity_key == -1)
		return NULL;

	return xf86GetEntityPrivate(entity_num, common_entity_key)->ptr;
}

/*
struct common_drm_device * common_entity_get_priv(ScrnInfoPtr pScrn)
{
	DevUnion *pPriv;

	struct common_drm_info * pDrm = GET_DRM_INFO(pScrn);

	pPriv = xf86GetEntityPrivate(pDrm->dev->index, common_entity_key);
	return pPriv->ptr;
}
*/

void common_entity_set_dev(int entity_num, struct common_drm_device *dev)
{
	DevUnion *pPriv;
        // suijingfeng: why set it sharable ?
	xf86SetEntitySharable(entity_num);

	if (common_entity_key == -1)
	{
		common_entity_key = xf86AllocateEntityPrivateIndex();
	}

	pPriv = xf86GetEntityPrivate(entity_num, common_entity_key);

//	xf86SetEntityInstanceForScreen(scrn, entity_num,
//		xf86GetNumEntityInstances(entity_num) - 1);
/*
	if (NULL == pPriv->ptr) {
		pPriv->ptr = xnfcalloc(sizeof(struct common_drm_device), 1);
	}

	xf86Msg(X_INFO,
                   " Setup entity: common_entity_key=%d\n", common_entity_key);
*/
	pPriv->ptr = dev;
}
