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


void common_entity_set_dev(int entity_num, struct common_drm_device *dev)
{
	if (common_entity_key == -1)
		common_entity_key = xf86AllocateEntityPrivateIndex();

	xf86GetEntityPrivate(entity_num, common_entity_key)->ptr = dev;
}
