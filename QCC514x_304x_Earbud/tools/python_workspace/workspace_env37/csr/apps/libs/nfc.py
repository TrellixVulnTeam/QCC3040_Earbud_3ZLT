############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import time
from csr.dev.env.env_helpers import var_address, var_size

SYSTEM_MESSAGE_BASE_ = 0x8000
MESSAGE_MORE_DATA  = (SYSTEM_MESSAGE_BASE_ + 33)


# Handover - carrier state
PS_INACTIVE = 0x00
PS_ACTIVE = 0x01
PS_ACTIVATING = 0x02
PS_UNKNOWN = 0x03

# Handover - carrier types
BT_EP_OOB = 20
BT_LE_OOB = 21
WFA_WSC   = 22
WFA_P2P   = 23

# EIR types
EIR_TYPE_FLAGS              = (0x01)
EIR_TYPE_INCOMPLETE_LIST_16_BIT_SC   = (0x02)
EIR_TYPE_COMPLETE_LIST_16_BIT_SC     = (0x03)
EIR_TYPE_INCOMPLETE_LIST_32_BIT_SC   = (0x04)
EIR_TYPE_COMPLETE_LIST_32_BIT_SC     = (0x05)
EIR_TYPE_INCOMPLETE_LIST_128_BIT_SC  = (0x06)
EIR_TYPE_COMPLETE_LIST_128_BIT_SC    = (0x07)
EIR_TYPE_SHORTENED_LOCAL_NAME        = (0x08)
EIR_TYPE_COMPLETE_LOCAL_NAME         = (0x09)
EIR_TYPE_CLASS_OF_DEVICE             = (0x0d)
EIR_TYPE_SIMPLE_PAIRING_HASH_C       = (0x0e)
EIR_TYPE_SIMPLE_PAIRING_RANDOMIZER_R = (0x0f)
EIR_TYPE_SECURITY_MANAGER_TK_VALUE   = (0x10)
EIR_TYPE_SECURITY_MANAGER_OOB_FLAGS  = (0x11)
EIR_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE = (0x12)

# NFC UID sizes
NO_NFCID    = 0
SINGLE_NFCID = 4
DOUBLE_NFCID = 7
TRIPLE_NFCID = 10

# example NFC UIDs
NFC_UID_EX1 = [0x00,0x11,0x22,0x33]
NFC_UID_EX2 = [0x00,0x11,0x22,0x33,0x44,0x55,0x66]
NFC_UID_EX3 = [0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99]

# example DUT BD addr and device name
DUT_BD_ADDR = [0x00,0x02,0x5b,0x00,0x08,0x15]
DUT_DEVICE_NAME = "NFC-Test1"

# example EIR data for a headset

EIR_DATA_HEADSET = [ 0x04, 0x0D,        #len=0x04 type=0x0d Class of Device
                     0x04, 0x04, 0x20,
                     0x05, 0x03,        #len=0x05 type=0x03 Complete List of 16-bit Service Class UUIDs
                     0x1e, 0x11, 0x0b, 0x11]

EIR_CLASS_OF_DEVICE = [0x04, 0x04, 0x20]
EIR_COMPLETE_LIST_16BIT_SC = [0x1E, 0x11, 0x0b, 0x11]

EIR_CLASS_OF_DEVICE_HEADSET = 0x200404

EIR_SERVICE_CLASS_NAMES_HEADSET = ["Handsfree","Audio Sink"]

CHECK_MSG_TIMEOUT = 0.1

class NfcApiLib():
    """
    Library supporting access to NFC API on P1
    """

    def __init__(self,apps,logger=None,apps_task=None):

        if logger != None:
            self.log = logger
        else:
            import logging
            self.log = logging

        self.apps0 = apps.subsystem.cores[0]
        self.apps1 = apps.subsystem.cores[1]

        self.add_enum_to_globals("nfc_vm_msg_id_enum")
        self.add_enum_to_globals("nfc_vm_status_enum")
        self.add_enum_to_globals("nfc_vm_mode_enum")
        self.add_enum_to_globals("nfc_vm_service_type_enum")
        self.add_enum_to_globals("nfc_cl_msg_id_enum")
        self.add_enum_to_globals("nfc_cl_car_enum")
        self.add_enum_to_globals("NFC_STREAM_HANDOVER_TYPE")

        self.trap_utils = self.apps1.fw.trap_utils

        if apps_task is None:
            self.apps1_task = self.trap_utils.create_task(None)
        else:
            self.apps1_task = apps_task

        self.apps1.fw.call.NfcSetRecvTask(self.apps1_task);
        time.sleep(1)

    def delete_task(self):
        self.trap_utils.delete_task(self.apps1_task)

    def add_enum_to_globals(self,name_enum):
        enum = dict(self.apps1.fw.env.enums[name_enum])
        for name, value in enum.items():
            globals()[name] = value

    def enum(self,enum_name):
        return type('Enum', (), dict(self.apps0.fw.env.enums[enum_name]))

    def eir_enc(self,eir_type,eir_data):
        eir_rec = []
        eir_rec.append(len(eir_data) + 1)
        eir_rec.append(eir_type)
        eir_rec.extend(eir_data)
        return eir_rec;

    def oob_enc(self,bd_addr, device_name):

        oob_data = []

        # add BD addr
        bd_addr1 = bd_addr[:]
        bd_addr1.reverse()
        oob_data += bd_addr1

        # add device name
        device_name = [ord(c) for c in device_name]
        eir_device_name = self.eir_enc(EIR_TYPE_COMPLETE_LOCAL_NAME,device_name)
        oob_data += eir_device_name

        # add class of device
        eir_device_class = [0x04,0x04,0x20]
        eir_device_class = self.eir_enc(EIR_TYPE_CLASS_OF_DEVICE,eir_device_class)
        oob_data += eir_device_class

        # add UUIDs
        eir_uids = [0x1e,0x11,0x0b,0x11]
        eir_uids = self.eir_enc(EIR_TYPE_COMPLETE_LIST_16_BIT_SC,eir_uids)
        oob_data += eir_uids

        # add length
        oob_len = [len(oob_data) + 1 , 0x00]
        oob_data = oob_len + oob_data

        return oob_data

    def clear_msg_list(self):
        time.sleep(0.01)
        for pending in self.trap_utils._pending_msg:
            if pending["m"]:
                self.apps1.fw.call.pfree(pending["m"])
            self.trap_utils._pending_msg.pop()

        while True:
            (msg_id,m) = self.get_core_msg(None,None,None)

            if msg_id is None:
                break
            if m:
               self.apps1.fw.call.pfree(m)


    def get_core_msg(self, id=None, task=None, timeout=None):
        """
        Read messages from the internal cache of previously-read messages and the
        firmware's cache of received messages until one is received with the
        indicated ID or the timeout is reached (a value of None makes this a
        one-shot check, and a value of 0 disables the timeout).
        Messages read from the firmware cache in the mean time are
        saved into the internal cache.
        """

        # Is there a message with this ID on the pending list?
        for i, pending in enumerate(self.trap_utils._pending_msg):
            if ((id is None or pending["id"] == id) and
                                (task is None or pending["t"] == task)):
                # Found one.  Remove it from the list and return it
                self.trap_utils._pending_msg.pop(i)
                return (pending["id"],pending["m"])

        start = time.clock()
        while True:
            next_rsp = self.trap_utils.try_get_core_msg()
            if next_rsp:
                if ((id is None or next_rsp["id"] == id) and
                                    (task is None or next_rsp["t"] == task)):
                    return (next_rsp["id"],next_rsp["m"])
                else:
                    # Restart the timer in case the message is present but we're
                    # slow churning through prior messages on the list
                    start = time.clock()
                    self.trap_utils._pending_msg.append(next_rsp)

            if timeout is None:
                return (None,None)
            elif timeout > 0 and (time.clock() - start > timeout):
                return (None,None)

    def get_msg(self,id=None,timeout=None):
        time.sleep(0.01)
        (msg_id,m) = self.get_core_msg(id,None,timeout)

        if m is not None:
            prim1 = self.trap_utils._apps1.fw.env.types["nfc_prim_msg_struct"]
            nfc_msg = self.trap_utils.build_var(prim1,m)
            self.apps1.fw.call.pfree(m)
            return (msg_id,nfc_msg.m)
        elif msg_id is not None:
            return (msg_id,None)
        else:
            return (None, None)

    def send_ctrl_register_req(self):
       self.log.info("Tx: NFC_CTRL_REGISTER_REQ")
       self.apps1.fw.call.NfcSendCtrlRegisterReqPrim();

       (msg_id,m) = self.get_msg()
       if msg_id != NFC_CTRL_READY_IND_ID:
           if msg_id is None:
              self.log.error("No message received in response")
           else:
              self.log.error("Unexpected message msg_id=%x"%(msg_id))
           return -1
       else:
           self.log.info("Rx: NFC_CTRL_READY_IND mode=%d",m.ready_ind.current_config.mode.value)
           ret_val = m.ready_ind.status.value
           (msg_id,m) = self.get_msg()
           if (msg_id) == NFC_CTRL_CARRIER_LOSS_IND_ID:
               self.log.info("Rx: NFC_CTRL_CARRIER_LOSS_IND")
           return ret_val

    def send_ctrl_reset_req(self):
        self.log.info("Tx: NFC_CTRL_RESET_REQ")
        self.apps1.fw.call.NfcSendCtrlResetReqPrim();
        (msg_id,m) = self.get_msg()
        if msg_id != NFC_CTRL_RESET_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            reset_status = m.reset_cnf.status.value
            self.log.info("Rx: NFC_CTRL_RESET_CNF reset_cnf=%d",reset_status)
            if reset_status != NFC_VM_SUCCESS:
                return -2
            else:
                return 0

    def send_ctrl_config_req(self,nfc_vm_mode,ch_service,snep_service):
        self.log.info("Tx: NFC_CTRL_CONFIG_REQ")
        self.apps1.fw.call.NfcSendCtrlConfigReqPrim(nfc_vm_mode,ch_service,snep_service)
        (msg_id,m) = self.get_msg()
        if msg_id != NFC_CTRL_CONFIG_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            config_status = m.config_cnf.status.value
            self.log.info("Rx: NFC_CTRL_CONFIG_CNF config_cnf=%d",config_status)
            if config_status != NFC_VM_SUCCESS:
                return -2
            else:
                return 0

    def send_tag_write_uid_req(self,uid):
        if len(uid) != 4 and len(uid) != 7 and len(uid) != 10:
            self.log.error("Invalid UID length %d",len(uid))
            return -3

        #allocate firmware memory for uid
        nfc_uid = self.apps1.fw.call.pnew("uint8",len(uid))

        # copy the uid to the allocated memory
        for i in range(len(uid)):
            nfc_uid[i].value = uid[i]

        self.log.info("Tx: NFC_TAG_WRITE_UID_REQ")
        self.apps1.fw.call.NfcSendTagWriteUidReqPrim(nfc_uid, len(uid));
        self.apps1.fw.call.pfree(nfc_uid)

        (msg_id,m) = self.get_msg()
        if msg_id != NFC_TAG_WRITE_UID_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
           write_status = m.write_uid_cnf.status.value
           self.log.info("Rx: NFC_TAG_WRITE_UID_CNF write_uid_cnf=%d",write_status)
           if write_status != NFC_VM_SUCCESS:
               return -2
           else:
               return 0

    def send_tag_write_ndef_req(self,ndef):
        #allocate firmware memory for ndef
        nfc_ndef = self.apps1.fw.call.pnew("uint8",len(ndef))

        # copy the uid to the allocated memory
        for i in range(len(ndef)):
            nfc_ndef[i].value = ndef[i]

        self.log.info("Tx: NFC_TAG_WRITE_NDEF")
        self.apps1.fw.call.NfcSendTagWriteNdefReqPrim(nfc_ndef, len(ndef));
        self.apps1.fw.call.pfree(nfc_ndef)

        (msg_id,m) = self.get_msg()
        if msg_id != NFC_TAG_WRITE_NDEF_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            write_status = m.write_ndef_cnf.status.value
            self.log.info("Rx: NFC_TAG_WRITE_NDEF_CNF write_ndef_cnf=%d", write_status)
            if write_status != NFC_VM_SUCCESS:
                return -2
            else:
                return 0


    def send_tag_write_ch_carriers_req(self,bd_addr,device_name):
        oob_data = self.oob_enc(bd_addr,device_name)
        carriers = self.apps1.fw.call.pnew("uint32");
        carrier_data = self.apps1.fw.call.pnew("uint8",4 + len(oob_data))

        carriers.value = var_address(carrier_data)
        carrier_data[0].value = PS_ACTIVE
        carrier_data[1].value = BT_EP_OOB
        carrier_data[2].value = len(oob_data) >> 8;
        carrier_data[3].value = len(oob_data) & 0xFF;

        for i in range(4,len(oob_data)):
            carrier_data[i].value = oob_data[i-4]

        self.apps1.fw.call.NfcSendTagWriteChCarriersReqPrim(1,var_address(carriers));
        (msg_id,m) = self.get_msg()
        if msg_id != NFC_TAG_WRITE_CH_CARRIERS_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            write_status = m.write_carriers_cnf.status.value
            self.log.info("Rx: NFC_TAG_WRITE_CH_CARRIERS_CNF wire_carriers_cnf=%d", write_status)
            if  write_status != NFC_VM_SUCCESS:
                return -2
            else:
                return 0

    def set_p2p_carrier_config(self,bd_addr,device_name):

       nfc_test_sink = self.apps1.fw.call.StreamNfcSink()

       #perform a dummy claim on the sink
       self.apps1.fw.call.SinkClaim(nfc_test_sink, 1)

       self.apps1.fw.call.MessageStreamTaskFromSink(nfc_test_sink, self.apps1_task)

       oob_data = self.oob_enc(bd_addr,device_name)

       oob_len =  len(oob_data)

       sc_prim = self.apps1.fw.call.pnew("NFC_STREAM_CONTENT")
       oob_prim = self.apps1.fw.call.pnew("uint8",oob_len)

       sc_prim_len = var_size(sc_prim) - 1
       msg_len = sc_prim_len + oob_len

       sc = self.apps1.fw.call.SinkClaim(nfc_test_sink, msg_len)
       stream_ptr = self.apps1.fw.call.SinkMap(nfc_test_sink)

       sc_prim.n_carriers.value = 1
       sc_prim.carrier_info[0].carrier_header.cps.value = 1
       sc_prim.carrier_info[0].carrier_header.carrier_tech.value = BT_EP_OOB
       sc_prim.carrier_info[0].carrier_header.data_length[0].value = 0
       sc_prim.carrier_info[0].carrier_header.data_length[1].value = oob_len

       self.apps1.fw.call.memmove(stream_ptr,var_address(sc_prim),sc_prim_len)

       for i in range(0,oob_len):
           oob_prim[i].value = oob_data[i]

       self.apps1.fw.call.memmove(stream_ptr+sc_prim_len,var_address(oob_prim),oob_len)
       self.apps1.fw.call.pfree(sc_prim)
       self.apps1.fw.call.pfree(oob_prim)

       self.apps1.fw.call.NfcFlushCHData(nfc_test_sink, msg_len, 2)

       return self.check_nfc_ch_msg_carrier_config_cnf()

    def check_nfc_ctrl_carrier_on_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CTRL_CARRIER_ON_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CTRL_CARRIER_ON_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CTRL_CARRIER_ON_IND")
            return 0

    def check_nfc_ctrl_carrier_loss_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CTRL_CARRIER_LOSS_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CTRL_CARRIER_LOSS_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CTRL_CARRIER_LOSS_IND")
            return 0

    def check_nfc_ctrl_selected_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CTRL_SELECTED_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CTRL_SELECTED_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CTRL_SELECTED_IND")
            return 0

    def check_nfc_tag_read_started_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_TAG_READ_STARTED_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_TAG_READ_STARTED_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_TAG_READ_STARTED_IND")
            return 0

    def check_nfc_ch_msg_carrier_config_cnf(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CH_MSG_CARRIER_CONFIG_CNF,CHECK_MSG_TIMEOUT)

        if msg_id !=NFC_CH_MSG_CARRIER_CONFIG_CNF:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CH_MSG_CARRIER_CONFIG_CNF")
            return 0


    def check_sink_more_data(self):
        (msg_id,m) = self.get_core_msg(None,None,None)
        if msg_id != MESSAGE_MORE_DATA:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: Sink MESSAGE_MORE_DATA")
            prim1 = self.trap_utils._apps1.fw.env.types["MessageMoreData"]
            nfc_msg = self.trap_utils.build_var(prim1,m)
            len = self.apps1.fw.call.SourceSize(nfc_msg.source.value)
            self.apps1.fw.call.SourceDrop(nfc_msg.source.value,len)
            return 0

class NfcClLib(NfcApiLib):
    """
    Library supporting access to the NFC CL on P1
    """

    def __init__(self,apps,logger=None,apps_task=None):
        NfcApiLib.__init__(self,apps,logger,apps_task)

    def nfc_cl_tag_config(self):
        self.log.info("Call: NfcClConfigReq mode=TT2")
        cl_prim = self.apps1.fw.call.pnew("nfc_cl_config_req_struct")
        cl_prim.nfcClientRecvTask.value = self.apps1_task
        cl_prim.send_carrier_on_ind.value = True
        cl_prim.send_carrier_loss_ind.value = True
        cl_prim.send_selected_ind.value = True
        cl_prim.nfc_config.mode.value = NFC_VM_TT2;
        cl_prim.nfc_config.ch_service.value = NFC_VM_NONE;
        cl_prim.nfc_config.snep_service.value = NFC_VM_LLCP_NONE;
        self.apps1.fw.call.NfcClConfigReq(var_address(cl_prim))
        self.apps1.fw.call.pfree(cl_prim)
        (msg_id,m) = self.get_msg()
        if msg_id != NFC_CL_CONFIG_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_CONFIG_CNF_IND")
            return 0

    def nfc_cl_p2p_config(self):
        self.log.info("Call: NfcClConfigReq mode=P2P")
        cl_prim = self.apps1.fw.call.pnew("nfc_cl_config_req_struct")
        cl_prim.nfcClientRecvTask.value = self.apps1_task
        cl_prim.send_carrier_on_ind.value = True
        cl_prim.send_carrier_loss_ind.value = True
        cl_prim.send_selected_ind.value = True
        cl_prim.nfc_config.mode.value = NFC_VM_P2P;
        cl_prim.nfc_config.ch_service.value = NFC_VM_LLCP_SERVER;
        cl_prim.nfc_config.snep_service.value = NFC_VM_LLCP_NONE;
        self.apps1.fw.call.NfcClConfigReq(var_address(cl_prim))
        self.apps1.fw.call.pfree(cl_prim)
        (msg_id,m) = self.get_msg()
        if msg_id != NFC_CL_CONFIG_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_CONFIG_CNF_IND")
            return 0

    def nfc_cl_none_config(self):
        self.log.info("Call: NfcClConfigReq mode=NONE")
        cl_prim = self.apps1.fw.call.pnew("nfc_cl_config_req_struct")
        cl_prim.nfcClientRecvTask.value = self.apps1_task
        cl_prim.send_carrier_on_ind.value = True
        cl_prim.send_carrier_loss_ind.value = True
        cl_prim.send_selected_ind.value = True
        cl_prim.nfc_config.mode.value = NFC_VM_NONE;
        cl_prim.nfc_config.ch_service.value = NFC_VM_NONE;
        cl_prim.nfc_config.snep_service.value = NFC_VM_NONE;
        self.apps1.fw.call.NfcClConfigReq(var_address(cl_prim))
        self.apps1.fw.call.pfree(cl_prim)
        (msg_id,m) = self.get_msg()
        if msg_id != NFC_CL_CONFIG_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_CONFIG_CNF_IND")
            return 0



    def nfc_cl_set_bd_addr(self,bdaddr):
        self.log.info("Call: NfcClEncBtCarBDAddr")

        #allocate firmware memory for bdaddr
        bdaddr_prim = self.apps1.fw.call.pnew("bdaddr")

        bdaddr_prim.nap.value = (bdaddr[0] << 8) + bdaddr[1]
        bdaddr_prim.uap.value = bdaddr[2]
        bdaddr_prim.lap.value = (bdaddr[3] << 16) + (bdaddr[4] << 8 ) + bdaddr[5]

        self.apps1.fw.call.NfcClEncBtCarBDAddr(bdaddr_prim)
        self.apps1.fw.call.pfree(bdaddr_prim)

    def nfc_cl_set_local_name(self,dname):
        self.log.info("Call: NfcClEncBtCarLocalName")

        # allocate firmware memory for device name
        dname_prim = self.apps1.fw.call.pnew("uint8",len(dname))
        for i,c in enumerate(dname):
            dname_prim[i].value = ord(c)
        self.apps1.fw.call.NfcClEncBtCarLocalName(dname_prim,len(dname))
        self.apps1.fw.call.pfree(dname_prim)

    def nfc_cl_set_class_of_device(self,class_of_device):
        self.log.info("Call: NfcClEncBtCarClassOfDevice")

        # allocate firmware memory for the EIR data
        eir_data_prim = self.apps1.fw.call.pnew("uint8",len(class_of_device))
        for i,c in enumerate(class_of_device):
            eir_data_prim[i].value = c
        self.apps1.fw.call.NfcClEncBtCarClassOfDevice(eir_data_prim)
        self.apps1.fw.call.pfree(eir_data_prim)

    def nfc_cl_set_complete_list_16bit_sc(self,complete_list_16bit_sc):
        self.log.info("Call: NfcClEncBtCarCompleteList16BitSc")

        # allocate firmware memory for the EIR data
        eir_data_prim = self.apps1.fw.call.pnew("uint8",len(complete_list_16bit_sc))
        for i,c in enumerate(complete_list_16bit_sc):
            eir_data_prim[i].value = c
        self.apps1.fw.call.NfcClEncBtCarCompleteList16BitSc(eir_data_prim,
                                                            len(complete_list_16bit_sc))
        self.apps1.fw.call.pfree(eir_data_prim)

    def nfc_cl_write_ch_carriers_req(self):

        NONE_CAR_ID = 0
        BT_CAR_ID = 1
        self.apps1.fw.call.NfcClWriteChCarriersReq(BT_CAR_ID,NONE_CAR_ID)
        (msg_id,m) = self.get_msg()
        if msg_id != NFC_CL_WRITE_CH_CARRIERS_CNF_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_WRITE_CH_CARRIERS_IND")
            return 0

    def check_nfc_cl_carrier_on_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CL_CARRIER_ON_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CL_CARRIER_ON_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_CARRIER_ON_IND")
            return 0

    def check_nfc_cl_carrier_loss_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CL_CARRIER_LOSS_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CL_CARRIER_LOSS_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_CARRIER_LOSS_IND")
            return 0

    def check_nfc_cl_selected_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CL_SELECTED_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CL_SELECTED_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_SELECTED_IND")
            return 0

    def check_nfc_cl_tag_read_started_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CL_TAG_READ_STARTED_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CL_TAG_READ_STARTED_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_TAG_READ_STARTED_IND")
            return 0


    def check_nfc_cl_handover_carrier_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CL_HANDOVER_CARRIER_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CL_HANDOVER_CARRIER_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_HANDOVER_CARRIER_IND")
            return 0

    def check_nfc_cl_handover_complete_ind(self,first_msg_only=True):
        if first_msg_only:
            (msg_id,m) = self.get_msg()
        else:
            (msg_id,m) = self.get_msg(NFC_CL_HANDOVER_COMPLETE_IND_ID,CHECK_MSG_TIMEOUT)

        if msg_id != NFC_CL_HANDOVER_COMPLETE_IND_ID:
            if msg_id is None:
                self.log.error("No message received in response")
            else:
                self.log.error("Unexpected message msg_id=%x"%(msg_id))
            return -1
        else:
            self.log.info("Rx: NFC_CL_HANDOVER_COMPLETE_IND")
            return 0




