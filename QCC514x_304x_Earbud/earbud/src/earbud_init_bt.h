/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   C:\work\src\csa\vmbi_critical\earbud\src\earbud_init_bt.h
\brief      Short description.
 
Description of how to use that module. It may contain PlantUML sequence diagrams or state machines.
 
*/

#ifndef EARBUD_SRC_EARBUD_INIT_BT_H_
#define EARBUD_SRC_EARBUD_INIT_BT_H_

#include <message.h>


/*@{*/

#define INIT_CL_CFM CL_INIT_CFM
#define INIT_READ_LOCAL_NAME_CFM CL_DM_LOCAL_BD_ADDR_CFM
#define INIT_READ_LOCAL_BD_ADDR_CFM CL_DM_LOCAL_BD_ADDR_CFM
#define INIT_PEER_PAIR_LE_CFM 0
#define INIT_PEER_FIND_ROLE_CFM 0

#ifdef USE_BDADDR_FOR_LEFT_RIGHT
bool appConfigInit(Task init_task);
bool appInitHandleReadLocalBdAddrCfm(Message message);
#endif

bool appConnectionInit(Task init_task);
void Earbud_StartBtInit(void);
bool Earbud_RegisterForBtMessages(Task init_task);

/*@}*/

#endif /* EARBUD_SRC_EARBUD_INIT_BT_H_ */
