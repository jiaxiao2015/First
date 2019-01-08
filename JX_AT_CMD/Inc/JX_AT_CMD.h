/*
 * JX_AT_CMD.h
 *
 */

#ifndef __JX_AT_CMD_H__
#define __JX_AT_CMD_H__
#include "ata_external.h"
#include "verno.h"

#define  JX_AT_CMD_DEBUG  1

kal_bool jx_at_handler(char *full_cmd_string);
void jx_at_mic_close_by_others();
void jx_at_stop_adc(void);

custom_rsp_type_enum jx_at_verno(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_imei(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_vibr(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_cali(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_spk(custom_cmdLine *pCmdBuf);

custom_rsp_type_enum jx_at_ligs(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_gsens(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_sim(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_wifi(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_mic(custom_cmdLine *pCmdBuf);

custom_rsp_type_enum jx_at_gps(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_keyp(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_heart(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_led(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_adc(custom_cmdLine *pCmdBuf);

custom_rsp_type_enum jx_at_logmod(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_atmod(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_reset(custom_cmdLine *pCmdBuf);
custom_rsp_type_enum jx_at_powoff(custom_cmdLine *pCmdBuf);

#if defined(__JX_LED_SUPPORT__)
extern void JX_led_status_stop_play(void);
extern void JX_led_status_play_autotest(void);
#endif /* __JX_LED_SUPPORT__ */

#endif /* __JX_AT_CMD_H__ */

