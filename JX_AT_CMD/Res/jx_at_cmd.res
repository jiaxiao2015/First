#include "MMI_features.h"
#include "custresdef.h"

/* Need this line to tell parser that XML start, must after all #include. */
<?xml version="1.0" encoding="UTF-8"?>
#ifdef __JX_AT_CMD_SUPPORT__
/* APP tag, include your app name defined in MMIDataType.h */
<APP id="APP_JX_AT_CMD">

    /* When you use any ID of other module, you need to add
       that header file here, so that Resgen can find the ID */
    <!--Include Area-->
    <INCLUDE file="GlobalResDef.h"/>
    <INCLUDE file="mmi_rp_all_defs.h"/>

    <SCREEN id="GRP_ID_JX_KEYPAD_TEST"/>
    <SCREEN id="SCR_ID_JX_KEYPAD_TEST"/>

    /***************************************** Timer ****************************************/
	<TIMER id="JX_AT_CMD_TIMER_VERNO"/>
	<TIMER id="JX_AT_CMD_TIMER_IMEI"/>
	<TIMER id="JX_AT_CMD_TIMER_VIBR"/>
	<TIMER id="JX_AT_CMD_TIMER_CALI"/>
	<TIMER id="JX_AT_CMD_TIMER_SPK"/>

	<TIMER id="JX_AT_CMD_TIMER_LIGS"/>
	<TIMER id="JX_AT_CMD_TIMER_GSENS"/>
	<TIMER id="JX_AT_CMD_TIMER_SIM"/>
	<TIMER id="JX_AT_CMD_TIMER_WIFI"/>
	<TIMER id="JX_AT_CMD_TIMER_MIC"/>
	
    <TIMER id="JX_AT_CMD_TIMER_GPS_READ"/>
    <TIMER id="JX_AT_CMD_TIMER_GPS_OFF"/>
    <TIMER id="JX_AT_CMD_TIMER_KEYP"/>
    <TIMER id="JX_AT_CMD_TIMER_HEART_READ"/>
    <TIMER id="JX_AT_CMD_TIMER_HEART_OFF"/>
    <TIMER id="JX_AT_CMD_TIMER_LED"/>
    <TIMER id="JX_AT_CMD_TIMER_ADC"/>
    
    <TIMER id="JX_AT_CMD_TIMER_LOGMOD"/>
    <TIMER id="JX_AT_CMD_TIMER_ATMOD"/>
    <TIMER id="JX_AT_CMD_TIMER_RESET"/>
    <TIMER id="JX_AT_CMD_TIMER_POWOFF"/>

</APP>
#endif /* __JX_AT_CMD_SUPPORT__ */
