#ifndef _FIGO_H_
#define _FIGO_H_

#include "memmap.h"
#include "ra_gbl.h"
#include "mem_ctrl.h"
#include "figo_drm.h"
#include "drm.h"

#define	DRM_REG_BASE					(0x00 + MEMMAP_TSI_REG_BASE)
#define	DRM_DMX_SEC_STAT				(RA_DRMDMX_SECSAT + DRM_REG_BASE)
#define	DRM_SECURITY_STATUS_REG			(RA_SECSTATUS_CFG + DRM_DMX_SEC_STAT)
#define	DRM_DMX_CMD_STAT				(RA_DRMDMX_DTCM + DRM_REG_BASE)
#define	DRM_ROM_CMD_STAT_REG			(RA_DRMROM_CMD_STAT + DRM_DMX_CMD_STAT)
#define	DRM_ROM_CMD_CFG_REG				(RA_DRMROM_CMD_CMD_CFG + DRM_DMX_CMD_STAT)
#define DRM_ROM_CMD_CRC_REG				(RA_DRMROM_CMD_CMD_DAT0 + DRM_DMX_CMD_STAT)
#define	DRM_ROM_CMD_IMG_SIZE_REG		(RA_DRMROM_CMD_CMD_DAT1 + DRM_DMX_CMD_STAT)
#define	DRM_ROM_CMD_IMG_SRC_ADDR_REG	(RA_DRMROM_CMD_CMD_DAT2 + DRM_DMX_CMD_STAT)
#define	DRM_ROM_CMD_IMG_DST_ADDR_REG	(RA_DRMROM_CMD_CMD_DAT3 + DRM_DMX_CMD_STAT)
#define	DRM_ROM_CMD_RESPONSE_CFG_REG	(RA_DRMROM_CMD_RSP_CFG + DRM_DMX_CMD_STAT)
#define	DRM_ROM_CMD_RESPONSE_CRC_REG	(RA_DRMROM_CMD_RSP_DAT0 + DRM_DMX_CMD_STAT)
#define	DRM_ROM_CMD_RESPONSE_ERR_REG	(RA_DRMROM_CMD_RSP_DAT1 + DRM_DMX_CMD_STAT)

#define	DRM_ROM_CMD_STAT_EN				(0x01 << LSb32DRMROM_CMD_STAT_en)
#define	DRM_ROM_CMD_RESPONSE_SUCCEED	(0x00 << LSb32DRMROM_CMD_RSP_DAT1_error)

#endif
