/*
 * JX_AT_CMD.c
 *
 */
#if 1//def __MTK_TARGET__
#if defined(__JX_AT_CMD_SUPPORT__)

#include "MMIDataType.h"
#include "MMI_include.h"

#include "nvram_editor_data_item.h"
#include "TimerEvents.h"
#include "NwInfoSrv.h"
#include "gpiosrvgprot.h"
#include "JX_AT_CMD.h"
#include "ImeiSrvGprot.h"
#include "dcl_uart.h"
//#include "med_aud_hal_struct.h"
#include "resource_audio.h"
#include "mmi_rp_app_jx_at_cmd_def.h"
#include "SimAccessSrvGprot.h"

#include "CharBatSrvGprot.h"
#include "PowerOnChargerProt.h"
#include "ProtocolEvents.h"

#if defined(__WIFI_SCAN_ONLY__)
#include "wlansrvscanonly.h"
#endif /* __WIFI_SCAN_ONLY__ */

#if defined(__GPS_SUPPORT__)
#include"mdi_gps.h"
#endif /* __GPS_SUPPORT__ */

#if defined(__JX_LED_SUPPORT__)
BOOL bLEDTesting = FALSE;
#endif /* __JX_LED_SUPPORT__ */

BOOL g_in_at_testing = KAL_FALSE;

#if defined(__JX_U_BLOX_GPS_SUPPORT__)
// FOR TEST GPS ANTENNA 配合测试GPS的工具来使用
//#define GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH
#endif /* __JX_U_BLOX_GPS_SUPPORT__ */

typedef enum
{
	AT_ACTION_VERNO,     // 获取版本号
	AT_ACTION_IMEI,      // 获取IMEI
	AT_ACTION_VIBR,      // 震动测试
	AT_ACTION_CALI,      // 获取校准信息
	AT_ACTION_SPK,       // 喇叭测试
	
	AT_ACTION_LIGS,      // 光感测试
	AT_ACTION_GSENS,     // GSENSOR测试
	AT_ACTION_SIM,       // 获取SIM卡信息
	AT_ACTION_WIFI,      // WIFI测试
	AT_ACTION_MIC,       // MIC测试
	
	AT_ACTION_GPS,       // GPS测试
	AT_ACTION_KEYP,      // 按键测试
	AT_ACTION_HEART,     // 心率测试
	AT_ACTION_LED,       // LED灯测试
	AT_ACTION_ADC,       // 充电测试
	
	AT_ACTION_LOGMOD,    //切换为LOG模式
	AT_ACTION_ATMOD,     //切换为AT模式
	AT_ACTION_RESET,     //重启终端
	AT_ACTION_POWOFF,    //关闭终端
	
	AT_ACTION_ALL
} AT_ACTION_ENUM;

typedef struct {
	AT_ACTION_ENUM action;
	char *atCmdStr;
	char *atRetHead;
	char *atRetTailOk;
	char *atRetTailError;
	custom_rsp_type_enum (*commandFunc)(custom_cmdLine *pCmdBuf);
	MMI_TIMER_IDS timerId;
	MMI_TIMER_IDS timerCloseId;
} custom_atcmd;

#define JX_AT_SIM_IMEI_LEN                 (15)    /* IMEI */
#define JX_AT_SIM_IMSI_LEN                 (16)    /* IMSI */
#define JX_AT_SIM_ICCID_LEN                (20)    /* ICCID */

kal_uint8 gGpsUartHandle = 0;

typedef struct
{
    S8 imei[JX_AT_SIM_IMEI_LEN+1];
	S8 imsi[JX_AT_SIM_IMSI_LEN+1];
    S8 iccid[JX_AT_SIM_ICCID_LEN+1];
} jx_at_phone_info_struct;

static jx_at_phone_info_struct jx_at_phone_info;

const custom_atcmd jx_at_cmd_table[] = { 

{ AT_ACTION_VERNO,	 "AT+VERNO",	"[VERNO]:",	 "[VERNO=OK][END]",	 "[VERNO=ERROR][END]",	jx_at_verno,	JX_AT_CMD_TIMER_VERNO,		0 	 }, 
{ AT_ACTION_IMEI,	 "AT+IMEI",	 	"[IMEI]:",	 "[IMEI=OK][END]",	 "[IMEI=ERROR][END]",	jx_at_imei,		JX_AT_CMD_TIMER_IMEI,		0 	 },
{ AT_ACTION_VIBR,	 "AT+VIBR",		"[VIBR]:",	 "[VIBR=OK][END]",	 "[VIBR=ERROR][END]",	jx_at_vibr, 	JX_AT_CMD_TIMER_VIBR,		0	 },
{ AT_ACTION_CALI,	 "AT+CALI",		"[CALI]:",	 "[CALI=OK][END]",	 "[CALI=ERROR][END]",	jx_at_cali,		JX_AT_CMD_TIMER_CALI,  		0	 },
{ AT_ACTION_SPK,	 "AT+SPK",		"[SPK]:",	 "[SPK=OK][END]",	 "[SPK=ERROR][END]",	jx_at_spk,		JX_AT_CMD_TIMER_SPK,		0  	 },

{ AT_ACTION_LIGS,	 "AT+LIGS",		"[LIGS]:",	 "[LIGS=OK][END]", 	 "[LIGS=ERROR][END]",	jx_at_ligs,		JX_AT_CMD_TIMER_LIGS,		0 	 },
{ AT_ACTION_GSENS,   "AT+GSENS",	"[GSENS]:",  "[GSENS=OK][END]",  "[GSENS=ERROR][END]",  jx_at_gsens, 	JX_AT_CMD_TIMER_GSENS,	0	 },
{ AT_ACTION_SIM,	 "AT+SIM",		"[SIM]:",	 "[SIM=OK][END]",	 "[SIM=ERROR][END]", 	jx_at_sim, 	    JX_AT_CMD_TIMER_SIM,		0	 },
{ AT_ACTION_WIFI,	 "AT+WIFI",		"[WIFI]:",	 "[WIFI=OK][END]",	 "[WIFI=ERROR][END]",	jx_at_wifi,		JX_AT_CMD_TIMER_WIFI,		0  	 },
{ AT_ACTION_MIC,	 "AT+MIC",		"[MIC]:",	 "[MIC=OK][END]",	 "[MIC=ERROR][END]", 	jx_at_mic,		JX_AT_CMD_TIMER_MIC,		0	 },

{ AT_ACTION_GPS,	 "AT+GPS",		"[GPS]:",	 "[GPS=OK][END]",	 "[GPS=ERROR][END]",	jx_at_gps,		JX_AT_CMD_TIMER_GPS_READ, JX_AT_CMD_TIMER_GPS_OFF},
{ AT_ACTION_KEYP,	 "AT+KEYP",		"[KEYP]:",	 "[KEYP=OK][END]", 	 "[KEYP=ERROR][END]",	jx_at_keyp, 	JX_AT_CMD_TIMER_KEYP,   	0    },
{ AT_ACTION_HEART,	 "AT+HEART", 	"[HEART]:",	 "[HEART=OK][END]",	 "[HEART=ERROR][END]",	jx_at_heart, 	JX_AT_CMD_TIMER_HEART_READ,	JX_AT_CMD_TIMER_HEART_OFF},
{ AT_ACTION_LED,	 "AT+LED",		"[LED]:",	 "[LED=OK][END]",	 "[LED=ERROR][END]",	jx_at_led, 	    JX_AT_CMD_TIMER_LED,		0	 },
{ AT_ACTION_ADC,	 "AT+ADC",		"[ADC]:",	 "[ADC=OK][END]",	 "[ADC=ERROR][END]", 	jx_at_adc,		JX_AT_CMD_TIMER_ADC,		0	 },

{ AT_ACTION_LOGMOD,	 "AT+LOGMOD",	"[LOGMOD]:", "[LOGMOD=OK][END]", "[LOGMOD=ERROR][END]",	jx_at_logmod,	JX_AT_CMD_TIMER_LOGMOD,		0	 },
{ AT_ACTION_ATMOD,	 "AT+ATMOD",	"[ATMOD]:",	 "[ATMOD=OK][END]",	 "[ATMOD=ERROR][END]", 	jx_at_atmod, 	JX_AT_CMD_TIMER_ATMOD,		0	 },
{ AT_ACTION_RESET,	 "AT+RESET", 	"[RESET]:",  "[RESET=OK][END]",	 "[RESET=ERROR][END]",	jx_at_reset,	JX_AT_CMD_TIMER_RESET,		0	 },
{ AT_ACTION_POWOFF,	 "AT+POWOFF",	"[POWOFF]:", "[POWOFF=OK][END]", "[POWOFF=ERROR][END]",	jx_at_powoff,	JX_AT_CMD_TIMER_POWOFF,		0	 },

};

typedef const char* PCSTR;

kal_bool jx_at_handler(char *full_cmd_string) {
	char buffer[MAX_UART_LEN + 1];
	char *cmd_name, *atCmdString;
	kal_uint8 index = 0;
	kal_uint16 length;
	kal_uint16 i;
	custom_cmdLine command_line;

	cmd_name = buffer;

	length = strlen(full_cmd_string);
	length = length > MAX_UART_LEN ? MAX_UART_LEN : length;
	while ((full_cmd_string[index] != '=') && //might be TEST command or EXE command
			(full_cmd_string[index] != '?') && // might be READ command
			(full_cmd_string[index] != 13) && //carriage return
			index < length) {
		cmd_name[index] = toupper(full_cmd_string[index]);
		index++;
	}
	cmd_name[index] = '\0';

	for (i = 0; jx_at_cmd_table[i].atCmdStr != NULL; i++) {
		char* atCmdString = jx_at_cmd_table[i].atCmdStr;
		if (strcmp(cmd_name, atCmdString) == 0) {
			strncpy(command_line.character, full_cmd_string,
			COMMAND_LINE_SIZE + NULL_TERMINATOR_LENGTH);
			command_line.character[COMMAND_LINE_SIZE] = '\0';
			command_line.length = strlen(command_line.character);
			command_line.position = index;
			if (jx_at_cmd_table[i].commandFunc(&command_line) == CUSTOM_RSP_OK) {
				sprintf(buffer, "OK");
				rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
			} else {
				sprintf(buffer, "ERROR");
				rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
			}
			return KAL_TRUE;
		}
	}
	return KAL_FALSE;
}

static kal_bool parseSkipSpace(PCSTR szSource, PCSTR*pszPointer) {
	*pszPointer = szSource;
	while ((**pszPointer) && ((**pszPointer) == ' ' || (**pszPointer) == '\t'))
		(*pszPointer)++;
	return KAL_TRUE;
}

static kal_bool parseUInt(PCSTR szSource, kal_bool fLeadingZerosAllowed, UINT *pnInt, PCSTR*pszPointer) {
	kal_bool ret = KAL_FALSE;

	do {
		int nLeadingDigit = -1;

		parseSkipSpace(szSource, pszPointer);

		*pnInt = 0;

		while (**pszPointer >= '0' && **pszPointer <= '9') {
			if (!fLeadingZerosAllowed) {
				// Test for leading 0s
				if (-1 == nLeadingDigit) {
					// Leading digit hasn't been set yet -- set it now
					nLeadingDigit = **pszPointer - '0';
				} else if (!nLeadingDigit) {
					// Leading digit is 0 and we got another digit
					// This means that we have leading 0s -- reset the pointer and punt
					ret = KAL_FALSE;
					break;
				}
			}

			*pnInt = *pnInt * 10 + (**pszPointer - '0');
			(*pszPointer)++;
			ret = KAL_TRUE;
		}

		parseSkipSpace(*pszPointer, pszPointer);
	} while (0);

	if (!ret) {
		*pszPointer = szSource;
	}
	return ret;
}

BOOL jx_at_cmd_testing(void)
{
	return g_in_at_testing;
}

void jx_at_verno_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_verno_handler");
	#endif

	sprintf(buffer, "%sSW=%s,TIME=%s,HW=%s%s", 
					jx_at_cmd_table[AT_ACTION_VERNO].atRetHead, 
					VERNO_STR, 
					BUILD_DATE_TIME_STR, 
					HW_VER_STR, 
					jx_at_cmd_table[AT_ACTION_VERNO].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);

	g_in_at_testing = KAL_TRUE;
	srv_backlight_turn_on(SRV_BACKLIGHT_PERMANENT);
	#if defined(__JX_LED_SUPPORT__) && (defined(__JX_K9_COMMON__) || defined(__JX_S12_COMMON__)) // K9 测试LED暂放这里
	JX_led_status_play_autotest();
	#endif /* defined(__JX_LED_SUPPORT__) && defined(__JX_K9_COMMON__) */
}

custom_rsp_type_enum jx_at_verno(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
		
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE:
		StartTimer(jx_at_cmd_table[AT_ACTION_VERNO].timerId, 0, jx_at_verno_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

void jx_at_imei_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_imei_handler imei=%s", jx_at_phone_info.imei);
#endif

	if (jx_at_phone_info.imei[0] != 0)
	{
		sprintf(buffer, "%sIMEI=%s%s", 
						jx_at_cmd_table[AT_ACTION_IMEI].atRetHead, 
						jx_at_phone_info.imei, 
						jx_at_cmd_table[AT_ACTION_IMEI].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		
		return;
	}
	
	if (srv_imei_get_imei(MMI_SIM1, jx_at_phone_info.imei, sizeof(jx_at_phone_info.imei)))
	{
		sprintf(buffer, "%sIMEI=%s%s", 
						jx_at_cmd_table[AT_ACTION_IMEI].atRetHead, 
						jx_at_phone_info.imei, 
						jx_at_cmd_table[AT_ACTION_IMEI].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	} 
	else
	{
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_IMEI].atRetHead, 
						"No IMEI!",
						jx_at_cmd_table[AT_ACTION_IMEI].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	}
}

custom_rsp_type_enum jx_at_imei(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_IMEI].timerId, 0, jx_at_imei_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

void jx_at_cali_handler()
{
	S32 Ret = 0;
	S8 gBarCode[64] = {0};
	S8 buffer[MAX_UART_LEN + 1] = {0};
	S16 ErrorCode;
	
	/* Get Barcode from Nvram */
	Ret = ReadRecord(NVRAM_EF_BARCODE_NUM_LID, 1, gBarCode, 64, &ErrorCode);

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_cali_handler Ret=%d,gBarCode=%s", Ret, gBarCode);
#endif

	if ((64 == Ret) && (('1' == gBarCode[60]) || ('P' == gBarCode[62])))
	{
		sprintf(buffer, "%s%s%s",
						jx_at_cmd_table[AT_ACTION_CALI].atRetHead, 
						gBarCode,
						jx_at_cmd_table[AT_ACTION_CALI].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	}
	else
	{
		sprintf(buffer, "%s%s%s",
						jx_at_cmd_table[AT_ACTION_CALI].atRetHead, 
						"No calibration!",
						jx_at_cmd_table[AT_ACTION_CALI].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	}
}

custom_rsp_type_enum jx_at_cali(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE:
		StartTimer(jx_at_cmd_table[AT_ACTION_CALI].timerId, 0, jx_at_cali_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

static void spk_handler_off(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

	mdi_audio_set_audio_mode(AUD_MODE_NORMAL);
	mdi_audio_stop_id(TONE_KEY_NORMAL);
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_setloud_spk flag==1");
#endif

	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_SPK].atRetHead, 
					"Speaker open success!", 
					jx_at_cmd_table[AT_ACTION_SPK].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
}

void jx_at_setloud_spk(U8 flag, U8 mode)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

	mdi_audio_set_audio_mode(mode);
    if (flag == 1)
    {
        /* play 1K tone */
        TONE_SetOutputVolume(255, 0);
        mdi_audio_play_id(TONE_KEY_NORMAL, DEVICE_AUDIO_PLAY_INFINITE);  // DEVICE_AUDIO_PLAY_ONCE DEVICE_AUDIO_PLAY_INFINITE
	StopTimer(jx_at_cmd_table[AT_ACTION_SPK].timerCloseId);
	StartTimer(jx_at_cmd_table[AT_ACTION_SPK].timerCloseId, 1000, spk_handler_off);
    }
    else
    {
        /* stop 1K tone */
        mdi_audio_stop_id(TONE_KEY_NORMAL);
		
#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_at_setloud_spk flag==0");
#endif	
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_SPK].atRetHead, 
						"Speaker close success!", 
						jx_at_cmd_table[AT_ACTION_SPK].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
    }
}

void jx_at_spk_handler_on()
{
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_spk_handler_on");
#endif
	// 关闭吹麦测试
	jx_at_mic_close_by_others();
 	jx_at_setloud_spk(1, AUD_MODE_LOUDSPK);
}

void jx_at_spk_handler_off()
{
 	jx_at_setloud_spk(0, AUD_MODE_NORMAL);
}

custom_rsp_type_enum jx_at_spk(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_SPK].timerId, 0, jx_at_spk_handler_on);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
	{
			PCSTR szParse = pCmdBuf->character + pCmdBuf->position;
			kal_uint32 uValue;
			if (!parseUInt(szParse, KAL_FALSE, &uValue, &szParse) || uValue > 2) 
			{
				S8 buffer[MAX_UART_LEN + 1] = {0};
		
				sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_SPK].atRetHead, 
						"Speaker test instruction error! Eg:AT+SPK=1(0)",
						jx_at_cmd_table[AT_ACTION_SPK].atRetTailOk);
				rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
			}
			if (uValue == 0) 
			{
				StartTimer(jx_at_cmd_table[AT_ACTION_SPK].timerId, 0, jx_at_spk_handler_off);
			} 
			else if (uValue == 1) 
			{
				StartTimer(jx_at_cmd_table[AT_ACTION_SPK].timerId, 0, jx_at_spk_handler_on);
			} 
			ret_value = CUSTOM_RSP_OK;
	}
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

void jx_at_mic_set_loopback(kal_uint32 on, void* arg )
{
#if defined(__TC01__) && defined(__ACOUSTIC_LOOPBACK_SUPPORT__) && defined(__MTK_TARGET__)

#else
    if (on)
    {
     	L1SP_SetAfeLoopback(KAL_TRUE);
		L1SP_Afe_On(L1SP_AFE_MODE_UL_OPEN | L1SP_AFE_MODE_DL_OPEN);
    }
    else
    {
    	L1SP_Afe_Off();
		L1SP_SetAfeLoopback(KAL_FALSE);
    }
#endif
}

void jx_at_mic_close_by_others(void)
{
	srv_profiles_stop_tone(SRV_PROF_TONE_FM);
	mdi_audio_set_audio_mode(AUD_MODE_NORMAL);

	kal_sleep_task(kal_milli_secs_to_ticks(100));
	aud_util_proc_in_med(MOD_MMI, jx_at_mic_set_loopback, MMI_FALSE, NULL);
}

void jx_at_mic_handler_on(void)
{
	char buffer[129];
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_mic_handler_on");
#endif
	//FM_SendStopAudioReq(0);
	//FM_SendSetAudioModeReq(AUD_MODE_NORMAL);
	srv_profiles_stop_tone(SRV_PROF_TONE_FM);
	mdi_audio_set_audio_mode(AUD_MODE_NORMAL);
			
	kal_sleep_task(kal_milli_secs_to_ticks(100));
	L1SP_SetOutputVolume(200, 0);
	aud_util_proc_in_med(MOD_MMI, jx_at_mic_set_loopback, MMI_TRUE, NULL);

	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_MIC].atRetHead, 
					"MIC loopback on!",
					jx_at_cmd_table[AT_ACTION_MIC].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
}

void jx_at_mic_handler_off(void)
{
	char buffer[129];

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_mic_handler_off");
#endif

	//FM_SendStopAudioReq(0);
	//FM_SendSetAudioModeReq(AUD_MODE_NORMAL);
	srv_profiles_stop_tone(SRV_PROF_TONE_FM);
	mdi_audio_set_audio_mode(AUD_MODE_NORMAL);
		
	kal_sleep_task(kal_milli_secs_to_ticks(100));
	aud_util_proc_in_med(MOD_MMI, jx_at_mic_set_loopback, MMI_FALSE, NULL);
	
	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_MIC].atRetHead, 
					"MIC loopback off!",
					jx_at_cmd_table[AT_ACTION_MIC].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
}

custom_rsp_type_enum jx_at_mic(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_MIC].timerId, 0, jx_at_mic_handler_on);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
	{
		PCSTR szParse = pCmdBuf->character + pCmdBuf->position;
		kal_uint32 uValue;
		if (!parseUInt(szParse, KAL_FALSE, &uValue, &szParse) || uValue > 2) 
		{
			S8 buffer[MAX_UART_LEN + 1] = {0};
	
			sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_MIC].atRetHead, 
					"MIC test instruction error! Eg:AT+MIC=1(0)",
					jx_at_cmd_table[AT_ACTION_MIC].atRetTailOk);
			rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
			break;
		}
		if (uValue == 0) 
		{
			StartTimer(jx_at_cmd_table[AT_ACTION_MIC].timerId, 0, jx_at_mic_handler_off);
		} 
		else if (uValue == 1) 
		{
			StartTimer(jx_at_cmd_table[AT_ACTION_MIC].timerId, 0, jx_at_mic_handler_on);
		} 
		ret_value = CUSTOM_RSP_OK;
	}
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#ifdef __STK_LIGHT_SENSOR__

void jx_at_ligs_handler(void)
{
    // 光感一般开机就打开，如果是没有打开的项目需要先打开,然后关闭
	//STK_Start_PS(NULL);
	//STK_Stop_PS();
	S8 buffer[MAX_UART_LEN + 1] = {0};
	
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_ligs_handler");
#endif	

	if (LS_check_device())
	{
		sprintf(buffer, "%s%s%d%s", 
						jx_at_cmd_table[AT_ACTION_LIGS].atRetHead, 
						"light sensor check ID ok! Read Data=", 
						STK_Read_PS(),
						jx_at_cmd_table[AT_ACTION_LIGS].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	} 
	else
	{
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_LIGS].atRetHead, 
						"light sensor check ID error!",
						jx_at_cmd_table[AT_ACTION_LIGS].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	}
}

custom_rsp_type_enum jx_at_ligs(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_LIGS].timerId, 0, jx_at_ligs_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#else /* __STK_LIGHT_SENSOR__ */

void jx_at_ligs_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_ligs_handler");
#endif	

	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_LIGS].atRetHead, 
					"This version not support light sensor!",
					jx_at_cmd_table[AT_ACTION_LIGS].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
}

custom_rsp_type_enum jx_at_ligs(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_LIGS].timerId, 0, jx_at_ligs_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#endif /* __STK_LIGHT_SENSOR__ */

#if defined(MOTION_SENSOR_SUPPORT) && !defined(WIN32)
void jx_at_gsens_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};
	S16 acc_x_adc, acc_y_adc, acc_z_adc;
	
	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_gsens_handler");
#endif	

	motion_sensor_power(KAL_TRUE);

    if(MS_check_device())
	{
		MS_get_xyz(&acc_x_adc, &acc_y_adc, &acc_z_adc);
		sprintf(buffer, "%s%s%d,%s%d,%s%d%s", 
						jx_at_cmd_table[AT_ACTION_GSENS].atRetHead, 
						"gsensor check ID ok! Read Data x=", 
						acc_x_adc,
						"y=", 
						acc_y_adc,
						"z=",
						acc_z_adc,
						jx_at_cmd_table[AT_ACTION_GSENS].atRetTailOk);
	} 
	else
	{
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_GSENS].atRetHead, 
						"gsensor check ID error!",
						jx_at_cmd_table[AT_ACTION_GSENS].atRetTailError);
	}

	motion_sensor_power(KAL_FALSE);

    rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
}

custom_rsp_type_enum jx_at_gsens(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_GSENS].timerId, 0, jx_at_gsens_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#else /* MOTION_SENSOR_SUPPORT */

void jx_at_gsens_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_gsens_handler");
#endif	

	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_GSENS].atRetHead, 
					"This version not support gsensor!",
					jx_at_cmd_table[AT_ACTION_GSENS].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
}

custom_rsp_type_enum jx_at_gsens(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_GSENS].timerId, 0, jx_at_gsens_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#endif /* MOTION_SENSOR_SUPPORT */

void jx_at_sim_get_iccid_by_bits( S8 *iccid_data, S8 *iccid, S32 iccid_len )
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
    S8 *tmp = iccid_data;
    S32 i = 0;

    /*----------------------------------------------------------------*/
    /* Code Body                                                      */
    /*----------------------------------------------------------------*/
    memset(iccid, 0, iccid_len);

    for (i = 0; i < 10; i++)
    {
        U8 l_data = tmp[i] & 0x0f;
        U8 h_data = (tmp[i] >> 4) & 0x0f;

        iccid[2 * i] = l_data > 9 ? l_data - 10 + 'A' : l_data + '0';
        iccid[2 * i + 1] = h_data > 9 ? h_data - 10 + 'A' : h_data + '0';
    }
}

void jx_at_sim_get_iccid_rsp( srv_sim_cb_struct *data )
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
	S8 buffer[MAX_UART_LEN + 1] = {0};
	S8 signalStrength = 0;
	srv_nw_info_cntx_struct* context = NULL;
    srv_sim_cb_struct* sim_cb_struct = data;
    srv_sim_data_struct* sim_data = (srv_sim_data_struct*)sim_cb_struct->data;

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_sim_get_iccid_rsp");
#endif

	if (!data)
	{
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_SIM].atRetHead, 
						"SIM=OK,get iccid error!",
						jx_at_cmd_table[AT_ACTION_SIM].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	}
	
    if (data->result)
	{
		#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_at_sim_get_iccid_rsp data->result = true");
		#endif
        jx_at_sim_get_iccid_by_bits((S8 *)sim_data->data, jx_at_phone_info.iccid, sizeof(jx_at_phone_info.iccid));
		srv_sim_ctrl_get_imsi(MMI_SIM1, jx_at_phone_info.imsi, sizeof(jx_at_phone_info.imsi));
		context = srv_nw_info_get_cntx(MMI_SIM1);
		signalStrength = context->signal.strength_in_percentage;
		
		if (signalStrength > 30)
		{
			sprintf(buffer, "%sSIM=%s,SIGNAL=%d,IMSI=%s,ICCID=%s%s", 
							jx_at_cmd_table[AT_ACTION_SIM].atRetHead, 
							"OK", 
							signalStrength,
							jx_at_phone_info.imsi,
							jx_at_phone_info.iccid,
							jx_at_cmd_table[AT_ACTION_SIM].atRetTailOk);
			rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		}
		else
		{
			sprintf(buffer, "%sSIM=%s,SIGNAL=%d,IMSI=%s,ICCID=%s%s", 
							jx_at_cmd_table[AT_ACTION_SIM].atRetHead, 
							"OK", 
							signalStrength,
							jx_at_phone_info.imsi,
							jx_at_phone_info.iccid,
							jx_at_cmd_table[AT_ACTION_SIM].atRetTailError);
			rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		}
		
    } 
	else 
	{
        sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_SIM].atRetHead, 
						"SIM=OK,get iccid error!",
						jx_at_cmd_table[AT_ACTION_SIM].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
    }
}

void jx_at_sim_handler(void)
{
	S8 signalStrength = 0;
	srv_nw_info_cntx_struct* context = NULL;
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_sim_handler imsi=%s,iccid=%s", jx_at_phone_info.imsi, jx_at_phone_info.iccid);
#endif	

	if (!srv_sim_ctrl_is_available(MMI_SIM1))
	{
		sprintf(buffer, "%sSIM=%s%s", 
						jx_at_cmd_table[AT_ACTION_SIM].atRetHead, 
						"FALSE",
						jx_at_cmd_table[AT_ACTION_SIM].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		return;
	}

	if ((jx_at_phone_info.imsi[0] != 0) && (jx_at_phone_info.iccid[0] != 0))
	{
		context = srv_nw_info_get_cntx(MMI_SIM1);
		signalStrength = context->signal.strength_in_percentage;
		//  修改为工具配置文件来判断 (signalStrength > 30)
		sprintf(buffer, "%sSIM=%s,SIGNAL=%d,IMSI=%s,ICCID=%s%s", 
						jx_at_cmd_table[AT_ACTION_SIM].atRetHead, 
						"OK", 
						signalStrength,
						jx_at_phone_info.imsi,
						jx_at_phone_info.iccid,
						jx_at_cmd_table[AT_ACTION_SIM].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);

	}
	else
	{
		srv_sim_read_record(FILE_ICCID_IDX, NULL, 0, 10, MMI_SIM1, jx_at_sim_get_iccid_rsp, NULL);
	}
}

custom_rsp_type_enum jx_at_sim(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_SIM].timerId, 0, jx_at_sim_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

void jx_at_vibr_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_vibr_handler");
#endif
	
	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_VIBR].atRetHead,
					"OK",
					jx_at_cmd_table[AT_ACTION_VIBR].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
	srv_vibrator_play_once();
}

custom_rsp_type_enum jx_at_vibr(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE:
		StartTimer(jx_at_cmd_table[AT_ACTION_VIBR].timerId, 0, jx_at_vibr_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#if defined(__KM_WIFI__)
#if defined(__WIFI_SCAN_ONLY__)
void jx_wlan_deinit_cb(void *user_data, wlan_deinit_cnf_struct *cnf)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_wlan_deinit_cb");
#endif

	if(cnf->status == SCANONLY_SUCCESS) 	//Deinit success
	{
		#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_wlan_deinit_cb success!");
		#endif
	
		sprintf(buffer, "%s", "WLAN scan ok,deinit ok!");
		rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
	}
	else
	{	
		#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_wlan_deinit_cb error!");
		#endif
		
		sprintf(buffer, "%s", "WLAN scan ok,deinit error!");
		rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
	}
}

void jx_wlan_scan_cb(void *user_data, wlan_scan_cnf_struct *cnf)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/
	U8 i = 0;
	S8 buffer[MAX_UART_LEN + 1] = {0};
	S8 temp[50] = {0};
	wlan_scan_cnf_struct *wifi_data = (wlan_scan_cnf_struct *)cnf;
	
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_wlan_scan_cb");
#endif

	if(cnf->status == SCANONLY_SUCCESS)
	{
		#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_wlan_scan_cb success! wifi_data->scan_ap_num=%d", wifi_data->scan_ap_num);
		#endif
		if (wifi_data->scan_ap_num > 0)
		{
		sprintf(buffer, "%sAPNUM=%d", jx_at_cmd_table[AT_ACTION_WIFI].atRetHead, wifi_data->scan_ap_num);
	    for(i=0; i<wifi_data->scan_ap_num && i<3; i++)
	    {
	        sprintf(temp, ",%s|%d", wifi_data->scan_ap[i].ssid, wifi_data->scan_ap[i].rssi);
			strcat(buffer, temp);
	    }
			strcat(buffer,jx_at_cmd_table[AT_ACTION_WIFI].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		}
		else
		{
			sprintf(buffer, "%sAPNUM=%d%s", 
							jx_at_cmd_table[AT_ACTION_WIFI].atRetHead, 
							wifi_data->scan_ap_num, 
							jx_at_cmd_table[AT_ACTION_WIFI].atRetTailError);
			rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		}
		wlan_deinit(NULL, jx_wlan_deinit_cb, NULL);
	}
    else
    {
    	#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_wlan_scan_cb error!");
		#endif
		
		sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_WIFI].atRetHead, 
					"WLAN scan callback error!", 
					jx_at_cmd_table[AT_ACTION_WIFI].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
    }
}

void jx_wlan_init_cb(void *user_data, wlan_init_cnf_struct *cnf)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_wlan_init_cb");
	#endif
	if(cnf->status == SCANONLY_SUCCESS) // WIFI init success
	{
	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_wlan_init_cb success!");
	#endif
		wlan_scan(NULL, jx_wlan_scan_cb, NULL);
	}
	else
	{
	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_wlan_init_cb error!");
	#endif
		sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_WIFI].atRetHead, 
					"WLAN init callback error!", 
					jx_at_cmd_table[AT_ACTION_WIFI].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	}
}
#else /* __WIFI_SCAN_ONLY__ */

#include "DtcntSrvGprot.h"

typedef struct
{
    U8 ap_list_num;/* total num of searched ap list */
    supc_abm_bss_info_struct ap_list[SRV_DTCNT_WLAN_MAX_AP_LIST_NUM]; /* bss info array */
    U32 scan_job_id;                            /* scan_job_id, 0xFFFFFF means scan result broadcasting */    
} JX_WIFI_DATA_STRUCT;

static JX_WIFI_DATA_STRUCT scanWifiData = {0};

static void jx_wlan_deinit_cb(void)
{
	kal_prompt_trace(MOD_ENG, "jx_wlan_deinit_cb");
	srv_dtcnt_wlan_scan_abort(scanWifiData.scan_job_id);
    scanWifiData.scan_job_id = 0;
}

static void jx_wlan_scan_cb(U32 job_id, void *user_data, srv_dtcnt_wlan_scan_result_struct *scan_res)
{
    U8 i = 0;
	U8 j = 0;
	S8 buffer[MAX_UART_LEN + 1] = {0};
	S8 temp[50] = {0};

	kal_prompt_trace(MOD_ENG, "jx_wlan_scan_cb");
	if ((!scan_res) || (scan_res->ap_list_num < 1))
    {
    	#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_wlan_scan_cb error!");
		#endif
						
		sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_WIFI].atRetHead, 
					"WLAN scan callback error!", 
					jx_at_cmd_table[AT_ACTION_WIFI].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
    }
	else
    {
    #if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_wlan_scan_cb(): ap_list_num = %d\n", scan_res->ap_list_num);
		kal_prompt_trace(MOD_ENG, "jx_wlan_scan_cb(): scan_job_id = %d\n", scan_res->scan_job_id);
	#endif 
		if (scanWifiData.scan_job_id == scan_res->scan_job_id)
		{
		    scanWifiData.ap_list_num = scan_res->ap_list_num;
		    for(i = 0;i<scan_res->ap_list_num;i++)
		    {
		        memcpy(&scanWifiData.ap_list[i], scan_res->ap_list[i], sizeof(supc_abm_bss_info_struct));
		        scanWifiData.ap_list[i].ssid[scanWifiData.ap_list[i].ssid_len] = '\0';
				
		        kal_prompt_trace(MOD_ENG,"get ssid[%d] = %s", i, scanWifiData.ap_list[i].ssid);
		        kal_prompt_trace(MOD_ENG,"get rssi[%d] = %d", i, scanWifiData.ap_list[i].rssi);
		    }

			sprintf(buffer, "%sAPNUM=%d", jx_at_cmd_table[AT_ACTION_WIFI].atRetHead, scanWifiData.ap_list_num);
		    for (i=0; (i < scanWifiData.ap_list_num) && (i < 3); i++)
		    {
		        sprintf(temp, ",%s|%d", scanWifiData.ap_list[i].ssid, scanWifiData.ap_list[i].rssi);
				strcat(buffer, temp);
		    }
			strcat(buffer,jx_at_cmd_table[AT_ACTION_WIFI].atRetTailOk);
			rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);

			StartTimer(jx_at_cmd_table[AT_ACTION_WIFI].timerCloseId, 0, jx_wlan_deinit_cb);
    	}
    }
}
#endif /* __WIFI_SCAN_ONLY__ */

void jx_at_wifi_handler(void)
{
	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_wifi_handler");
	#endif

#if defined(__WIFI_SCAN_ONLY__)
	wlan_init(NULL, jx_wlan_init_cb, NULL);//第三个参数user_data，有需要可以设置；RETURN:WLAN_RESULT_WOULDBLOCK；
#else /* __WIFI_SCAN_ONLY__ */
	memset(&scanWifiData, 0, sizeof(scanWifiData));
    scanWifiData.scan_job_id = srv_dtcnt_wlan_scan(jx_wlan_scan_cb, NULL);
    kal_prompt_trace(MOD_ENG,"JX_AutoTest_wifi_scan_one(): scan_job_id = %d\n", scanWifiData.scan_job_id);
#endif /* __WIFI_SCAN_ONLY__ */
}

custom_rsp_type_enum jx_at_wifi(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: {
		StartTimer(jx_at_cmd_table[AT_ACTION_WIFI].timerId, 0, jx_at_wifi_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	}
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#else /* __WIFI_SCAN_ONLY__ */

void jx_at_wifi_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};
		
	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_WIFI].atRetHead, 
					"This version not support WIFI!",
					jx_at_cmd_table[AT_ACTION_WIFI].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
}

custom_rsp_type_enum jx_at_wifi(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_WIFI].timerId, 0, jx_at_wifi_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#endif /* __WIFI_SCAN_ONLY__ */

#if defined(__GPS_SUPPORT__)
static S16 jx_2503_gps_port;	
typedef struct
{
	S32 fixed_time;
	kal_int8  status;
	double rmc_latitude; 
	double rmc_longitude;
	S32 num_sv_trk;
	kal_int8 north_south;  		/*N or S*/
    kal_int8 east_west;    		/*E or W*/
	S32 sates_in_view;
	struct
	{
	    kal_uint8 sate_id;              /*satellite id*/
	    kal_uint8 elevation;           	/*elevation in degrees*/
	    kal_int16 azimuth;             	/*azimuth in degrees to true*/
	    kal_uint8 snr;                 	/*SNR in dB*/
	} rsv[MDI_GPS_NMEA_MAX_SVVIEW];
}fm_2503_gps_data;

static fm_2503_gps_data GPS_DATA;

static kal_bool bGpsTestOK = FALSE;
static kal_bool bGpsTesting = FALSE;

static void jx_2503_gps_get_data_timer_handler(void);

static void jx_fm_2503_gps_nmea_rmc_callback(mdi_gps_nmea_rmc_struct *param)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/
	CHAR asc[128] = {0};
	double rmc_latitude = 0.0;                                      
    double rmc_longitude = 0.0;    
	kal_int8  status = 'V';
	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	if(param)
	{
		GPS_DATA.rmc_latitude = param->latitude;
		GPS_DATA.rmc_longitude = param->longitude;
		GPS_DATA.status = param->status;

		GPS_DATA.north_south= param->north_south;
		GPS_DATA.east_west= param->east_west;
		
		//sprintf(asc, "%0.2f %0.2f\n%c NS:%c EW:%c", GPS_DATA.rmc_latitude, GPS_DATA.rmc_longitude,GPS_DATA.status,GPS_DATA.north_south,GPS_DATA.east_west);
		//rmmi_write_to_uart(asc, strlen(asc), KAL_TRUE);
	}
}

static void jx_fm_2503_gps_nmea_gsv_callback(mdi_gps_nmea_gsv_struct *param)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/
	S32 i;
	WCHAR temp[128] = {0};
	CHAR asc[128] = {0};	
	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	if(param)
	{
		GPS_DATA.sates_in_view = param->sates_in_view;
		GPS_DATA.num_sv_trk = param->num_sv_trk;
		
		for(i=0;i<MDI_GPS_NMEA_MAX_SVVIEW;i++)
		{
			GPS_DATA.rsv[i].sate_id = param->rsv[i].sate_id;
			GPS_DATA.rsv[i].elevation = param->rsv[i].elevation;
			GPS_DATA.rsv[i].azimuth = param->rsv[i].azimuth;
			GPS_DATA.rsv[i].snr = param->rsv[i].snr;
		}
		//sprintf(asc,"%d %d\n%d[%d] %d[%d] %d[%d]", GPS_DATA.sates_in_view,GPS_DATA.num_sv_trk,
			//GPS_DATA.rsv[0].sate_id,GPS_DATA.rsv[0].snr,
			//GPS_DATA.rsv[1].sate_id,GPS_DATA.rsv[1].snr,
			//GPS_DATA.rsv[2].sate_id,GPS_DATA.rsv[2].snr);
		//rmmi_write_to_uart(asc, strlen(asc), KAL_TRUE);
	}	
}

static void jx_fm_2503_gps_nmea_gga_callback(mdi_gps_nmea_gga_struct *param)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/
	
	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	if(param)
	{
		//gga_north_south = param->north_south;
		//gga_east_west = param->east_west;
		//gga_quality = param->quality;
	}
}

static void jx_fm_2503_gps_nmea_gsa_callback(mdi_gps_nmea_gsa_struct *param)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/
	S32 i;
	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	if(param)
	{
		//gsa_pdop = param->pdop;
		//gsa_hdop = param->hdop;
	}
}

static void jx_fm_2503_gps_nmea_vtg_callback(mdi_gps_nmea_vtg_struct *param)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/

	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	if(param)
	{
		//vtg_hspeed_knot = param->hspeed_knot;
		//vtg_hspeed_jx = param->hspeed_jx;
	}
}

static void jx_fm_2503_gps_nmea_gll_callback(mdi_gps_nmea_gll_struct *param)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/
	WCHAR temp[128] = {0};
	CHAR asc[128] = {0};
	double rmc_latitude =0.0;                                      
    double rmc_longitude = 0.0;    
	kal_int8  status = 'V';
	kal_int8  EW = 'X';
	kal_int8  NS = 'X';
	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	if(param)
	{
		GPS_DATA.rmc_latitude = param->latitude;
		GPS_DATA.rmc_longitude = param->longitude;
		GPS_DATA.status = param->status;
		GPS_DATA.east_west = param->north_south;
		GPS_DATA.north_south = param->east_west;

		sprintf(asc, "%0.2f %0.2f\n%c %c %c", GPS_DATA.rmc_latitude, GPS_DATA.rmc_longitude,GPS_DATA.status,GPS_DATA.east_west,GPS_DATA.north_south);
		//mmi_asc_to_wcs(temp, asc);
		//mmi_display_popup(temp, MMI_EVENT_DEFAULT);
	}
}

static void jx_fm_2503_gps_nmea_callback(mdi_gps_parser_info_enum type, void *buffer, U32 length)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/

	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	if(buffer)
	{
		switch(type)
		{	//常用项:GGA,RMC,GSA,GSV,VTG,GLL
			case MDI_GPS_PARSER_NMEA_GGA:
				//JX_fm_2503_gps_nmea_gga_callback((mdi_gps_nmea_gga_struct *)buffer);
				break;
			case MDI_GPS_PARSER_NMEA_RMC:
				jx_fm_2503_gps_nmea_rmc_callback((mdi_gps_nmea_rmc_struct *)buffer);
				break;
			case MDI_GPS_PARSER_NMEA_GSA:
				//JX_fm_2503_gps_nmea_gsa_callback((mdi_gps_nmea_gsa_struct *)buffer);
				break;
			case MDI_GPS_PARSER_NMEA_GSV:
				jx_fm_2503_gps_nmea_gsv_callback((mdi_gps_nmea_gsv_struct *)buffer);
				break;    
			case MDI_GPS_PARSER_NMEA_VTG:
				//JX_fm_2503_gps_nmea_vtg_callback((mdi_gps_nmea_vtg_struct *)buffer);
				break;        
			case MDI_GPS_PARSER_NMEA_GLL:
				jx_fm_2503_gps_nmea_gll_callback((mdi_gps_nmea_gll_struct *)buffer);
				break;                                   
		}
	}
}

void jx_gps_stop( void )
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/

	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	S8 buffer[MAX_UART_LEN + 1] = {0};
	StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId);
	
	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_gps_stop");
	#endif

	L1SM_SleepEnable(gGpsUartHandle);
	
	if(!mdi_gps_is_parser_enabled())
	{
		mdi_gps_disable_parser();
	}
	mdi_gps_uart_close(jx_2503_gps_port, MDI_GPS_UART_MODE_LOCATION, jx_fm_2503_gps_nmea_callback);
    memset(&GPS_DATA, 0, sizeof(fm_2503_gps_data));
	bGpsTesting = FALSE;

	// 关闭吹麦测试
	jx_at_mic_close_by_others();
	// 关闭ADC检测
	//jx_at_stop_adc(); 
#if defined(__JX_LED_SUPPORT__)
	// 关闭LED灯测试
	bLEDTesting = FALSE;
	JX_led_status_stop_play();
#endif /* __JX_LED_SUPPORT__ */

	srv_backlight_turn_off();
	srv_backlight_turn_on(SRV_BACKLIGHT_SHORT_TIME);
	g_in_at_testing = KAL_FALSE;

	if (bGpsTestOK)
	{
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_GPS].atRetHead, 
						"GPS stop,GPS test pass!", 
						jx_at_cmd_table[AT_ACTION_GPS].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
	}
	else
	{
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_GPS].atRetHead, 
						"GPS stop,GPS test fail!",  
						jx_at_cmd_table[AT_ACTION_GPS].atRetTailError);
		rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
	}
}

static void jx_2503_gps_get_data_timer_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_gps_handler GPS_DATA.status=%d", GPS_DATA.status);
	#endif

	if (GPS_DATA.status == 'A')
	{
		sprintf(buffer, "%sTime=%d,status=%c(%d:%d) %d[%d] %d[%d] %d[%d] %d[%d] %d[%d] %d[%d]%s",
					jx_at_cmd_table[AT_ACTION_GPS].atRetHead,
					GPS_DATA.fixed_time,
					GPS_DATA.status, 
					GPS_DATA.sates_in_view,
					GPS_DATA.num_sv_trk, 
					//GPS_DATA.east_west,
					//GPS_DATA.north_south,
					GPS_DATA.rsv[0].sate_id, GPS_DATA.rsv[0].snr,
					GPS_DATA.rsv[1].sate_id, GPS_DATA.rsv[1].snr, 
					GPS_DATA.rsv[2].sate_id, GPS_DATA.rsv[2].snr,
					GPS_DATA.rsv[3].sate_id, GPS_DATA.rsv[3].snr,
					GPS_DATA.rsv[4].sate_id, GPS_DATA.rsv[4].snr,
					GPS_DATA.rsv[5].sate_id, GPS_DATA.rsv[5].snr,
					jx_at_cmd_table[AT_ACTION_GPS].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		
		StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId);
		StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerCloseId);
		bGpsTestOK = TRUE;
		jx_gps_stop();
	}
	else
	{
		sprintf(buffer, "%sTime=%d,status=%c(%d:%d) %d[%d] %d[%d] %d[%d] %d[%d] %d[%d] %d[%d]%s",
					jx_at_cmd_table[AT_ACTION_GPS].atRetHead,
					GPS_DATA.fixed_time,
					GPS_DATA.status, 
					GPS_DATA.sates_in_view,
					GPS_DATA.num_sv_trk, 
					//GPS_DATA.east_west,
					//GPS_DATA.north_south,
					GPS_DATA.rsv[0].sate_id, GPS_DATA.rsv[0].snr,
					GPS_DATA.rsv[1].sate_id, GPS_DATA.rsv[1].snr, 
					GPS_DATA.rsv[2].sate_id, GPS_DATA.rsv[2].snr,
					GPS_DATA.rsv[3].sate_id, GPS_DATA.rsv[3].snr,
					GPS_DATA.rsv[4].sate_id, GPS_DATA.rsv[4].snr,
					GPS_DATA.rsv[5].sate_id, GPS_DATA.rsv[5].snr,
					jx_at_cmd_table[AT_ACTION_GPS].atRetTailError);
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);

		GPS_DATA.fixed_time++;
		StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId, 1000, jx_2503_gps_get_data_timer_handler);
	}
}

void jx_at_gps_handler(void)
{
	/*----------------------------------------------------------------*/
	/* Local Variables												  */
	/*----------------------------------------------------------------*/
	S32 gps_handle;
	S8 buffer[MAX_UART_LEN + 1] = {0};

	/*----------------------------------------------------------------*/
	/* Code Body													  */
	/*----------------------------------------------------------------*/
	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_gps_handler bGpsTesting=%d", bGpsTesting);
	#endif
			
	jx_2503_gps_port = mdi_get_gps_port();
	memset(&GPS_DATA, 0, sizeof(fm_2503_gps_data));
	if(jx_2503_gps_port >= 0)
	{
		gps_handle = mdi_gps_uart_open(jx_2503_gps_port, MDI_GPS_UART_MODE_LOCATION, jx_fm_2503_gps_nmea_callback);
		
		if (gps_handle >= MDI_RES_GPS_UART_SUCCEED )
		{
			mdi_gps_set_work_port((U8)jx_2503_gps_port);
		}
		else if (gps_handle == MDI_RES_GPS_UART_ERR_PORT_ALREADY_OPEN)
		{
			mdi_gps_set_work_port((U8)jx_2503_gps_port);
		}
		else
		{
			sprintf(buffer, "%s", jx_at_cmd_table[AT_ACTION_GPS].atRetTailError);
			rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
		}

		mdi_gps_uart_cmd(jx_2503_gps_port, MDI_GPS_UART_GPS_HOT_START, NULL);

		if(!mdi_gps_is_parser_enabled())
		{
			mdi_gps_enable_parser();
		}

		bGpsTesting = TRUE;
		if (0 == gGpsUartHandle)
		{
			gGpsUartHandle = L1SM_GetHandle();
		}
		L1SM_SleepDisable(gGpsUartHandle);
	
		StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId, 1000, jx_2503_gps_get_data_timer_handler);
		StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerCloseId, 2*60*1000, jx_gps_stop);
	}
	else
	{
		sprintf(buffer, "%s", jx_at_cmd_table[AT_ACTION_GPS].atRetTailError);
		rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
	}
}

custom_rsp_type_enum jx_at_gps(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		if (bGpsTesting)
		{
			S8 buffer[MAX_UART_LEN + 1] = {0};
			#if JX_AT_CMD_DEBUG
			kal_prompt_trace(MOD_ENG, "jx_at_gps bGpsTesting=TRUE");
			#endif
			sprintf(buffer, "%s%s%s", 
							jx_at_cmd_table[AT_ACTION_GPS].atRetHead, 
							"GPS is in testing!",  
							jx_at_cmd_table[AT_ACTION_GPS].atRetTailError);
			rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
		}
		else
		{
			#if JX_AT_CMD_DEBUG
			kal_prompt_trace(MOD_ENG, "jx_at_gps bGpsTesting=FALSE");
			#endif
			
			bGpsTestOK = FALSE;
			StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId);
			StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerCloseId);
			StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId, 0, jx_at_gps_handler);
		}
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#elif defined(__JX_U_BLOX_GPS_SUPPORT__)

#include "ublox_gps_Gport.h"
//GSV 相关数据全局变量
extern kal_uint8 numMsg; 	//Number of messages
extern kal_uint8 msgNum; 	//Number of this message
extern kal_uint8 numSV; 		//Number of satellites in view
extern kal_uint16 Satellite_info[12][4];

extern kal_uint8 navMode;
extern kal_uint8 Satellite[12];


static kal_bool bGpsTestOK = FALSE;
static kal_bool bGpsTesting = FALSE;

static S32 timeSecond;
static S32 timeSecondHeart;

BOOL JX_gps_is_testing()
{
	return bGpsTesting;
}

void JX_ublox_gps_eng_test_end(void)
{
	int i=0;
	S8 buffer[MAX_UART_LEN + 1] = {0};
	
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "JX_ublox_gps_eng_test_end");
#endif
	StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId);

	// 关闭吹麦测试
	//jx_at_mic_close_by_others();
	// 关闭ADC检测
	//jx_at_stop_adc(); 
#if defined(__JX_LED_SUPPORT__)
	// 关闭LED灯测试
	bLEDTesting = FALSE;
	JX_led_status_stop_play();
#endif /* __JX_LED_SUPPORT__ */

	srv_backlight_turn_off();
	srv_backlight_turn_on(SRV_BACKLIGHT_SHORT_TIME);
	g_in_at_testing = KAL_FALSE;

	bGpsTesting = FALSE;
	timeSecond = 0;

	navMode = 0;
	for(i=0;i<12;i++)
	{
		Satellite[i] =0;
	}
	ublox_test_power_off();

	if (bGpsTestOK)
	{
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_GPS].atRetHead, 
						"GPS stop,GPS test pass!", 
						jx_at_cmd_table[AT_ACTION_GPS].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
	}
	else
	{
		sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_GPS].atRetHead, 
						"GPS stop,GPS test fail!",  
						jx_at_cmd_table[AT_ACTION_GPS].atRetTailError);
		rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
	}
}

void JX_ublox_gps_eng_timer_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};
	S8 asc[301] = {0}, unicode[602] = {0};
	ublox_gps_rmc_message_struct gps_data = {0};

	ublox_gps_get_gps_rmc_data(&gps_data);
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_gps_handler GPS_DATA.status=%c", gps_data.status);
#endif
	
	if (gps_data.status == 'A')
	{
	#if defined(GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH)
		sprintf(buffer, "%sTime=%d,status=%c(%d|%d)\r\n %d[%d] %d[%d] %d[%d] %d[%d]\r\n %d[%d] %d[%d] %d[%d] %d[%d]\r\n %d[%d] %d[%d] %d[%d] %d[%d]%s",
			jx_at_cmd_table[AT_ACTION_GPS].atRetHead,
			timeSecond,
			gps_data.status,
			navMode,
			numSV,
			Satellite_info[0][0],
			Satellite_info[0][3],
			Satellite_info[1][0],
			Satellite_info[1][3],
			Satellite_info[2][0],
			Satellite_info[2][3],
			Satellite_info[3][0],
			Satellite_info[3][3],
			Satellite_info[4][0],
			Satellite_info[4][3],
			Satellite_info[5][0],
			Satellite_info[5][3],
			Satellite_info[6][0],
			Satellite_info[6][3],
			Satellite_info[7][0],
			Satellite_info[7][3],
			Satellite_info[8][0],
			Satellite_info[8][3],
			Satellite_info[9][0],
			Satellite_info[9][3],
			Satellite_info[10][0],
			Satellite_info[10][3],
			Satellite_info[11][0],
			Satellite_info[11][3],
			jx_at_cmd_table[AT_ACTION_GPS].atRetTailOk);
	#else /* GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH */
		sprintf(buffer, "%sTime=%d,status=%c(%d|%d)\n %d[%d] %d[%d] %d[%d] %d[%d]%s",
				jx_at_cmd_table[AT_ACTION_GPS].atRetHead,
				timeSecond,
				gps_data.status,
				navMode,
				numSV,
				Satellite_info[0][0],
				Satellite_info[0][3],
				Satellite_info[1][0],
				Satellite_info[1][3],
				Satellite_info[2][0],
				Satellite_info[2][3],
				Satellite_info[3][0],
				Satellite_info[3][3],
				jx_at_cmd_table[AT_ACTION_GPS].atRetTailOk);
	#endif /* GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH */
		
		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		
		#if defined(GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH)
		timeSecond++;
		StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId, 1000, JX_ublox_gps_eng_timer_handler);
		#else /* GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH */
		StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId);
		StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerCloseId);
		bGpsTestOK = TRUE;
		JX_ublox_gps_eng_test_end();
		#endif /* GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH */
	}
	else
	{
#if defined(GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH)
			sprintf(buffer, "%sTime=%d,status=%c(%d|%d)\r\n %d[%d] %d[%d] %d[%d] %d[%d]\r\n %d[%d] %d[%d] %d[%d] %d[%d]\r\n %d[%d] %d[%d] %d[%d] %d[%d]%s",
				jx_at_cmd_table[AT_ACTION_GPS].atRetHead,
				timeSecond,
				gps_data.status,
				navMode,
				numSV,
				Satellite_info[0][0],
				Satellite_info[0][3],
				Satellite_info[1][0],
				Satellite_info[1][3],
				Satellite_info[2][0],
				Satellite_info[2][3],
				Satellite_info[3][0],
				Satellite_info[3][3],
				Satellite_info[4][0],
				Satellite_info[4][3],
				Satellite_info[5][0],
				Satellite_info[5][3],
				Satellite_info[6][0],
				Satellite_info[6][3],
				Satellite_info[7][0],
				Satellite_info[7][3],
				Satellite_info[8][0],
				Satellite_info[8][3],
				Satellite_info[9][0],
				Satellite_info[9][3],
				Satellite_info[10][0],
				Satellite_info[10][3],
				Satellite_info[11][0],
				Satellite_info[11][3],
				jx_at_cmd_table[AT_ACTION_GPS].atRetTailOk);	
#else /* GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH */
		sprintf(buffer, "%sTime=%d,status=%c(%d|%d)\n %d[%d] %d[%d] %d[%d] %d[%d]%s",
				jx_at_cmd_table[AT_ACTION_GPS].atRetHead,
				timeSecond,
				gps_data.status,
				navMode,
				numSV,
				Satellite_info[0][0],
				Satellite_info[0][3],
				Satellite_info[1][0],
				Satellite_info[1][3],
				Satellite_info[2][0],
				Satellite_info[2][3],
				Satellite_info[3][0],
				Satellite_info[3][3],
				jx_at_cmd_table[AT_ACTION_GPS].atRetTailError);
#endif /* GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH */

#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "buffer=%s", buffer);
#endif

		rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
		timeSecond++;
		StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId, 1000, JX_ublox_gps_eng_timer_handler);
	}
}

void jx_at_gps_handler(void)
{
	bGpsTesting = TRUE;
	ublox_test_power_on();
	JX_ublox_gps_eng_timer_handler();
	#if defined(GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH)
	StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerCloseId, 10*60*1000, JX_ublox_gps_eng_test_end);
	#else
	StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerCloseId, 2*60*1000, JX_ublox_gps_eng_test_end);
	#endif /* GPS_SHOW_ALL_SATALITE_SIGNAL_STRENGTH */
}

custom_rsp_type_enum jx_at_gps(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		if (bGpsTesting)
		{
			S8 buffer[MAX_UART_LEN + 1] = {0};
			#if JX_AT_CMD_DEBUG
			kal_prompt_trace(MOD_ENG, "jx_at_gps bGpsTesting=TRUE");
			#endif
			sprintf(buffer, "%s%s%s", 
							jx_at_cmd_table[AT_ACTION_GPS].atRetHead, 
							"GPS is in testing!",  
							jx_at_cmd_table[AT_ACTION_GPS].atRetTailError);
			rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
		}
		else
		{
			#if JX_AT_CMD_DEBUG
			kal_prompt_trace(MOD_ENG, "jx_at_gps bGpsTesting=FALSE");
			#endif
			
			bGpsTestOK = FALSE;
			StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId);
			StopTimer(jx_at_cmd_table[AT_ACTION_GPS].timerCloseId);
			StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId, 0, jx_at_gps_handler);
		}
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#else /* __GPS_SUPPORT__ */

void jx_at_gps_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};
	
	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_GPS].atRetHead, 
					"This version not support GPS!",
					jx_at_cmd_table[AT_ACTION_GPS].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
}

custom_rsp_type_enum jx_at_gps(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_GPS].timerId, 0, jx_at_gps_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#endif /* __GPS_SUPPORT__ */

mmi_ret jx_keypad_entry_test_screen_proc(mmi_event_struct *param)
{
	/*----------------------------------------------------------------*/
	/* Local Variables												  */
	/*----------------------------------------------------------------*/
	
	/*----------------------------------------------------------------*/
	/* Code Body													  */
	/*----------------------------------------------------------------*/
	return MMI_RET_OK;
}

typedef struct
{
	U16 key_code;
	BOOL bPressed;
}jx_at_keyp_struct;

#if defined(__JX_K9_COMMON__)

jx_at_keyp_struct jx_at_keyp_need_test[] = 
{
	{KEY_LEFT_ARROW,  0},
	{KEY_RIGHT_ARROW, 0},
	{KEY_DOWN_ARROW,  0},
	{KEY_END,         0},
};

#elif defined(__S1_HMR__) || defined(__JX_T1_3G02__)

jx_at_keyp_struct jx_at_keyp_need_test[] = 
{
	{KEY_END,         0},
};

#elif defined(__JX_S12_COMMON__) || defined(__JX_T2_3G02__)

jx_at_keyp_struct jx_at_keyp_need_test[] = 
{
	{KEY_LEFT_ARROW,  0},
	{KEY_RIGHT_ARROW, 0},
	{KEY_UP_ARROW,  0},
	{KEY_DOWN_ARROW,  0},
	{KEY_END,         0},
};

#elif defined(__JX_S6_COMMON__)

jx_at_keyp_struct jx_at_keyp_need_test[] = 
{
	{KEY_VOL_UP,         0},
    {KEY_VOL_DOWN,         0},
    {KEY_END,         0},
};

#else

#error "Please config need test keypad first!"

#endif

BOOL jx_at_keyp_test_screen(void)
{
	if (GetActiveScreenId() == SCR_ID_JX_KEYPAD_TEST)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void jx_at_keyp_init()
{
	int i;
	int count = sizeof(jx_at_keyp_need_test)/sizeof(jx_at_keyp_struct);

	for(i=0;i<count;i++)
	{
		jx_at_keyp_need_test[i].bPressed = 0;
	}
	kal_prompt_trace(MOD_ENG,"jx_at_keyp_init");
}

BOOL jx_at_keyp_all_key_pressed()
{
	int i;
	int count = sizeof(jx_at_keyp_need_test)/sizeof(jx_at_keyp_struct);
	BOOL bFlag = MMI_TRUE;

	for(i=0;i<count;i++)
	{
		if (jx_at_keyp_need_test[i].bPressed == 0)
		{
			bFlag = MMI_FALSE;
			break;
		}
	}

	return bFlag;
}

void jx_keyinfo_keyhandler(void)
{
	U16 code, type;
	S8 buffer[MAX_UART_LEN + 1] = {0};
	wchar_t disKeyName[30] = {0};
	
	int i;
	int count = sizeof(jx_at_keyp_need_test)/sizeof(jx_at_keyp_struct);

#if defined(__JX_USE_SOFT_IIC_LCD_96X16__)
	wchar_t *wKeyStr[] = 
	{
		L"KEY_0", 			L"KEY_1", 			L"KEY_2", 			L"KEY_3", 			L"KEY_4", 			L"KEY_5",
		L"KEY_6", 			L"KEY_7", 			L"KEY_8", 			L"KEY_9", 			L"KEY_LSK",         L"KEY_RSK", 
		L"KEY_CSK",			L"U",				L"D", 				L"L", 				L"R", 				L"KEY_SEND",  
		L"E",         		L"KEY_STAR",		L"KEY_POUND", 		L"KEY_VOL_UP", 		L"KEY_VOL_DOWN", 	L"KEY_STAR",		
		L"KEY_CAMERA",		L"KEY_ENTER",		L"KEY_WAP", 		L"KEY_IP", 			L"KEY_EXTRA_1", 	L"KEY_EXTRA_2", 		
		L"KEY_PLAY_STOP",   L"KEY_FWD",			L"KEY_BACK", 		L"KEY_EXTRA_A"      L"KEY_EXTRA_B"
	};
#else /* __JX_USE_SOFT_IIC_LCD_96X16__ */
	wchar_t *wKeyStr[] = 
	{
		L"KEY_0",			L"KEY_1",			L"KEY_2",			L"KEY_3",			L"KEY_4",			L"KEY_5",
		L"KEY_6",			L"KEY_7",			L"KEY_8",			L"KEY_9",			L"KEY_LSK", 		L"KEY_RSK", 
		L"KEY_CSK", 		L"KEY_UP_ARROW",	L"KEY_DOWN_ARROW",	L"KEY_LEFT_ARROW",	L"KEY_RIGHT_ARROW", L"KEY_SEND",  
		L"KEY_END", 		L"KEY_STAR",		L"KEY_POUND",		L"KEY_VOL_UP",		L"KEY_VOL_DOWN",	L"KEY_STAR",		
		L"KEY_CAMERA",		L"KEY_ENTER",		L"KEY_WAP", 		L"KEY_IP",			L"KEY_EXTRA_1", 	L"KEY_EXTRA_2", 		
		L"KEY_PLAY_STOP",	L"KEY_FWD", 		L"KEY_BACK",		L"KEY_EXTRA_A"		L"KEY_EXTRA_B"
	};
#endif /* __JX_USE_SOFT_IIC_LCD_96X16__ */

	char *cKeyStr[] = 
	{
		"KEY_0",			"KEY_1",			"KEY_2",			"KEY_3",			"KEY_4",			"KEY_5",
		"KEY_6",			"KEY_7",			"KEY_8",			"KEY_9",			"KEY_LSK", 			"KEY_RSK", 
		"KEY_CSK", 			"KEY_UP_ARROW",		"KEY_DOWN_ARROW",	"KEY_LEFT_ARROW",	"KEY_RIGHT_ARROW", 	"KEY_SEND",  
		"KEY_END", 			"KEY_STAR",			"KEY_POUND",		"KEY_VOL_UP",		"KEY_VOL_DOWN",		"KEY_STAR",		
		"KEY_CAMERA",		"KEY_ENTER",		"KEY_WAP", 			"KEY_IP",			"KEY_EXTRA_1", 		"KEY_EXTRA_2", 		
		"KEY_PLAY_STOP",	"KEY_FWD", 			"KEY_BACK",			"KEY_EXTRA_A"		"KEY_EXTRA_B"
	};

	if (GetActiveScreenId() == SCR_ID_JX_KEYPAD_TEST)
	{
		GetkeyInfo(&code, &type);
		srv_vibrator_play_once();

		for(i=0;i<count;i++)
		{
			if (code == jx_at_keyp_need_test[i].key_code)
			{
				jx_at_keyp_need_test[i].bPressed = 1;
				break;
			}
		}

		entry_full_screen();
		gdi_layer_clear(GDI_COLOR_WHITE);
		
		gui_set_font(&MMI_medium_font);
		gui_set_text_color(gui_color(0, 0, 0));

		for(i=0;i<count;i++)
		{
		 	if (jx_at_keyp_need_test[i].bPressed == 0)
		 	{
			#if defined(__JX_USE_SOFT_IIC_LCD_96X16__)
				gui_move_text_cursor(1+16*i, 1);
			#else /* __JX_USE_SOFT_IIC_LCD_96X16__ */
			 gui_move_text_cursor(1, 1+16*i);
			#endif /* __JX_USE_SOFT_IIC_LCD_96X16__ */
			 gui_print_text((UI_string_type)wKeyStr[jx_at_keyp_need_test[i].key_code]);
		 	}
		}
	
		gui_BLT_double_buffer(0, 0, UI_device_width-1, UI_device_height-1);

		if (jx_at_keyp_all_key_pressed())
		{
			jx_at_keyp_init();
			mmi_frm_scrn_close_active_id();
			sprintf(buffer, "%s%s%s", 
						jx_at_cmd_table[AT_ACTION_KEYP].atRetHead, 
						"ALL KEY TEST PASS!",  // do not modify,pc tools use it
						jx_at_cmd_table[AT_ACTION_KEYP].atRetTailOk);
			rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
		}
		else
		{
			sprintf(buffer, "%s%s pressed! %s", 
							jx_at_cmd_table[AT_ACTION_KEYP].atRetHead, 
							cKeyStr[code],
							jx_at_cmd_table[AT_ACTION_KEYP].atRetTailOk);
			rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
		}
	}
}

void jx_entry_keypad_test_screen(void)
{
	int i;
	int count = sizeof(jx_at_keyp_need_test)/sizeof(jx_at_keyp_struct);
	S8 buffer[MAX_UART_LEN + 1] = {0};

    U16 current_keymap[] = 
	{
		KEY_0, 			KEY_1,			KEY_2, 				KEY_3, 			KEY_4, 			KEY_5, 
		KEY_6, 			KEY_7, 			KEY_8, 				KEY_9,			KEY_LSK, 		KEY_RSK,
		KEY_CSK,		KEY_UP_ARROW, 	KEY_DOWN_ARROW, 	KEY_LEFT_ARROW, KEY_RIGHT_ARROW,KEY_SEND, 		
		KEY_END,		KEY_STAR, 		KEY_POUND, 			KEY_VOL_UP, 	KEY_VOL_DOWN, 	KEY_STAR, 		
		KEY_CAMERA, 	KEY_ENTER, 		KEY_WAP,			KEY_IP, 		KEY_EXTRA_1, 	KEY_EXTRA_2, 	
		KEY_PLAY_STOP, 	KEY_FWD, 		KEY_BACK, 			KEY_EXTRA_A,	KEY_EXTRA_B
	};
	
	#if defined(__JX_USE_SOFT_IIC_LCD_96X16__)
	wchar_t *wKeyStr[] = 
	{
		L"KEY_0", 			L"KEY_1", 			L"KEY_2", 			L"KEY_3", 			L"KEY_4", 			L"KEY_5",
		L"KEY_6", 			L"KEY_7", 			L"KEY_8", 			L"KEY_9", 			L"KEY_LSK",         L"KEY_RSK", 
		L"KEY_CSK",			L"U",				L"D", 				L"L", 				L"R", 				L"KEY_SEND",  
		L"E",         		L"KEY_STAR",		L"KEY_POUND", 		L"KEY_VOL_UP", 		L"KEY_VOL_DOWN", 	L"KEY_STAR",		
		L"KEY_CAMERA",		L"KEY_ENTER",		L"KEY_WAP", 		L"KEY_IP", 			L"KEY_EXTRA_1", 	L"KEY_EXTRA_2", 		
		L"KEY_PLAY_STOP",   L"KEY_FWD",			L"KEY_BACK", 		L"KEY_EXTRA_A"      L"KEY_EXTRA_B"
	};
	#else /* __JX_USE_SOFT_IIC_LCD_96X16__ */
	wchar_t *wKeyStr[] = 
	{
		L"KEY_0", 			L"KEY_1", 			L"KEY_2", 			L"KEY_3", 			L"KEY_4", 			L"KEY_5",
		L"KEY_6", 			L"KEY_7", 			L"KEY_8", 			L"KEY_9", 			L"KEY_LSK",         L"KEY_RSK", 
		L"KEY_CSK",			L"KEY_UP_ARROW",	L"KEY_DOWN_ARROW", 	L"KEY_LEFT_ARROW", 	L"KEY_RIGHT_ARROW", L"KEY_SEND",  
		L"KEY_END",         L"KEY_STAR",		L"KEY_POUND", 		L"KEY_VOL_UP", 		L"KEY_VOL_DOWN", 	L"KEY_STAR",		
		L"KEY_CAMERA",		L"KEY_ENTER",		L"KEY_WAP", 		L"KEY_IP", 			L"KEY_EXTRA_1", 	L"KEY_EXTRA_2", 		
		L"KEY_PLAY_STOP",   L"KEY_FWD",			L"KEY_BACK", 		L"KEY_EXTRA_A"      L"KEY_EXTRA_B"
	};
	#endif /* __JX_USE_SOFT_IIC_LCD_96X16__ */

	U8 current_key_count = sizeof(current_keymap) / sizeof(U16);

	mmi_frm_group_create_ex(
			GRP_ID_ROOT, 
			GRP_ID_JX_KEYPAD_TEST, 
			jx_keypad_entry_test_screen_proc, 
			NULL,
			MMI_FRM_NODE_SMART_CLOSE_FLAG);
	mmi_frm_group_enter(GRP_ID_JX_KEYPAD_TEST, MMI_FRM_NODE_SMART_CLOSE_FLAG);

	if (FALSE == mmi_frm_scrn_enter(GRP_ID_JX_KEYPAD_TEST, SCR_ID_JX_KEYPAD_TEST,NULL, jx_entry_keypad_test_screen, MMI_FRM_FULL_SCRN))
	{
		sprintf(buffer, "%s%s key pressed! %s", 
						jx_at_cmd_table[AT_ACTION_KEYP].atRetHead, 
						"Entry keypad test screen error!",
						jx_at_cmd_table[AT_ACTION_KEYP].atRetTailOk);
		rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
		return;
	}

	entry_full_screen();
	gdi_layer_clear(GDI_COLOR_WHITE);
	
	gui_set_font(&MMI_medium_font);
	gui_set_text_color(gui_color(0, 0, 0));

	for(i=0; i<count; i++)
	{
	#if defined(__JX_USE_SOFT_IIC_LCD_96X16__)
		gui_move_text_cursor(1+16*i, 1);
	#else /* __JX_USE_SOFT_IIC_LCD_96X16__ */
		gui_move_text_cursor(1, 1+16*i);
	#endif /* __JX_USE_SOFT_IIC_LCD_96X16__ */
		gui_print_text((UI_string_type)wKeyStr[jx_at_keyp_need_test[i].key_code]);
	}
	gui_BLT_double_buffer(0, 0, UI_device_width-1, UI_device_height-1);
	
	ClearKeyHandler(KEY_END, KEY_EVENT_DOWN);
	ClearKeyHandler(KEY_END, KEY_EVENT_UP);
	ClearKeyHandler(KEY_END,KEY_EVENT_LONG_PRESS);
	ClearKeyEvents();

	SetGroupKeyHandler(jx_keyinfo_keyhandler, (PU16) current_keymap, current_key_count, KEY_EVENT_DOWN);
	 /* Long press END key to exit*/
    SetKeyHandler(mmi_frm_scrn_close_active_id, KEY_END, KEY_EVENT_LONG_PRESS);
}

void jx_at_keyp_handler()
{
	#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_keyp_handler");
	#endif

	if (GetActiveScreenId() == SCR_ID_JX_KEYPAD_TEST)
	{
		#if JX_AT_CMD_DEBUG
		kal_prompt_trace(MOD_ENG, "jx_at_keyp_handler GetActiveScreenId() == SCR_ID_JX_KEYPAD_TEST");
		#endif
		return;
	}
	jx_at_keyp_init();
    jx_entry_keypad_test_screen();
}

custom_rsp_type_enum jx_at_keyp(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) 
	{
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_KEYP].timerId, 0, jx_at_keyp_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#if defined(__JX_HEARTRATE_SUPPORT__)
extern kal_uint8 HR_get_value(void);

void jx_at_heart_timer_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};
	timeSecondHeart++;

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_heart_timer_handler,heartrate=%d", HR_get_value());
#endif	

	sprintf(buffer, "%sTime=%d,%s%d%s", 
					jx_at_cmd_table[AT_ACTION_HEART].atRetHead, 
					timeSecondHeart,
					"HeartRate=",
					HR_get_value(),
					jx_at_cmd_table[AT_ACTION_HEART].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
	StartTimer(jx_at_cmd_table[AT_ACTION_HEART].timerId, 1000, jx_at_heart_timer_handler);
}

void jx_at_heart_end_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};
	
	timeSecondHeart = 0;
	StopTimer(jx_at_cmd_table[AT_ACTION_HEART].timerId);
	HR_close();
	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_HEART].atRetHead, 
					"Heart rate test end!",
					jx_at_cmd_table[AT_ACTION_HEART].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
}

void jx_at_heart_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_heart_handler");
#endif	
	timeSecondHeart = 0;

	HR_open();
	StartTimer(jx_at_cmd_table[AT_ACTION_HEART].timerId, 1500, jx_at_heart_timer_handler);
	StartTimer(jx_at_cmd_table[AT_ACTION_HEART].timerCloseId, 2*60*1000, jx_at_heart_end_handler);
}

custom_rsp_type_enum jx_at_heart(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_HEART].timerId, 0, jx_at_heart_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#else /* __JX_HEARTRATE_SUPPORT__ */

void jx_at_heart_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_heart_handler");
#endif	

	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_HEART].atRetHead, 
					"This version not support Heart Rate!",
					jx_at_cmd_table[AT_ACTION_HEART].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
}

custom_rsp_type_enum jx_at_heart(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_HEART].timerId, 0, jx_at_heart_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#endif /* __JX_HEARTRATE_SUPPORT__ */

#if defined(__JX_LED_SUPPORT__)
BOOL jx_at_led_testing(void)
{
	return bLEDTesting;
}

void jx_at_led_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};
#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_led_handler");
#endif	
    bLEDTesting = TRUE;
	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_LED].atRetHead, 
					"LED turn on ok,please check it!", 
					jx_at_cmd_table[AT_ACTION_LED].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);

#if defined(__JX_LED_SUPPORT__) && (defined(__JX_K9_COMMON__) || defined(__JX_S12_COMMON__)) // K9 测试LED暂放这里
	JX_led_status_play_autotest();
#endif
}

custom_rsp_type_enum jx_at_led(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_LED].timerId, 0, jx_at_led_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#else /* __JX_LED_SUPPORT__ */

void jx_at_led_handler(void)
{
	S8 buffer[MAX_UART_LEN + 1] = {0};

#if JX_AT_CMD_DEBUG
	kal_prompt_trace(MOD_ENG, "jx_at_led_handler");
#endif	

	sprintf(buffer, "%s%s%s", 
					jx_at_cmd_table[AT_ACTION_LED].atRetHead, 
					"This version not support LED!",
					jx_at_cmd_table[AT_ACTION_LED].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*)buffer, strlen(buffer), KAL_TRUE);
}

custom_rsp_type_enum jx_at_led(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) {
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_LED].timerId, 0, jx_at_led_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#endif /* __JX_LED_SUPPORT__ */


#define  MAX_JX_ADC_MAP_TAB_SIZE 3   /* size of jx_adc_map_tab */

typedef struct
{
	DCL_ADC_CHANNEL_TYPE_ENUM type;
	CHAR format[15]; 
	double adc_value;
	CHAR*  name;
	CHAR display[30];
}jx_adc_view_struct;

/*This table is for showing ADC menu list*/
jx_adc_view_struct jx_adc_map_tab[3]=
{
	{DCL_VBAT_ADC_CHANNEL,		"%s %4.3f V",	0.0,	"VBAT",	   "0"},
	{DCL_VISENSE_ADC_CHANNEL,	"%s %4.3f A",  	0.0,   	"CURRENT", "0"},
	{DCL_VCHARGER_ADC_CHANNEL,	"%s %4.3f V",  	0.0,   	"VCHGR",   "0"},
};

void jx_at_stop_adc(void)
{
	kal_prompt_trace(MOD_ENG, "jx_at_stop_adc");
	mmi_frm_set_protocol_event_handler(MSG_ID_MMI_EQ_ADC_ALL_CHANNEL_IND, NULL, MMI_FALSE);
	mmi_frm_set_protocol_event_handler(MSG_ID_MMI_EQ_BATTERY_STATUS_IND, BatteryStatusRsp, MMI_FALSE);
	mmi_frm_send_ilm((oslModuleType)MOD_L4C, (oslMsgType)MSG_ID_MMI_EQ_STOP_ADC_ALL_CHANNEL_REQ, NULL, NULL);
}

static void jx_at_update_adc_hdlr(void *inMsg)
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/ 
    U32 i = 0;
	S8 buffer[MAX_UART_LEN + 1] = {0};
	S8 temp[100] = {0};
    mmi_eq_adc_all_channel_ind_struct *adc_struct = (mmi_eq_adc_all_channel_ind_struct*) inMsg;
	DCL_HANDLE adc_handle;
	ADC_CTRL_GET_PHYSICAL_CHANNEL_T adc_ch;

    /*----------------------------------------------------------------*/
    /* Code Body                                                      */
    /*----------------------------------------------------------------*/
	kal_prompt_trace(MOD_ENG, "jx_at_update_adc_hdlr");
	/* update adc_value */
	jx_adc_map_tab[0].adc_value = (double)adc_struct->vbat / 1000000.0;
	jx_adc_map_tab[1].adc_value = (double)adc_struct->charge_current / 1000000.0;
	jx_adc_map_tab[2].adc_value = (double)adc_struct->vcharge / 1000000.0;
	
	adc_handle = DclSADC_Open(DCL_ADC, FLAGS_NONE);
	if(adc_handle == DCL_HANDLE_INVALID)
	{
		 ASSERT(0);   
	}

	for (i=0; i<MAX_JX_ADC_MAP_TAB_SIZE; i++)
	{
		adc_ch.u2AdcName = jx_adc_map_tab[i].type;
		DclSADC_Control(adc_handle, ADC_CMD_GET_CHANNEL, (DCL_CTRL_DATA_T *)&adc_ch);
		if (adc_ch.u1AdcPhyCh != DCL_ADC_ERR_CHANNEL_NO)
		{
		    if (((adc_struct->charge_current) & 0x80000000) && 
				jx_adc_map_tab[i].type == DCL_VISENSE_ADC_CHANNEL)
         	{                
				memcpy(jx_adc_map_tab[i].format, "%s n/a", 7);      // This is a special condition
			}
			else if (jx_adc_map_tab[i].type == DCL_VISENSE_ADC_CHANNEL)
			{
				
				memcpy(jx_adc_map_tab[i].format, "%s %4.3f A", 13);  
			}

			memset(&jx_adc_map_tab[i].display, 0, sizeof(jx_adc_map_tab[i].display));
			sprintf(jx_adc_map_tab[i].display, jx_adc_map_tab[i].format, jx_adc_map_tab[i].name, jx_adc_map_tab[i].adc_value);    
		}
	}

	DclSADC_Close(adc_handle);

	strcpy(temp, jx_adc_map_tab[0].display);
	strcat(temp, ",");
	strcat(temp, jx_adc_map_tab[1].display);
	strcat(temp, ",");
	strcat(temp, jx_adc_map_tab[2].display);
	kal_prompt_trace(MOD_ENG, "temp=%s", temp);
	sprintf(buffer, "%s%s%s", 
				jx_at_cmd_table[AT_ACTION_ADC].atRetHead, 
				temp,
				jx_at_cmd_table[AT_ACTION_ADC].atRetTailOk);
	rmmi_write_to_uart((kal_uint8*) buffer, strlen(buffer), KAL_TRUE);
}

void jx_at_adc_handler(void)
{
	kal_prompt_trace(MOD_ENG, "jx_at_adc_handler");

	mmi_frm_set_protocol_event_handler(MSG_ID_MMI_EQ_ADC_ALL_CHANNEL_IND, jx_at_update_adc_hdlr, MMI_FALSE);
    mmi_frm_send_ilm((oslModuleType)MOD_L4C, (oslMsgType)MSG_ID_MMI_EQ_START_ADC_ALL_CHANNEL_REQ, (oslParaType*)NULL, (oslPeerBuffType*)NULL);
	//StartTimer(jx_at_cmd_table[AT_ACTION_ADC].timerId, 40*1000, jx_at_stop_adc);
}

custom_rsp_type_enum jx_at_adc(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) 
	{
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_ADC].timerId, 0, jx_at_adc_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#define NVRAM_PORT_SETTING_SIZE     sizeof(port_setting_struct)

void jx_at_logmod_handler(void)
{
	kal_uint8 buffer[NVRAM_PORT_SETTING_SIZE] = {0};
    kal_bool result;

	result = nvram_external_read_data(NVRAM_EF_PORT_SETTING_LID, 1, buffer, NVRAM_PORT_SETTING_SIZE);
	if (result)
	{
		buffer[0] = 4;
		buffer[1] = 0;
		buffer[2] = 99;
		buffer[3] = 0;
		buffer[15] = 4;
		buffer[16] = 0;
		result = nvram_external_write_data(NVRAM_EF_PORT_SETTING_LID, 1, buffer, NVRAM_PORT_SETTING_SIZE);
		if (result)
		{
			AlmATPowerReset(MMI_FALSE, 3);
		}
	}
}

custom_rsp_type_enum jx_at_logmod(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) 
	{
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_LOGMOD].timerId, 0, jx_at_logmod_handler);
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

void jx_at_atmod_handler(void)
{
	kal_uint8 buffer[NVRAM_PORT_SETTING_SIZE] = {0};
    kal_bool result;

	result = nvram_external_read_data(NVRAM_EF_PORT_SETTING_LID, 1, buffer, NVRAM_PORT_SETTING_SIZE);
	if (result)
	{
		buffer[0] = 99;
		buffer[1] = 0;
		buffer[2] = 4;
		buffer[3] = 0;
		buffer[15] = 99;
		buffer[16] = 0;
		result = nvram_external_write_data(NVRAM_EF_PORT_SETTING_LID, 1, buffer, NVRAM_PORT_SETTING_SIZE);
		if (result)
		{
			AlmATPowerReset(MMI_FALSE, 3);
		}
	}
}

custom_rsp_type_enum jx_at_atmod(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) 
	{
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_ATMOD].timerId, 0, jx_at_atmod_handler);
		ret_value = CUSTOM_RSP_OK;
		break;	
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

void jx_at_reset_handler(void)
{
	AlmATPowerReset(MMI_FALSE, 1);
}

custom_rsp_type_enum jx_at_reset(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) 
	{
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_RESET].timerId, 0, jx_at_reset_handler);
		ret_value = CUSTOM_RSP_OK;
		break;	
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

void jx_at_powoff_handler(void)
{
	srv_shutdown_normal_start(0);
}

custom_rsp_type_enum jx_at_powoff(custom_cmdLine *pCmdBuf)
{
	custom_cmd_mode_enum result;
	custom_rsp_type_enum ret_value = CUSTOM_RSP_ERROR;

	result = custom_find_cmd_mode(pCmdBuf);
	switch (result) 
	{
	case CUSTOM_READ_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_ACTIVE_MODE: 
		StartTimer(jx_at_cmd_table[AT_ACTION_POWOFF].timerId, 0, jx_at_powoff_handler);
		ret_value = CUSTOM_RSP_OK;
		break;	
	case CUSTOM_SET_OR_EXECUTE_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	case CUSTOM_TEST_MODE:
		ret_value = CUSTOM_RSP_OK;
		break;
	default:
		ret_value = CUSTOM_RSP_ERROR;
		break;
	}
	return ret_value;
}

#endif /* __JX_AT_CMD_SUPPORT__ */
#endif /* __MTK_TARGET__ */

