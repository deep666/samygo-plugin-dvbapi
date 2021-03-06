/**
 * Copyright (c) 2016 harlequin
 * https://github.com/harlequin/samygo-plugin-dvbapi
 *
 * This file is part of samygo-plugin-dvbapi.
 *
 * samygo-plugin-dvbapi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h> /*ntohs*/
#include "../dvbapi.h"
#include "../utlist.h"
#include "../hook.h"
#include "../models.h"
#include "../log.h"
#include "../version.h"

#define FILTER_MASK_SIZE 16

#define DMX_HANDLE_19800000 0x19800000
#define DMX_HANDLE_LIVE 	0x19800620
#define DMX_HANDLE_PVR  	0x19800621
#define DMX_HANDLE_UNKNOWN 	0x19800622
#define DMX_HANDLE_PIP  	0x19800623

static pmt_t *g_pmt = NULL;
static u32 g_send_PMT_required = 0;
static demux_filter_t *g_demux_filter = NULL;
static int g_SID = 0;
static int g_fltDscmb = 0;
static unsigned int g_dmxHandle = DMX_HANDLE_LIVE;
static SdTSData_Settings_t g_dmxParams;
static s32 g_dmxMonHandle = -1;
static s32 g_dmxTableId = -1;

static SdTSData_Settings_t g_emmParams;

typedef union {
	const void *procs[18];
	struct	{
		const int (*SdTSData_StartMonitor)(u32 dmx_handle, SdTSData_Settings_t *a1);
		const int (*SdTSData_StopMonitor)(u32 dmx_handle, u32 mon_handle);
		const int (*MDrv_DSCMB_Init)(void);
		void **g_pAppWindow;
		const int (*TCCIMManagerBase_HostChannelChangeCompleted)(u32 a1, u32 a2, u32 a3, u32 a4, u32 a5);
		const int (*GetSource) (void *window, u32 *source, int arg3);
		const int (*GetTvChannel)(void *window, void *channel, int arg3);
		const int (*SetChannelQuiet)(void *window, void *channel, int arg3);
		const void* (*TCChannel_Create)(void *channel);
		const void* (*TCChannel_Destroy)(void *channel);
		const int (*ProgramNumber)(void *this);
		const int (*FlagScrambled)(void *this);
		const int (*MDrv_DSCMB_FltDisconnectPid)(unsigned int u32DscmbId, unsigned int u32Pid);
		const int (*MDrv_DSCMB_FltFree)(unsigned int u32DscmbId);
		const int (*MDrv_DSCMB_FltConnectPid)(unsigned int u32DscmbId, unsigned int u32Pid);
		const int (*MDrv_DSCMB_FltTypeSet)(unsigned int u32DscmbId, unsigned int eType);
		const int (*MDrv_DSCMB_FltAlloc)(void);
		const int (*MDrv_DSCMB_FltKeySet)(u32 u32DscmbId, DSCMB_Key_Type eKeyType, u8 *pu8Key);
	};

} api_callbacks_t;

api_callbacks_t api_callbacks = {{
		(const void*)"_Z21SdTSData_StartMonitorjP19SdTSData_Settings_tj",
		(const void*)"_Z20SdTSData_StopMonitorjj",
		(const void*)"MDrv_DSCMB_Init",
		(const void*)"g_pAppWindow",
		(const void*)"_ZN16TCCIMManagerBase26HostChannelChangeCompletedEP9TCChanneliN8TCWindow7ESourceE",
		(const void*)"_ZN8TCWindow9GetSourceEPii",
		(const void*)"_ZN8TCWindow12GetTVChannelEP9TCChanneli",
		(const void*)"_ZN8TCWindow15SetChannelQuietEPK9TCChannelb",
		(const void*)"_ZN9TCChannelC2Ev",
		(const void*)"_ZN9TCChannelD2Ev",
		(const void*)"_ZNK9TCChannel13ProgramNumberEv",
		(const void*)"_ZNK9TCChannel13FlagScrambledEv",
		(const void*)"MDrv_DSCMB_FltDisconnectPid",
		(const void*)"MDrv_DSCMB_FltFree",
		(const void*)"MDrv_DSCMB_FltConnectPid",
		(const void*)"MDrv_DSCMB_FltTypeSet",
		(const void*)"MDrv_DSCMB_FltAlloc",
		(const void*)"MDrv_DSCMB_FltKeySet",
}};

static void stopMonitors() {
	u32 ret;

	if (g_dmxMonHandle >= 0) {
		ret = api_callbacks.SdTSData_StopMonitor(g_dmxHandle, g_dmxMonHandle);
		log("ECM%02X monitor stopped, dmxHandle=0x%08x, monHandle=0x%08x, ret=0x%08x\n",g_dmxTableId, g_dmxHandle, g_dmxMonHandle, ret);
		g_dmxMonHandle = -1;
		g_dmxTableId = -1;
	}
}


static void changeMonitor(unsigned int dmxHandle) {
	if ( g_dmxTableId == -1) {
		return;
	}

	stopMonitors();

	g_dmxHandle = dmxHandle;
	log("using dmxHandle=0x%08X\n", dmxHandle);

	g_dmxMonHandle = api_callbacks.SdTSData_StartMonitor(DMX_HANDLE_LIVE, &g_dmxParams);
	log("ECM%02x monitor restarted, dmxHandle=0x%08x, monHandle=0x%08x\n",g_dmxTableId, g_dmxHandle, g_dmxMonHandle);
}




_HOOK_IMPL(int,SdAVDec_DemuxStop, unsigned int dmxHandle, int eDemuxOut) {
	log("SdAVDec_DemuxStop, dmxHandle=0x%08X, eDemuxOut=0x%08X\n", dmxHandle, eDemuxOut);
	log("g_dmxHandle=0x%08X\n", g_dmxHandle);

	switch (g_dmxHandle) {
	case DMX_HANDLE_LIVE:
		api_callbacks.SdTSData_StopMonitor(DMX_HANDLE_LIVE, 0x00);
		break;
	case DMX_HANDLE_PIP:
	case DMX_HANDLE_PVR:
		log("CHANGE MONITOR NORMALY ...\n");
		changeMonitor(DMX_HANDLE_LIVE);
		break;
	default:
		log("Unknown dmx handle value: 0x%08X\n", dmxHandle);
		break;
	}

	_HOOK_DISPATCH(SdAVDec_DemuxStop, dmxHandle, eDemuxOut);
	return (int)h_ret;
}

_HOOK_IMPL(int,SdAVDec_DemuxStart, unsigned int dmxHandle, int eDemuxOut) {
	log("SdDemux_Start, dmxHandle=0x%08X, eDemuxOut=0x%08X\n", dmxHandle, eDemuxOut);
	log("g_dmxHandle=0x%08X\n", g_dmxHandle);
	_HOOK_DISPATCH(SdAVDec_DemuxStart, dmxHandle, eDemuxOut);

	if ( g_dmxHandle != DMX_HANDLE_PVR) {
		changeMonitor(dmxHandle);
	}

	return (int)h_ret;
}

_HOOK_IMPL(int,DemuxBase_m_Demux_SICallback, SICallBackSettings_t* data) {
	//log("DemuxBase_m_Demux_SICallback, ra: %p, hmon=0x%08X, pid=0x%08X, buf=0x%08X, len=0x%08X\n", ra, SICallBackSettings_t[0], SICallBackSettings_t[1],SICallBackSettings_t[2],SICallBackSettings_t[3]);
	//void *ra;
	//asm("move %0, $ra\n" : "=r" (ra));

	_HOOK_DISPATCH(DemuxBase_m_Demux_SICallback, data);

	//pmt_t *buf = NULL;
	//pmt_t *tmp = NULL;
	pmt_t *p = NULL;
	pmt_t *pmt = NULL;
	u16 sid = 0x00;

	if ( data->len > 0 && data->len <= 1024) {

		if ( be8(data->ptr) == 0x02 ) {

			sid = be16( (data->ptr) + 0x03 );

			if ( sid == 0x00 ) {
				return (int)h_ret;
			}

			LL_SEARCH_SCALAR(g_pmt, pmt, sid, sid);
			if(pmt) {
				if ( pmt->ptr == NULL /*|| memcmp( data->ptr, pmt->ptr, data->len ) != 0 */) {
					//pmt->sid = sid;
					pmt->len = data->len;
					pmt->ptr = malloc(pmt->len);
					memcpy(pmt->ptr, data->ptr, pmt->len);
					//LL_APPEND(g_pmt, pmt);
					//send complete pmt list to oscam
					u8 lm = PMT_LIST_FIRST;
					LL_FOREACH(g_pmt, p) {
						if ( p->next == NULL ) { lm |= PMT_LIST_LAST; }
						socket_send_capmt(p->ptr, lm);
						lm = PMT_LIST_MORE;
					}
				}
			}

		} else {
			demux_filter_t *filter;
			LL_SEARCH_SCALAR(g_demux_filter, filter, monHandle, data->hmon);
			if ( filter ) {
				log(">> EMM%02x ... hmon:0x%08x send data\n", be8(data->ptr), data->hmon);
				socket_send_filter_data( filter->demuxId, filter->filterId, data->ptr , data->len );
			}
		}
	}

	return (int)h_ret;
}

_HOOK_IMPL(int, TCCIMManagerBase_HostChannelChangeCompleted, u32 this, u32 TCChannel, u32 a3, u32 ESource, u32 a5) {
	log("TCCIMManagerBase_HostChannelChangeCompleted, this: 0x%08x tcchannel: 0x%08x a3: 0x%08x esource: 0x%08x a5: 0x%08x\n",this,TCChannel,a3,ESource,a5);
	_HOOK_DISPATCH(TCCIMManagerBase_HostChannelChangeCompleted, this, TCChannel, a3, ESource, a5);

	if ( TCChannel != 0x00 ) {
		int sid = api_callbacks.ProgramNumber( (void *) TCChannel);
		pmt_t *p = NULL;
		LL_SEARCH_SCALAR(g_pmt, p, sid, sid);
		if(!p) {
			p = malloc(sizeof(pmt_t));
			p->sid = sid;
			p->ptr = NULL;
			LL_APPEND(g_pmt, p);
			g_send_PMT_required = 1;
			log("Service id changes, new SID: 0x%04x\n", g_SID);
		}
	}

	return (int)h_ret;
}

_HOOK_IMPL(int,TCCIMManagerBase_SetAVPID,u32 a1, u32 pid_video, u32 pid_audio, u32 eWindow, u32 a4, u32 a5) {
	log("TCCIMManagerBase_SetAVPID, video: 0x%08x, audio: 0x%08x, window: 0x%08x, a4: 0x%08x, a5: 0x%08x\n",pid_video, pid_audio, eWindow, a4, a5);
	_HOOK_DISPATCH(TCCIMManagerBase_SetAVPID,a1, pid_video, pid_audio, eWindow, a4, a5);

	u32 res;
	if (g_fltDscmb == 1) {
		api_callbacks.MDrv_DSCMB_FltDisconnectPid(0, pid_video);
		api_callbacks.MDrv_DSCMB_FltDisconnectPid(0, pid_audio);
		api_callbacks.MDrv_DSCMB_FltFree(0);
		g_fltDscmb = 0;
	}
	api_callbacks.MDrv_DSCMB_FltAlloc();
	res = api_callbacks.MDrv_DSCMB_FltConnectPid( 0, pid_video);
	log("MDrv_DSCMB_FltConnectPid=%d\n", res);
	res = api_callbacks.MDrv_DSCMB_FltConnectPid( 0, pid_audio);
	log("MDrv_DSCMB_FltConnectPid=%d\n", res);
	res = api_callbacks.MDrv_DSCMB_FltTypeSet(0, 0);
	log("MDrv_DSCMB_FltTypeSet=%d\n", res);

	return (int)h_ret;
}

STATIC dyn_fn_t TCCIMManagerBase_func_table[] = {
		{ 0, "_ZN13TDsPrimeDemux15t_DemuxCallbackEPN8CDiDemux20SICallBackSettings_tE" },
		{ 0, "_ZN16TCCIMManagerBase26HostChannelChangeCompletedEP9TCChanneliN8TCWindow7ESourceE" },
		{ 0, "_Z18SdAVDec_DemuxStartj18SdAVDec_DemuxOut_k" },
		{ 0, "_Z17SdAVDec_DemuxStopj18SdAVDec_DemuxOut_k" },
		{ 0, "_ZN16TCCIMManagerBase8SetAVPIDEiii"},
};

STATIC hook_entry_t TCCIMManagerBase_hooks[] =
{
#define _HOOK_ENTRY(F, I) \
		&hook_##F, &TCCIMManagerBase_func_table[I], &x_##F
		{ _HOOK_ENTRY(DemuxBase_m_Demux_SICallback, __COUNTER__) },
		{ _HOOK_ENTRY(TCCIMManagerBase_HostChannelChangeCompleted, __COUNTER__) },
		{ _HOOK_ENTRY(SdAVDec_DemuxStart, __COUNTER__) },
		{ _HOOK_ENTRY(SdAVDec_DemuxStop, __COUNTER__) },
		{ _HOOK_ENTRY(TCCIMManagerBase_SetAVPID, __COUNTER__) },
#undef _HOOK_ENTRY
};


//////////////////////////////////////////////////////////////////////////////

int dvbapi_install(void *h) {
	int res = 0;

	log("Install "LIB_TV_MODELS" lib"LIB_NAME" "LIB_VERSION" - "BUILD_GIT_TIME" (c) harlequin 2016\n");

	samyGO_whacky_t_init(h, &api_callbacks, ARRAYSIZE(api_callbacks.procs));

	if ( dyn_sym_tab_init(h, TCCIMManagerBase_func_table, ARRAYSIZE(TCCIMManagerBase_func_table)) >= 0 ) {
		set_hooks(TCCIMManagerBase_hooks, ARRAYSIZE(TCCIMManagerBase_hooks));
		res = 1;
	}

	u32 init = api_callbacks.MDrv_DSCMB_Init();
	log("MDrc_DSCMB_Init=0x%04x\n", init);

	return res;
}

int dvbapi_server_info(void) {
	//g_SID = -1;

	u8 channel[0x20];
	u32 source = 0; /*TODO Check if source is live tv  0x00 */
	api_callbacks.GetSource(*api_callbacks.g_pAppWindow, &source, 0x01);
	api_callbacks.TCChannel_Create(channel);
	api_callbacks.GetTvChannel(*api_callbacks.g_pAppWindow, channel, 0x01);
	int scrambled = api_callbacks.FlagScrambled(channel);
	if ( scrambled != 0) {
		api_callbacks.SetChannelQuiet(*api_callbacks.g_pAppWindow, channel, 0x01);
	}

	return 0;
}

int dvbapi_set_descriptor(ca_descr_t ca_descr) {
	g_fltDscmb = api_callbacks.MDrv_DSCMB_FltKeySet(0 /*ca_descr.index*/ , ca_descr.parity + 1 , ca_descr.cw);
	log("MDrv_DSCMB_FltKeySet=%d g_fltDscmb=%d\n",  g_fltDscmb, g_fltDscmb);
	return g_fltDscmb;
}

void dvbapi_dmx_stop(u8 demux_index, u8 filter_num, u16 pid) {
	demux_filter_t *filter;
	log("DVBAPI_DMX_STOP request, pid=%X, demux_idx=%d, filter_num=%d\n", pid, demux_index, filter_num);
	LL_SEARCH_SCALAR(g_demux_filter, filter, filterId, filter_num);
	if(filter) {
		u32 res = api_callbacks.SdTSData_StopMonitor( g_dmxHandle, filter->monHandle & 0x7FFFFFFF );
		log("Monitor stopped, idx=0x%02X, flt=0x%02X, dmxHandle=0x%08X, monHandle=0x%08X, ret=0x%08X\n", demux_index, filter_num, g_dmxHandle, filter->monHandle & 0x7FFFFFFF, res);
	}
}

int dvbapi_start_filter(u8 demux_index, u8 filter_num, struct dmx_sct_filter_params params){
	/* This pid zero still occurs because the pmt is not send correctly */
	if ( ntohs(params.pid) != 0x00 ) {
		demux_filter_t *filter;

		LL_SEARCH_SCALAR(g_demux_filter, filter, filterId, filter_num );
		if(!filter) {
			filter = malloc(sizeof(demux_filter_t));
			filter->tableId = -1;
			filter->filterId = filter_num;
			filter->demuxId = demux_index;
			filter->monHandle = -1;
			LL_APPEND(g_demux_filter, filter);
		}

		if (filter->monHandle != -1) {
			u32 ret = api_callbacks.SdTSData_StopMonitor(g_dmxHandle, filter->monHandle);
			log("EMM%02X monitor stopped, dmxHandle=0x%08x, monHandle=0x%08x, ret=0x%08x\n",filter->tableId, g_dmxHandle, filter->monHandle, ret);
			filter->monHandle = -1;
			filter->tableId = -1;
		}

		g_emmParams.pid = ntohs(params.pid);
		g_emmParams.res = 1;
		g_emmParams.filter_len = DMX_FILTER_SIZE;
		g_emmParams.filter[0] = params.filter.filter[0] ;//buff[8];
		g_emmParams.mask[0] = params.filter.mask[0];//buff[24];
		memcpy(&g_emmParams.filter[3], &params.filter.filter[1], DMX_FILTER_SIZE - 3);
		memcpy(&g_emmParams.mask[3], &params.filter.mask[1], DMX_FILTER_SIZE - 3);

		//memcpy(g_emmParams.filter, params.filter.filter, FILTER_MASK_SIZE);
		//memcpy(g_emmParams.mask, params.filter.mask, FILTER_MASK_SIZE);

		filter->tableId = params.filter.filter[0];
		filter->demuxId = demux_index;
		filter->filterId = filter_num;
		filter->monHandle = api_callbacks.SdTSData_StartMonitor(DMX_HANDLE_LIVE, &g_emmParams);
		log("EMM%02x monitor started, dmxHandle=0x%08x, monHandle=0x%08x\n",filter->tableId, g_dmxHandle, filter->monHandle);
	}
	return 0;
}
