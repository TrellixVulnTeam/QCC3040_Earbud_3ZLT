/****************************************************************************
Copyright (c) 2004 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    hfp_sdp.h
    
DESCRIPTION
    
*/

#ifndef HFP_SDP_H_
#define HFP_SDP_H_

#define HFP_1_8_VERSION_NUMBER    (0x0108)     

/* The Bitfield contained hfp->hf_supported_features holds the bitmap as 
   used for the AT+BRSF message.         
   
   These macros converts to format required for the SDP record */

/* Macro takes supported features and a feature ID, if it's supported shift it the required number of bits
   to match the SDP record */
#define BRSF_FEATURE_TO_SDP_FEATURE(features, feature, shift)   ((features & feature) >> shift)

/* Individual feature macros, resolve to 0 if not in SDP record else shift the required number of bits
   to match the SDP record if enabled */
#define HFP_NREC_FUNCTION_TO_SDP(f)         BRSF_FEATURE_TO_SDP_FEATURE(f, HFP_NREC_FUNCTION, 0)
#define HFP_THREE_WAY_CALLING_TO_SDP(f)     BRSF_FEATURE_TO_SDP_FEATURE(f, HFP_THREE_WAY_CALLING, 0)
#define HFP_CLI_PRESENTATION_TO_SDP(f)      BRSF_FEATURE_TO_SDP_FEATURE(f, HFP_CLI_PRESENTATION, 0)
#define HFP_VOICE_RECOGNITION_TO_SDP(f)     BRSF_FEATURE_TO_SDP_FEATURE(f, HFP_VOICE_RECOGNITION, 0)
#define HFP_REMOTE_VOL_CONTROL_TO_SDP(f)    BRSF_FEATURE_TO_SDP_FEATURE(f, HFP_REMOTE_VOL_CONTROL, 0)
#define HFP_ENHANCED_CALL_STATUS_TO_SDP(f)  (0) /*not included in SDP record */
#define HFP_ENHANCED_CALL_CONTROL_TO_SDP(f) (0) /*not included in SDP record */
#define HFP_CODEC_NEGOTIATION_TO_SDP(f)     BRSF_FEATURE_TO_SDP_FEATURE(f, HFP_CODEC_NEGOTIATION, 2)
#define HFP_HF_INDICATORS_TO_SDP(f)         (0) /*not included in SDP record */
#define HFP_ESCO_S4_SUPPORTED_TO_SDP(f)     (0) /*not included in SDP record */
#define HFP_EVRA_SUPPORTED_TO_SDP(f)        BRSF_FEATURE_TO_SDP_FEATURE(f, HFP_EVRA_SUPPORTED, 4)
#define HFP_EVRA_TEXT_SUPPORTED_TO_SDP(f)   BRSF_FEATURE_TO_SDP_FEATURE(f, HFP_EVRA_TEXT_SUPPORTED, 4)
   
/* Merge the supported AT+BRSF bits into an SDP type bitfield */   
#define BRSF_BITMAP_TO_SDP_BITMAP(f)        (HFP_NREC_FUNCTION_TO_SDP(f) |\
                                             HFP_THREE_WAY_CALLING_TO_SDP(f) |\
                                             HFP_CLI_PRESENTATION_TO_SDP(f) |\
                                             HFP_VOICE_RECOGNITION_TO_SDP(f) |\
                                             HFP_REMOTE_VOL_CONTROL_TO_SDP(f) |\
                                             HFP_ENHANCED_CALL_STATUS_TO_SDP(f) |\
                                             HFP_ENHANCED_CALL_CONTROL_TO_SDP(f) |\
                                             HFP_CODEC_NEGOTIATION_TO_SDP(f) |\
                                             HFP_HF_INDICATORS_TO_SDP(f) |\
                                             HFP_ESCO_S4_SUPPORTED_TO_SDP(f) |\
                                             HFP_EVRA_SUPPORTED_TO_SDP(f) |\
                                             HFP_EVRA_TEXT_SUPPORTED_TO_SDP(f) )

/****************************************************************************
NAME    
    hfpRegisterServiceRecord

DESCRIPTION
    Register the service record corresponding to the specified service

RETURNS
    void
*/
void hfpRegisterServiceRecord(hfp_service_data* service);


/****************************************************************************
NAME    
    hfpUnregisterServiceRecord

DESCRIPTION
    Unregister the service record corresponding to the specified service

RETURNS
    void
*/
void hfpUnregisterServiceRecord(hfp_service_data* service);


/****************************************************************************
NAME    
    hfpHandleSdpRegisterCfm

DESCRIPTION
    Outcome of SDP service register request.

RETURNS
    void
*/
void hfpHandleSdpRegisterCfm(const CL_SDP_REGISTER_CFM_T *cfm);


/****************************************************************************
NAME    
    handleSdpUnregisterCfm

DESCRIPTION
    Outcome of SDP service unregister request.

RETURNS
    void
*/
void handleSdpUnregisterCfm(const CL_SDP_UNREGISTER_CFM_T *cfm);


/****************************************************************************
NAME    
    hfpGetProfileServerChannel

DESCRIPTION
    Initiate a service search to get the rfcomm server channel of the 
    required service on the remote device. We need this before we can 
    initiate a service level connection.

RETURNS
    void
*/
void hfpGetProfileServerChannel(hfp_link_data* link, hfp_service_data* service, const bdaddr *bd_addr);


/****************************************************************************
NAME    
    hfpHandleServiceSearchAttributeCfm

DESCRIPTION
    Service search has completed, check it has succeeded and get the required
    attrubutes from the returned list.

RETURNS
    void
*/
void hfpHandleServiceSearchAttributeCfm(const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *cfm);


/****************************************************************************
NAME    
    hfpGetAgSupportedFeatures

DESCRIPTION
    AG does not support BRSF command so we need to perform an SDP search
    to get its supported features.

RETURNS
    void
*/
void hfpGetAgSupportedFeatures(hfp_link_data* link);



#endif /* HFP_SDP_H_ */

