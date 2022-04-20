/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Header file for setting up SDP record for TWS topology
*/

#ifndef TWS_TOPOLOGY_SDP_H_
#define TWS_TOPOLOGY_SDP_H_

#include <connection.h>
#include <bdaddr.h>

/*! \brief Register the TWS service record.

    Starts the registration of the SDP record for TWS.
    \param[in] response_task  Task to send #CL_SDP_REGISTER_CFM response message to.
 */
void TwsTopology_RegisterServiceRecord(Task response_task);

/*! \brief Handle the response from the call to ConnectionRegisterServiceRecord.

    Handle the response from the call to ConnectionRegisterServiceRecord within the
    function TwsTopology_RegisterServiceRecord.

    \param[in] task  currently unused.
    \param[in] bool status (success or failure).
    \param[in] service_handle SDP service handle.

 */
void TwsTopology_HandleSdpRegisterCfm(Task task, bool status, uint32 service_handle);


/*! \brief Handle the response from the call to ConnectionUnRegisterServiceRecord.

    Handle the response from the call to ConnectionUnRegisterServiceRecord within the
    function TwsTopology_RegisterServiceRecord. This will go on to register a new
    service record.

    \param[in] task Task to send the response from ConnectionRegisterServiceRecord.
    \param[in] bool status (success or failure).
    \param[in] service_handle SDP service handle.
 */
void TwsTopology_HandleSdpUnregisterCfm(Task task, bool status, uint32 service_handle);

/*! \brief Check if device with bd_addr supports TWS+
    \param[in] bd_addr Address of device to check
 */
void TwsTopology_SearchForTwsPlusSdp(bdaddr bd_addr);
#endif /* TWS_TOPOLOGY_SDP_H_ */
