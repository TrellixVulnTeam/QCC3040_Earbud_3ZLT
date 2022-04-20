/****************************************************************************
Copyright (c) 2004 - 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    pbapc_private.h
    
DESCRIPTION
    PhoneBook Access Profile Client Library - private header.

*/

#ifndef    PBAPC_PRIVATE_H_
#define    PBAPC_PRIVATE_H_

#include <obex.h>

#define PBAP_REP_LOCAL  0x01
#define PBAP_REP_SIM1   0x02 

/* Delay(in msec) after which PBAP state task shall be deleted 
   after handling any messages directed to this task */
#define PBAPC_TASK_DELETE_DELAY  500

/*Single Response Mode*/
#define SRMP_WAIT 0x01

/*PBAPC Features*/
#define FEATURE_DOWNLOAD      (1 << 0) /* Download                     */
#define FEATURE_BROWSING      (1 << 1) /* Browsing                     */
#define FEATURE_DATABASE_ID   (1 << 2) /* Database Identifier          */
#define FEATURE_FOLDER_VER    (1 << 3) /* Folder Version Counters      */
#define FEATURE_VCARD_SEL     (1 << 4) /* vCard Selecting              */
#define FEATURE_MISSED_CALL   (1 << 5) /* Enhanced Missed Calls        */
#define FEATURE_UCI           (1 << 6) /* X-BT-UCI vCard Property      */
#define FEATURE_UID           (1 << 7) /* X-BT-UID vCard Property      */
#define FEATURE_CONTACT_REF   (1 << 8) /* Contact Referencing          */
#define FEATURE_CONTACT_IMG   (1 << 9) /* Default Contact Image Format excluded for PCE */

/* All the features supported by PBAP server and client */
#define FEATURE_ALL           (uint32)(FEATURE_DOWNLOAD     |\
                                 FEATURE_BROWSING     |\
                                 FEATURE_DATABASE_ID  |\
                                 FEATURE_FOLDER_VER   |\
                                 FEATURE_VCARD_SEL    |\
                                 FEATURE_MISSED_CALL  |\
                                 FEATURE_UCI          |\
                                 FEATURE_UID          |\
                                 FEATURE_CONTACT_REF)


/* Currently running PBAP Client Command */
typedef enum
{
    pbapc_com_none,
    pbapc_com_reg_sdp,
    pbapc_com_connect,
    pbapc_com_disconnect,

    pbapc_com_pull_phonebook,

    pbapc_com_set_phonebook,
    pbapc_com_pull_vcard_list,
    pbapc_com_pull_vcard

} pbapcRunningCommand;

/* Pbap connection state */
typedef enum
{
    pbapc_disconnected,
    pbapc_connecting,    
    pbapc_connected,
    pbapc_disconnecting
} pbapc_connect_state;


/* Set Phonebook state machine */
typedef enum
{
    pbapc_spb_none,
    pbapc_spb_goto_root,
    pbapc_spb_goto_parent,
    pbapc_spb_goto_sim1,
    pbapc_spb_goto_telecom,
    pbapc_spb_goto_phonebook,
    
    pbapc_spb_eol

} pbapcSetPhonebookState;


struct __PB
{
    PbapcPhoneRepository currRepos:4;
    PbapcPhoneRepository targetRepos:4;
    PbapcPhoneBook currPb:4;
    PbapcPhoneBook targetPb:4;
};

typedef struct __PB pbapcPb;

/* Internal state structures */
struct __pbapcState
{
    /* Task associated with this session */
    TaskData        task;

    /* The client Task associated with this connection */
    Task            theAppTask;
    
    /* Server Bluetooth address */
    bdaddr          bdAddr;
    
    /* Device id for this pbapc connection */
    unsigned connect_state:4;
	unsigned device_id:4;
    unsigned unused: 8;

    /* Goep Handle */
    Obex    handle;

    /* Currently running PBAPC command */
    pbapcRunningCommand currCom;

    /* phone book state */
    pbapcSetPhonebookState setPbState;

    /* Remote server supported repositories */
    uint8   srvRepos;

    /* Phone Book details */
    pbapcPb  pb;

    /*Type of connection flag*/
    bool L2CAP_conn;

    bool support_features;
};

typedef struct __pbapcState pbapcState;

void pbapcRegisterSdpRecord(pbapcState *state);
void pbapcIntHandler(Task task, MessageId id, Message message);
void pbapcGoepHandler(Task task, MessageId id, Message message);
void pbapcInitHandler( Task task, MessageId id,Message message);
void pbapcGoepConnect( pbapcState *state,
                       const bdaddr *addr,
                       uint16       *channel );
void pbapcInitConnection( pbapcState *state, const bdaddr *bdAddr );

/* Set Phone book functions */
bool pbapcPbCompleteState( pbapcState *state );
pbapcSetPhonebookState pbapcPbNextState( pbapcPb pb );
void pbapcSetPb(pbapcState *state);



#endif /* PBAPC_PRIVATE_H_ */

