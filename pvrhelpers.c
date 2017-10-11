/*
 * Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <services.h>
#include <sgxapi_km.h>

#include "helpers.h"
#include "pvrhelpers.h"

bool InitialiseServices(struct pvr_data *pvr_data)
{
	DBG();

	PVRSRV_ERROR err;
	unsigned i;

	err = PVRSRVConnect(&pvr_data->services, 0);
	FAIL_IF(err, "PVRSRVConnect failed: %s", PVRSRVGetErrorString(err));

	PVRSRV_DEVICE_IDENTIFIER dev_ids[PVRSRV_MAX_DEVICES];
	IMG_UINT32 num_devices;

	err = PVRSRVEnumerateDevices(pvr_data->services, &num_devices, dev_ids);
	FAIL_IF(err, "PVRSRVEnumerateDevices failed: %s", PVRSRVGetErrorString(err));

	for (i = 0; i < num_devices; i++)
		if (dev_ids[i].eDeviceClass == PVRSRV_DEVICE_CLASS_3D)
			break;
	FAIL_IF(i == num_devices, "Couldn't find a 3D device");

	err = PVRSRVAcquireDeviceData(pvr_data->services,
				      dev_ids[i].ui32DeviceIndex,
				      &pvr_data->dev_data,
				      PVRSRV_DEVICE_TYPE_UNKNOWN);
	FAIL_IF(err, "PVRSRVAcquireDeviceData failed: %s", PVRSRVGetErrorString(err));

	PVRSRV_HEAP_INFO heap_info[PVRSRV_MAX_CLIENT_HEAPS];
	IMG_UINT32 heap_count;

	err = PVRSRVCreateDeviceMemContext(&pvr_data->dev_data,
					   &pvr_data->h_dev_mem_context,
					   &heap_count,
					   heap_info);
	FAIL_IF(err, "PVRSRVCreateDeviceMemContext failed: %s", PVRSRVGetErrorString(err));

	for (i = 0; i < heap_count; i++) {
		if (HEAP_IDX(heap_info[i].ui32HeapID) == SGX_GENERAL_HEAP_ID) {
			pvr_data->h_mapping_heap = heap_info[i].hDevMemHeap;
			break;
		}
	}

	FAIL_IF(i == heap_count, "Couldn't find heap %u", SGX_GENERAL_HEAP_ID);

	pvr_data->misc_info.ui32StateRequest = PVRSRV_MISC_INFO_GLOBALEVENTOBJECT_PRESENT;
	err = PVRSRVGetMiscInfo(pvr_data->services, &pvr_data->misc_info);
	FAIL_IF(err, "Request for global event object failed: %s", PVRSRVGetErrorString(err));

	DBG("PVR Services Initialised");

	return true;
}

void DeInitialiseServices(struct pvr_data *pvr_data)
{
	PVRSRVReleaseMiscInfo(pvr_data->services, &pvr_data->misc_info);
	PVRSRVDestroyDeviceMemContext(&pvr_data->dev_data, pvr_data->h_dev_mem_context);
	PVRSRVDisconnect(pvr_data->services);
	DBG("PVR Services DeInitialised");
}

static inline bool unsigned_greater_equal(unsigned a, unsigned b)
{
	return (a - b) < INT_MAX;
}

/*
 * Wait for GPU ops to complete
 */
void WaitForOpsComplete(const struct pvr_data *pvr_data, const PVRSRV_CLIENT_SYNC_INFO *sync_info)
{
	PVRSRV_SYNC_DATA *sync = sync_info->psSyncData;

	if (!sync)
		return;

	const IMG_UINT32 wops_pending = sync->ui32WriteOpsPending;
	const IMG_UINT32 rops_pending = sync->ui32ReadOpsPending;
	const IMG_UINT32 rops2_pending = sync->ui32ReadOps2Pending;

	DBG("wop=%u, rop=%u, rop2=%u", wops_pending, rops_pending, rops2_pending);

	int loops = 0;

	for (;;)
	{
		IMG_UINT32 wops_complete = sync->ui32WriteOpsComplete;
		IMG_UINT32 rops_complete = sync->ui32ReadOpsComplete;
		IMG_UINT32 rops2_complete = sync->ui32ReadOps2Complete;

		if (unsigned_greater_equal(wops_complete, wops_pending) &&
		    unsigned_greater_equal(rops_complete, rops_pending) &&
		    unsigned_greater_equal(rops2_complete, rops2_pending))
		{
			break;
		}

		loops++;
		PVRSRVEventObjectWait(pvr_data->services, pvr_data->misc_info.hOSGlobalEvent);
	}

	DBG("SGX ops completed in %d loops", loops);
}
