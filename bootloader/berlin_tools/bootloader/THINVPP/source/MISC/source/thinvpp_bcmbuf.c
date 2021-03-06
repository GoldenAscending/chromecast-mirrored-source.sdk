#define _THINVPP_BCMBUF_C
/*******************************************************************************
*                Copyright 2007, MARVELL SEMICONDUCTOR, LTD.                   *
* THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL.                      *
* NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT  *
* OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE        *
* DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.     *
* THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,       *
* IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.   *
*                                                                              *
* MARVELL COMPRISES MARVELL TECHNOLOGY GROUP LTD. (MTGL) AND ITS SUBSIDIARIES, *
* MARVELL INTERNATIONAL LTD. (MIL), MARVELL TECHNOLOGY, INC. (MTI), MARVELL    *
* SEMICONDUCTOR, INC. (MSI), MARVELL ASIA PTE LTD. (MAPL), MARVELL JAPAN K.K.  *
* (MJKK), MARVELL ISRAEL LTD. (MSIL).                                          *
*******************************************************************************/

#include "thinvpp_module.h"
#include "thinvpp_common.h"
#include "memmap.h"
#include "avio.h"

#ifdef __LINUX_KERNEL__
#include <asm/cacheflush.h>
#include <asm/io.h>
#define FLUSH_DCACHE_RANGE(start, size) __cpuc_flush_dcache_area(start, size)
#else
#define FLUSH_DCACHE_RANGE(start, size) flush_dcache_range(start, (unsigned int *)(((int)(start))+(size)))
#endif

/***************************************************************
 * FUNCTION: allocate register programming buffer
 * PARAMS: *buf - pointer to a register programming buffer
 *       : size - size of the buffer to allocate
 *       :      - (should be a multiple of 4)
 * RETURN:  1 - succeed
 *          0 - failed to initialize a BCM buffer
 ****************************************************************/
int THINVPP_BCMBUF_Create(BCMBUF *pbcmbuf, int size)
{
    if (size <= 0)
        return (MV_THINVPP_EBADPARAM);

    /* allocate memory for the buffer */
    pbcmbuf->addr = (int)THINVPP_MALLOC(size);
    if(!pbcmbuf->addr)
        return MV_THINVPP_ENOMEM;

    pbcmbuf->size = size;
    pbcmbuf->head = (unsigned int *)((pbcmbuf->addr+0x1f)&(~0x01f));
    if (pbcmbuf->head != (unsigned int *)pbcmbuf->addr)
        pbcmbuf->size = size-64;

    return MV_THINVPP_OK;
}


/***************************************************************
 * FUNCTION: free register programming buffer
 * PARAMS: *buf - pointer to a register programming buffer
 * RETURN:  1 - succeed
 *          0 - failed to initialize a BCM buffer
 ****************************************************************/
int THINVPP_BCMBUF_Destroy(BCMBUF *pbcmbuf)
{
    /* allocate memory for the buffer */
    if (!pbcmbuf->addr)
        return (MV_THINVPP_EBADCALL);

    THINVPP_FREE((int *)(pbcmbuf->addr));

    pbcmbuf->addr = 0;
    pbcmbuf->head = NULL;

    return MV_THINVPP_OK;
}


/***************************************************************
 * FUNCTION: reset a register programming buffer
 * PARAMS: *buf - pointer to a register programming buffer
 * RETURN:  1 - succeed
 *          0 - failed to initialize a BCM buffer
 ****************************************************************/
int THINVPP_BCMBUF_Reset(BCMBUF *pbcmbuf)
{
    pbcmbuf->tail = pbcmbuf->head + (pbcmbuf->size/4);

    /*set pointers to the head*/
    pbcmbuf->writer = pbcmbuf->head;
    pbcmbuf->dv1_head = pbcmbuf->head;
    pbcmbuf->dv3_head = pbcmbuf->dv1_head + (pbcmbuf->size/16)*3;
    pbcmbuf->subID = -1; /* total */

    return MV_THINVPP_OK;
}

/*********************************************************
 * FUNCTION: Select sub register programming buffer
 * PARAMS: *buf - pointer to the buffer descriptor
 *         subID - CPCB_1, CPCB_2, CPCB_3 or total
 ********************************************************/
void THINVPP_BCMBUF_Select(BCMBUF *pbcmbuf, int subID)
{
    /* reset read/write pointer of the buffer */
    if (subID == CPCB_1){
        pbcmbuf->writer = pbcmbuf->dv1_head;
    } else if (subID == CPCB_3) {
        pbcmbuf->writer = pbcmbuf->dv3_head;
    } else {
        pbcmbuf->writer = pbcmbuf->head;
    }

    pbcmbuf->subID = subID;

    return;
}

/*********************************************************
 * FUNCTION: write register address (4 bytes) and value (4 bytes) to the buffer
 * PARAMS: *buf - pointer to the buffer descriptor
 *               address - address of the register to be set
 *               value - the value to be written into the register
 * RETURN: 1 - succeed
 *               0 - register programming buffer is full
 ********************************************************/
int THINVPP_BCMBUF_Write(BCMBUF *pbcmbuf, unsigned int address, unsigned int value)
{
    unsigned int *end;

    /*if not enough space for storing another 8 bytes, wrap around happens*/
    if (pbcmbuf->subID == CPCB_1)
        end = pbcmbuf->dv3_head;
    else
        end = pbcmbuf->tail;

    if(pbcmbuf->writer == end){
        /*the buffer is full, no space for wrap around*/
        return MV_THINVPP_EBCMBUFFULL;
    }

    /*save the data to the buffer*/
   *pbcmbuf->writer = value;
    pbcmbuf->writer ++;
   *pbcmbuf->writer = address;
    pbcmbuf->writer ++;

    return MV_THINVPP_OK;
}

/*********************************************************************
 * FUNCTION: do the hardware transaction
 * PARAMS: *buf - pointer to the buffer descriptor
 ********************************************************************/
void THINVPP_BCMBUF_HardwareTrans(BCMBUF *pbcmbuf, int block)
{
    HDL_semaphore *pSemHandle;
    HDL_dhub2d *pDhubHandle;
    unsigned int bcm_sched_cmd[2];
    int dhubID;
    unsigned int *start;
    int status;
    int size;

    if (pbcmbuf->subID == CPCB_1)
        start = pbcmbuf->dv1_head;
    else if (pbcmbuf->subID == CPCB_3)
        start = pbcmbuf->dv3_head;
    else
        start = pbcmbuf->head;

    size = (int)pbcmbuf->writer-(int)start;

    if (size <= 0)
        return;

    /* flush data in D$ */
    FLUSH_DCACHE_RANGE(start, size);
#if __LINUX_KERNEL__
    start = (unsigned int *)virt_to_phys(start);
#endif

    /* start BCM engine */
    dhubID = avioDhubChMap_vpp_BCM_R;
    pDhubHandle = &VPP_dhubHandle;

    /* clear BCM interrupt */
    pSemHandle = dhub_semaphore(&(pDhubHandle->dhub));
    status = semaphore_chk_full(pSemHandle, dhubID);
    while (status) {
        semaphore_pop(pSemHandle, dhubID, 1);
        semaphore_clr_full(pSemHandle, dhubID);
        status = semaphore_chk_full(pSemHandle, dhubID);
    }

    dhub_channel_generate_cmd(&(pDhubHandle->dhub), dhubID, (int)start, (int)size, 0, 0, 0, 1, bcm_sched_cmd);
    while( !BCM_SCHED_PushCmd(BCM_SCHED_Q12, bcm_sched_cmd, NULL));

    if (block){
        /* check BCM interrupt */
        pSemHandle = dhub_semaphore(&(pDhubHandle->dhub));
        status = semaphore_chk_full(pSemHandle, dhubID);
        while (!status) {
            status = semaphore_chk_full(pSemHandle, dhubID);
        }

        /* clear BCM interrupt */
        semaphore_pop(pSemHandle, dhubID, 1);
        semaphore_clr_full(pSemHandle, dhubID);
    }

    return;
}

#if BOOTLOADER_SHOWLOGO

int THINVPP_CFGQ_Create(DHUB_CFGQ *cfgQ, int size)
{
    if (size <= 0)
        return (MV_THINVPP_EBADPARAM);

    /* allocate memory for the buffer */
    cfgQ->base_addr = (int)THINVPP_MALLOC(size);
    if(!cfgQ->base_addr)
        return MV_THINVPP_ENOMEM;
    cfgQ->addr = (int *)((cfgQ->base_addr+0x1f)&(~0x01f));
    cfgQ->len = 0;

    return MV_THINVPP_OK;
}

int THINVPP_CFGQ_Destroy(DHUB_CFGQ *cfgQ)
{
    if (!cfgQ->base_addr )
        return (MV_THINVPP_EBADCALL);

    THINVPP_FREE((int *)(cfgQ->base_addr));

    cfgQ->base_addr = 0;
    cfgQ->addr = 0;

    return MV_THINVPP_OK;
}

/*******************************************************************************
 * FUNCTION: commit cfgQ which contains BCM DHUB programming info to interrupt service routine
 * PARAMS: *cfgQ - cfgQ
 *         cpcbID - cpcb ID which this cmdQ belongs to
 *         intrType - interrupt type which this cmdQ belongs to: 0 - VBI, 1 - VDE
 * NOTE: this API is only called from VBI/VDE ISR.
 *******************************************************************************/
int THINVPP_BCMDHUB_CFGQ_Commit(DHUB_CFGQ *cfgQ, int cpcbID)
{
    unsigned int sched_qid;
    unsigned int bcm_sched_cmd[2];

    if (cfgQ->len <= 0)
        return MV_THINVPP_EBADPARAM;

    if (cpcbID == CPCB_1)
        sched_qid = BCM_SCHED_Q0;
    else if (cpcbID == CPCB_2)
        sched_qid = BCM_SCHED_Q1;
    else
        sched_qid = BCM_SCHED_Q2;

    FLUSH_DCACHE_RANGE(cfgQ->addr, cfgQ->len*8);
#ifdef __LINUX_KERNEL__
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)virt_to_phys(cfgQ->addr), (int)cfgQ->len*8, 0, 0, 0, 1, bcm_sched_cmd);
#else
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)cfgQ->addr, (int)cfgQ->len*8, 0, 0, 0, 1, bcm_sched_cmd);
#endif
    while( !BCM_SCHED_PushCmd(sched_qid, bcm_sched_cmd, NULL));

    return MV_THINVPP_OK;
}

/*********************************************************************
 * FUNCTION: send a BCM BUF info to a BCM cfgQ
 * PARAMS: *pbcmbuf - pointer to the BCMBUF
 *         *cfgQ - target BCM cfgQ
 * NOTE: this API is only called from VBI/VDE ISR.
 ********************************************************************/
int THINVPP_BCMBUF_To_CFGQ(BCMBUF *pbcmbuf, DHUB_CFGQ *cfgQ)
{
    unsigned int *start;
    int size;
    unsigned int bcm_sched_cmd[2];

    if (pbcmbuf->subID == CPCB_1)
        start = pbcmbuf->dv1_head;
    else if (pbcmbuf->subID == CPCB_3)
        start = pbcmbuf->dv3_head;
    else
        start = pbcmbuf->head;

    size = (int)pbcmbuf->writer-(int)start;

    if (size <= 0)
        return MV_THINVPP_EBADPARAM;

    FLUSH_DCACHE_RANGE(start, size);
#ifdef __LINUX_KERNEL__
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)virt_to_phys(start), size, 0, 0, 0, 1, bcm_sched_cmd);
#else
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)start, size, 0, 0, 0, 1, bcm_sched_cmd);
#endif
    while( !BCM_SCHED_PushCmd(BCM_SCHED_Q13, bcm_sched_cmd, cfgQ->addr + cfgQ->len*2));
    cfgQ->len += 2;

    return MV_THINVPP_OK;
}

/*********************************************************************
 * FUNCTION: send a BCM cfgQ info to a BCM cfgQ
 * PARAMS: src_cfgQ - pointer to the source BCM cfgQ
 *         *cfgQ - target BCM cfgQ
 * NOTE: this API is only called from VBI/VDE ISR.
 ********************************************************************/
void THINVPP_CFGQ_To_CFGQ(DHUB_CFGQ *src_cfgQ, DHUB_CFGQ *cfgQ)
{
    unsigned int bcm_sched_cmd[2];

    if (src_cfgQ->len <= 0)
        return;

    FLUSH_DCACHE_RANGE(src_cfgQ->addr, src_cfgQ->len*8);
#ifdef __LINUX_KERNEL__
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)virt_to_phys(src_cfgQ->addr), (int)src_cfgQ->len*8, 0, 0, 0, 1, bcm_sched_cmd);
#else
    dhub_channel_generate_cmd(&(VPP_dhubHandle.dhub), avioDhubChMap_vpp_BCM_R, (int)src_cfgQ->addr, (int)src_cfgQ->len*8, 0, 0, 0, 1, bcm_sched_cmd);
#endif
    while( !BCM_SCHED_PushCmd(BCM_SCHED_Q13, bcm_sched_cmd, cfgQ->addr + cfgQ->len*2));
    cfgQ->len += 2;
}

#endif
