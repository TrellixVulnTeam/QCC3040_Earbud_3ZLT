############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################import socket
from csr.wheels import iprint
from csr.wheels.bitsandbobs import NameSpace, StaticNameSpaceDict, \
                                    create_reverse_lookup, timeout_clock
from csr.dev.framework.connection.tcp_socket import TcpClientSocket, TcpServerSocket, \
                                    ConnectionTimeoutException, ConnectionClosedException
from csr.dev.framework.connection.connection_utils import SupportsGAIAApp
from csr.transport.remote_debug_protocol_primitives import RemoteDebugProtocol
import struct
import time
import subprocess
import platform
NotFoundExceptionType = WindowsError if platform.system() == "Windows" else FileNotFoundError

class SocketTransportException(RuntimeError):
    pass

class SocketTransportDeviceChanged(SocketTransportException):
    '''
    Raised when a response is received from a different device to that expected. If
    connection specified (or defaulted to) "primary" or "secondary" then this can be
    due to a handover and if it happens during a firmware log read it could be
    successfully handled. In that case a call to the reset_transport() method
    will reset the expected device to the current one and the connection can then
    be used again.
    '''
    pass

class SocketTransportUnroutable(SocketTransportException):
    '''
    This indicates that the requested device can not be reached. Typically this
    means that the request was destined for the secondary device but the link
    between primary and secondary is not active. This may be caused by one
    device being placed in the case. The link may recover at a later time so
    retrying the request is valid.
    '''
    pass

class SocketTransportRouteFailure(SocketTransportException):
    '''
    This is raised when a response is received from a device that is incompatible
    with the request
    e.g. The request was to the left device bur the response came from the right. 
    Or the request was intendeed for the primary but was received from the secondary.
    It always indicates an error and is not recoverable.
    '''
    pass

class SocketTransportArgsError(SocketTransportException):
    ''' The URL specifying the transport cannot be parsed correctly '''
    pass

class SocketTransportNoLink(SocketTransportException):
    '''
    This can indicate either a failure to connect to the socket of the phone
    app or a faiure of the phone app to connect to the device.
    '''
    pass

class SocketTransportReadFailed(SocketTransportException):
    '''The device reported an error during a read operation'''
    pass

class SocketTransportWriteFailed(SocketTransportException):
    '''The device reported an error during a write operation'''
    pass

class SocketTransportReceiveTimeout(SocketTransportException):
    '''
    Raised when a response has not arrived within the timeout period.
    '''
    pass

class SocketTransport(SupportsGAIAApp):
    """
    This class provides a debug transport over an IP socket based on the protocol at
    https://confluence.qualcomm.com/confluence/display/AppsFW/Remote+Debug+Protocols
    The main part is for providing a scoket client that connects to a remote server
    and provides the same memory read and write functionality as a TrB transport.
    However, there are also facilities (such as open_server) to use this class as
    a remote server with a simple handler for the command.
    """
    # From the specification - the IP port to use for the link
    DEFAULT_PORT_NUMBER = 13570
    
    # Size of the header that must be received before the length of the
    # payload can be determined
    PDU_HEADER_LEN_BYTES = 6
    
    # Size of GAIA command that tunnels the debug commands to the chip
    _GAIA_TUNNELLED_CMD_LENGTH_BYTES = 10
    
    # Size of the header plus payload other than the read data
    # in a memory read response
    _MEMORY_READ_RSP_OVERHEAD_BYTES = PDU_HEADER_LEN_BYTES + 2
    
    # Size of the header plus payload other than the write data
    # in a memory write request
    _MEMORY_WRITE_REQ_OVERHEAD_BYTES = PDU_HEADER_LEN_BYTES + 8
    
    # Overall payload overhead on returning a set of memroy read data
    PAYLOAD_TRANSPORT_OVERHEAD = max(_MEMORY_READ_RSP_OVERHEAD_BYTES,
                                     _MEMORY_WRITE_REQ_OVERHEAD_BYTES) \
                                     + _GAIA_TUNNELLED_CMD_LENGTH_BYTES
    
    def __init__(self, url, verbose=False, rx_timeout_seconds=20, route=None):
        self.p = self.initialise_protocol()
        self.tag = 123  # Not important - just increments for each pdu sent
        self.verbose = verbose
        self.rx_timeout_seconds = rx_timeout_seconds
        self.set_route(route)
        self.server = False
        # Set defaults before parsing URL
        self._ip_address = "localhost" 
        self._ip_port = self.DEFAULT_PORT_NUMBER 
        self._bt_name = ""

        self._url = url     # Just for debugging
        # Mapping from specifier in the URL to a routed_req_route_t
        self._url_route_mapping = {'P':'primary', 'S':'secondary', 'L':"left", 'R':'right'}

        args_help_str = '''Accepted socket transport syntax is any of:
        skt
        skt:<port> 
        skt:<ip addr>:<port> 
        skt:<ip addr>:
        skt:adb
        skt:adb:<bt device name>
        skt:adb:<bt device address>
        Any of these can have :P :S :L or :R appended to specify the primary, secondary, 
        left or right device of a pair (defaults to primary if nothing specified):
        skt:L
        skt:<port>:R
        skt:<ip addr>:<port> :S
        skt:<ip addr>::S
        skt:adb:L
        skt:adb:<bt device name>:R
        skt:adb:<bt device address>:S        
        '''
        args_error_string = ("Cannot parse transport parameter 'skt:{}'. " + args_help_str).format(url) 
        if url:
            if url in self._url_route_mapping:
                self.set_route(self._url_route_mapping[url])
            else:
                if len(url) > 1 and url[-2] == ':' and url[-1] in self._url_route_mapping:
                    # Parse and remove any trailing routing specification
                    self.set_route(self._url_route_mapping[url[-1]])
                    url = url[:-2]
                if url.startswith("adb"):
                    if url.startswith("adb:"):
                        self._bt_name = url[url.find(':')+1:]
                    elif url != "adb":
                        raise SocketTransportArgsError(args_error_string)
                    self._start_adb_forwarding()
                    self._start_gaia_debug_client_app()
                else:
                    if ':' in url:
                        self._ip_address, port_str = url.split(':')[:2]
                        if port_str:
                            try:
                                self._ip_port = int(port_str)
                            except ValueError:
                                raise SocketTransportArgsError(args_error_string)
                    else:
                        try:
                            self._ip_port = int(url)
                        except ValueError:
                            raise SocketTransportArgsError(args_error_string)

    def initialise_protocol(self):
        p = RemoteDebugProtocol()
        # Make all the dictionaries work in either direction name<->ID
        for enum in [   p.cmd_type,
                        p.tr_cmd_id,
                        p.debug_cmd_id,
                        p.route_cmd_id,                        
                        p.trb_status_id,
                        p.undeliverable_status,
                        p.connect_status,
                        p.connection_status,
                        p.disconnection_status,
                        p.remote_debug_device_id_t,
                        p.routed_req_route_t,
                        p.routed_resp_route_t,
                        p.routed_reason]:
            enum.update(create_reverse_lookup(enum))

        for dictionary in (p.pdu_cmd_type_discriminant, 
                        p.pdu_type_cmd_pack_fn, 
                        p.pdu_type_cmd_unpack_fn):
            dictionary.update(dict([(p.cmd_type[k],v) for k,v in dictionary.items()]))

        for dictionary in (p.control_payload_pack_discriminant, 
                            p.control_payload_unpack_discriminant):
            dictionary.update(dict([(p.tr_cmd_id[k],v) for k,v in dictionary.items()]))

        for dictionary in (p.debug_payload_pack_discriminant, 
                            p.debug_payload_unpack_discriminant):
            dictionary.update(dict([(p.debug_cmd_id[k],v) for k,v in dictionary.items()]))
        
        for dictionary in (p.route_payload_pack_discriminant, 
                            p.route_payload_unpack_discriminant):
            dictionary.update(dict([(p.route_cmd_id[k],v) for k,v in dictionary.items()]))

        return p

    def set_route(self, route):
        if route is not None and route not in self.p.routed_req_route_t and route not in self.p.routed_resp_route_t:
            raise SocketTransportArgsError("unrecognised route {}".format(route))
        self.route = route

            
    def open_server(self, server_handler_fn):
        '''
        This is the call to start the protocol as server. The server_handler_fn
        is called with each receive PDU when the poll_loop function is called.
        '''
        self.server = True
        self.server_handler_fn = server_handler_fn
        self.rx_data = bytearray()
        self.socket = TcpServerSocket(self._ip_port, self._server_rx_packet_handler, 
                                      auto_reconnect=True)
                       
    def open(self):
        '''
        This is the start for the socket transport as a client connecting to a 
        remote server.
        '''
        self.rx_data = bytearray()
        try:
            self.socket = TcpClientSocket(self._ip_port, None, ip_address=self._ip_address)
        except ConnectionTimeoutException:
            raise SocketTransportNoLink

        try:
            self.transport_version = self.get_transport_version()
        except ConnectionClosedException:
            try:
                raise SocketTransportException(
                    "{} on the phone doesn't have 'remote debug' enabled".format(
                                                    self.phone_app_display_name))
            except AttributeError:
                raise SocketTransportException(
                    "IP connection unexpectedly closed by remote server")
        self.show_transport_version(self.transport_version)
        
        self.transport_pdu_size = self.get_transport_pdu_size()
        # Set max payload size and number of packets from the transport for now
        # It will be combined with the chip limits once the connection with the
        # chip has been established
        self.max_payload_size_bytes = self.transport_pdu_size.pdu_size_bytes - \
                                          self.PAYLOAD_TRANSPORT_OVERHEAD

        if self.transport_version.transport_type == 0:
            self._connect_phone_app_to_chip()
        self.get_chip_details()
        
    def show_transport_version(self, ver):
        '''Do an iprint of the version information of the connected IP device'''
        connection_desc_str = "phone app" if ver.transport_type == 0 else "TRB forwarder"
        iprint("Connected to {} version {}.{}.{}".format(connection_desc_str,
                                        ver.major_version,
                                        ver.minor_version,
                                        ver.tertiary_version))
        
    def _connect_phone_app_to_chip(self):
        '''
        Instruct the phone app to connect to the chip
        We need to do this even if it is alread connected because it might
        be connected to the wrong device
        '''
        result = self.connect_to_device(self._bt_name)
        if result.status != "success":
            # This could turn into an exception but is better as a warning for now
            # so we can debug what is happening
            raise SocketTransportException(
                "Qualcomm remote debug app on the phone can't connect to the " +
                "BT device {}. Result = {}".format(self._bt_name, result.status))
        self.connection_info = self.get_connection_info()

    def get_chip_details(self):
        '''
        Update the max pdu size and number of packets according to the restrictions
        of the connected chip. This is the first attempt to talk to the chip firmware
        so may fail if there isn't really a connection.
        It is also used to re-establish communications after a
        SocketTransportDeviceChanged exception.
        '''
        try:
            self.chip_protocol_version = self.get_chip_protocol_version()
            self.chip_pdu_size = self.get_chip_max_pdu_size()
        except SocketTransportNoLink:
            # We can't get device properties because the phone app isn't connected
            # to a BT device.
            raise SocketTransportException("No chip response to version request")
        else:
            self.max_payload_size_bytes = min(self.transport_pdu_size.pdu_size_bytes, 
                                          self.chip_pdu_size.pdu_size_bytes) - \
                                          self.PAYLOAD_TRANSPORT_OVERHEAD
            self.chip_capabilities = NameSpace()
            self.chip_capabilities.routing = self.chip_protocol_version.capabilities & \
                                    self.p.capabilities_bitmap_mask["supports_routing"]

            if self.route and not self.chip_capabilities.routing:
                raise SocketTransportArgsError("Device does not support routing "
                                    "(requested by transport option %s)" % self._url)
            self.connected = True
                
    def request(self, type_name, cmd_name, cmd_args, rsp_name, extra_payload=None):
        '''
        Send a PDU to the server and ecode the response.
        This is only used on the client side.
        This takes the type_name, cmd_name and rsp_name as strings and handles
        packing the elements into the PDU. It waits for the next receive PDU and 
        will raise an exception if it does not match the expected one. 
        It will also raise an exception if the tag value in the response does
        not match the one sent in the request.
        Otherwise it returns a decoded payload as a dictionary of field values.
        extra_paylod - used for commands that have a variable length field at the
        end of the PDU. It should be supplied to this method as a bytearray.
        '''
        sent_tag = self.send_pdu(type_name, cmd_name, cmd_args, extra_payload)
        rsp_hdr,rsp_payload = self.wait_for_response(type_name, rsp_name)
        if sent_tag != rsp_hdr.tag:
            raise SocketTransportException(
                "Mismatched tags - sent {}, received {}".format(sent_tag, rsp_hdr.tag))
        return rsp_payload
 
    
    def send_pdu(self, type_name, cmd_name, cmd_args=None, extra_payload=None, tag=None):
        '''
        Send a request PDU to the server.
        This takes the type_name and cmd_name as strings and handles
        packing the elements into the PDU. 
        extra_paylod - used for commands that have a variable length field at the
        end of the PDU. It should be supplied to this method as a bytearray.
        tag - Used by the server to respond to local commands with the same tag as the
        request. Leave it set to None on the client to use a different (incrementing)
        value for each request.
        '''
        type_id = self.p.cmd_type[type_name]
        cmd_id = self.p.pdu_cmd_type_discriminant[type_id][cmd_name]
        if cmd_args is None:
            payload = bytearray()
        else:
            pack_fn = self.p.pdu_type_cmd_pack_fn[type_id][cmd_id]
            payload = pack_fn(self.p, *cmd_args)
        if extra_payload != None:
            payload += extra_payload
        if tag is None:
            self.tag = (self.tag + 1) & 0xffff
            tag = self.tag
        if self.route and type_name != "transport_cmd":
            if self.verbose:
                iprint("Sending to {}: {} {} {} {} {}".format(self.route, type_id, cmd_id, 
                                                        len(payload), tag, list(payload)))
            if self.server:
                hdr = self.p.header_pack(self.p.cmd_type["routed_cmd"], 
                                        self.p.pdu_cmd_type_discriminant["routed_cmd"]["routed_response"], 
                                        len(payload)+4, tag)
                packed_payload = self.p.pdu_routed_response_payload_pack(
                                self.p.routed_resp_route_t[self.route], type_id, cmd_id, payload)
            else:
                hdr = self.p.header_pack(self.p.cmd_type["routed_cmd"], 
                                        self.p.pdu_cmd_type_discriminant["routed_cmd"]["routed_request"], 
                                        len(payload)+4, tag)
                packed_payload = self.p.pdu_routed_request_payload_pack(
                                self.p.routed_req_route_t[self.route], type_id, cmd_id, payload)
            self.socket.send(hdr + packed_payload)
                
        else:
            if self.verbose:
                iprint("Sending: {} {} {} {} {}".format(type_id, cmd_id, len(payload), tag, 
                                                                            list(payload)))
            self.socket.send(self.p.header_pack(type_id, cmd_id, len(payload), tag) + payload)
        return tag
    
    def wait_for_response(self, type_name, rsp_name):
        '''
        This method waits for the next receive PDU and will raise an exception if it 
        does not match the expected one (supplied as a string). 
        When the response matches it returns a tuple of the decoded header of the 
        response (as a dictionary) and the payload (as a dictionary).
        '''
        expected_type_id = self.p.cmd_type[type_name]
        expected_rsp_id = self.p.pdu_cmd_type_discriminant[expected_type_id][rsp_name]
        hdr,payload = self.recv_pdu()
        response = self._unpack(hdr.type, hdr.cmd_id, payload)
        if hdr.type == self.p.cmd_type["routed_cmd"]:
            if hdr.cmd_id == self.p.route_cmd_id["unroutable_response"]:
                raise SocketTransportUnroutable(response)
            if self.verbose:
                iprint("Rx from {}: {}".format(
                        self.p.routed_resp_route_t[response.response_routing], response))
            if hdr.cmd_id == self.p.route_cmd_id["routed_request"]:
                raise SocketTransportException("Expected Rx ID routed_response, "
                                               "received routed_request")
            rx_route = self.p.routed_resp_route_t[response.response_routing]
            if self.route and not (rx_route.startswith(self.route) or rx_route.endswith(self.route)):
                # route is "left" or "secondary", rx_route is of the form "left_primary"
                raise SocketTransportRouteFailure(
                            "Expected response from {}, got {}".format(self.route, rx_route))
            # Routing is good so remove the routing header and make the response look
            # like the conventional one
            hdr = self.p.header_tuple(response.routed_type,  response.routed_cmd_id, 
                                                                hdr.payload_length, hdr.tag)
            response = response.payload
        if (hdr.type == self.p.cmd_type["transport_cmd"] and 
                            hdr.cmd_id == self.p.tr_cmd_id["undeliverable_debug_cmd_rsp"]):
            cmd_name = self.p.pdu_cmd_type_discriminant[response.type][response.command_id]
            status_name = self.p.undeliverable_status[response.status]
            raise SocketTransportNoLink("Cmd {} undeliverable - {}".format(cmd_name, status_name))
        if hdr.type != expected_type_id or hdr.cmd_id != expected_rsp_id:
            raise SocketTransportException(
                "Expected Rx ID {}, received ID {} (pkt={})".format(expected_rsp_id, hdr.cmd_id, hdr))
        return hdr,response

    def poll_loop(self):
        '''
        A convenience for going into the blocking poll loop that will call the 
        _server_rx_packet_handler() with any received data.
        Only used by the server variant.
        '''
        self.socket.poll_loop()
         
    def _server_rx_packet_handler(self, data):
        '''
        Call back from the tcp_socket server (not used for client) that presents
        the received bytes. This method accumulates the bytes until a header can
        be decoded and then further accumulates until the full packet is present.
        It then calls the server handler function passing the decoded header and
        decoded payload. 
        '''
        self.rx_data += data
        rx_hdr,rx_payload = self._pdu_from_rx_data()
        if rx_hdr:
            decoded_payload = None
            if rx_payload:
                decoded_payload = self._unpack(rx_hdr.type, rx_hdr.cmd_id, rx_payload)
            self.server_handler_fn(rx_hdr, decoded_payload)
            
    def _unpack(self, type_id, cmd_id, payload):
        ''' Return an unpacked payload given the type and cmd ID 
        If it is a routed packet the routed payload is also decoded
        '''
        if not payload:
            return []
        try:
            unpack_fn = self.p.pdu_type_cmd_unpack_fn[type_id][cmd_id]
        except KeyError:
            raise KeyError("Type {}, cmd {}".format(type_id, cmd_id))
        pdu = unpack_fn(self.p, payload)

        if type_id == self.p.cmd_type["routed_cmd"] and (
                            cmd_id == self.p.route_cmd_id["routed_request"] or
                            cmd_id == self.p.route_cmd_id["routed_response"]):
            routed_pdu = self._unpack(pdu.routed_type,
                                        pdu.routed_cmd_id,
                                        pdu.payload)
            # Reconstruct the namedtuple with the decoded payload 
            # (we can't just change the payload field because a namedtuple is immutable)
            if cmd_id == self.p.route_cmd_id["routed_request"]: 
                pdu = self.p.pdu_routed_request_tuple(pdu.request_routing, pdu.routed_type, pdu.routed_cmd_id, routed_pdu)
            else:
                pdu = self.p.pdu_routed_response_tuple(pdu.response_routing, pdu.routed_type, pdu.routed_cmd_id, routed_pdu)
        return pdu
        
    def _pdu_from_rx_data(self):
        '''
        This checks the rx_data to see if it has a complete packet. If so it returns
        the decoded header and payload bytearray as a tuple. It removes this data from 
        the rx_data so that any remaining bytes can be decoded as part of the next PDU
        '''
        if len(self.rx_data) < self.PDU_HEADER_LEN_BYTES:
            return None, None
        rx_header = self.p.header_unpack(self.rx_data[0:self.PDU_HEADER_LEN_BYTES])
        pkt_length = self.PDU_HEADER_LEN_BYTES + rx_header.payload_length
        if len(self.rx_data) < pkt_length:
            return None, None
        rx_payload = self.rx_data[self.PDU_HEADER_LEN_BYTES : 
                                         self.PDU_HEADER_LEN_BYTES + 
                                                rx_header.payload_length]
        self.rx_data = self.rx_data[self.PDU_HEADER_LEN_BYTES + 
                                                    rx_header.payload_length:]
        if self.verbose:
            iprint("Received: {} {}".format(rx_header, list(rx_payload)))
        return rx_header, rx_payload
   
    def recv_pdu(self):
        '''
        Gets bytes from the client socket until there is a complete PDU. It returns
        the decoded header and payload bytearray as a tuple.
        '''
        expected_rx_length = 4096
        rx_header = None
        timeout_time = timeout_clock() + self.rx_timeout_seconds
        while not rx_header and timeout_clock() < timeout_time:
            self.rx_data += self.socket.recv(expected_rx_length)
            rx_header, rx_payload = self._pdu_from_rx_data()
        if not rx_header:
            raise SocketTransportReceiveTimeout()
        return rx_header, rx_payload
    
    def close(self):
        iprint("Closing SocketTransport")
        self.socket.close()

    # These methods are the main interface to the protocol for the debug client
      
    def get_transport_version(self):
        '''
        Send the PDU and decode the response to get the transport version of the
        server
        '''
        return self.request("transport_cmd", "transport_version_req", None, "transport_version_rsp")
        
    def get_transport_pdu_size(self):
        '''
        Send the PDU and decode the response to get the transport PDU size
        from the server
        '''
        return self.request("transport_cmd", "max_pdu_size_req", None, "max_pdu_size_rsp") 

    def get_connection_info(self):
        '''
        Send the PDU and decode the response to get the connection information
        from the server
        '''
        response = self.request("transport_cmd", "connection_info_req", None, "connection_info_rsp")
        return response._replace(status = self.p.connection_status[response.status], 
                                 device_address = self.unpack_strings(response.device_address)[0])
    
    def get_available_devices(self):
        '''
        Send the PDU to the server and decode the response to return a list of available devices
        '''
        response = self.request("transport_cmd", "available_devices_req", None, "available_devices_rsp")
        return response._replace(devices = self.unpack_strings(response.devices))
    
    def connect_to_device(self, device_str):
        ''' Request the server to connect to a specific device '''
        response = self.request("transport_cmd", "connect_req", (self.pack_strings([device_str]),), "connect_rsp")
        return response._replace(status = self.p.connect_status[response.status])
    
    def disconnect_device(self):
        ''' Request the server to disconnect the current device '''
        response = self.request("transport_cmd", "disconnect_req", None, "disconnect_rsp")
        return response._replace(status = self.p.disconnection_status[response.status])
    
    def get_chip_protocol_version(self):
        ''' Tunnelled command to the firmware to get the protocol version '''
        return self.request("debug_cmd", "protocol_version_req", None, "protocol_version_rsp")
    
    def get_chip_max_pdu_size(self):
        ''' Tunnelled command to the firmware to get the chip limits on pdu size '''
        return self.request("debug_cmd", "max_pdu_size_req", None, "max_pdu_size_rsp")
        
    def read(self, subsys, block, addr, length):
        '''
        Perform a memory read using the socket transport. We send a
        request PDU with the parameters and parse the result from the respose.
        '''
        rsp = self.request("debug_cmd", "memory_read_req", 
                                 (subsys, block, 4, 0, addr, length), 
                                 "memory_read_rsp")
        status_str = self.p.trb_status_id[rsp.status]
        if status_str != "no_error":
            raise SocketTransportReadFailed("transport error {}".format(status_str))
        if rsp.device_id != self.chip_protocol_version.device_id:
            raise SocketTransportDeviceChanged(
                "Received response from {} but initial connection was with {}".format(
                self.p.remote_debug_device_id_t[rsp.device_id],
                self.p.remote_debug_device_id_t[self.chip_protocol_version.device_id]))
        return list(rsp.data_bytes)
    
    def write(self, subsys, block, addr, data):
        '''
        Perform a memory write using the socket transport. We send a
        request PDU with the parameters and parse the result from the respose.
        '''
        rsp = self.request("debug_cmd", "memory_write_req", 
                                 (subsys, block, 4, 0, addr),
                                 "memory_write_rsp",
                                 extra_payload = bytearray(data)) 
        status_str = self.p.trb_status_id[rsp.status]
        if status_str != "no_error":
            raise SocketTransportReadFailed("transport error {}".format(status_str))
        if rsp.device_id != self.chip_protocol_version.device_id:
            raise SocketTransportDeviceChanged(
                "Received response from {} but initial connection was with {}".format(
                self.p.remote_debug_device_id_t[rsp.device_id],
                self.p.remote_debug_device_id_t[self.chip_protocol_version.device_id]))
    
    def reset_chip(self, wait_for_response=True):
        '''
        Send the reset command and optinally wait for the response
        '''
        if wait_for_response:
            return self.request("debug_cmd", "chip_reset_req", (0,), "chip_reset_rsp")
        else:
            self.send_pdu("debug_cmd", "chip_reset_req", (0,))

    def get_max_payload(self):
        return self.max_payload_size_bytes

    def reset_transport(self):
        '''Can be used to recover from a SocketTransportDeviceChanged exception'''
        d = self.chip_protocol_version.device_id
        self.get_chip_details()
        if d != self.chip_protocol_version.device_id:
            iprint("SocketTransport device changed from {} to {}".format(
                self.p.remote_debug_device_id_t[d],
                self.p.remote_debug_device_id_t[self.chip_protocol_version.device_id]))

    def unpack_strings(self, data_bytes):
        '''
        Used for the PDUs that have strings stored as a length byte followed by characters
        This method takes a bytearray and returns a list of strings
        '''
        string_list = []
        total_len = len(data_bytes)
        used_len = 0
        while used_len < total_len:
            s_len = data_bytes[used_len]
            used_len += 1
            string_list.append(str(data_bytes[used_len:used_len+s_len]))
            used_len += s_len
        return string_list
    
    def pack_strings(self, string_list):
        '''
        Used for the PDUs that have strings stored as a length byte followed by characters
        This method takes a list of strings and returns a bytearray
        '''
        b = bytearray()
        for s in string_list:
            s = s.encode("utf-8")
            b +=  bytearray([len(s)]) + bytearray(s)
        return b
