############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import socket
import errno
from csr.wheels import iprint
from csr.wheels.polling_loop import add_poll_function, remove_poll_function, \
                                    poll_loop

class ConnectionTimeoutException(RuntimeError):
    pass

class RxTimeoutException(RuntimeError):
    pass

class ConnectionClosedException(RuntimeError):
    pass

class TcpServerSocket(object):
    """
    The tx_data_poll_fn, if supplied will be periodically called whilst 
    the socket is connected. It is provided for the client to use to
    call this object's tcp_send() method with any data it has available.
    The client should repeatedly call either this object's poll_loop() method
    or the global poll_loop() to allow this object to receive data and
    pass it to the supplied rx_data_handler function.
    The disconnect_handler parameter can pass in a function to be called
    when the remote end of the IP link disconnects.
    If auto_reconnect is True then this object will automatically start
    listening again on the socket for the remote end to reconnect to. 
    """
    def __init__(self, port, rx_data_handler, 
                 tx_data_poll_fn=None, 
                 disconnect_handler=None,
                 auto_reconnect=True, 
                 timeout=0.2,
                 verbose=True):
        self.port = port
        self.rx_data_handler = rx_data_handler            
        self.tx_data_poll_fn = tx_data_poll_fn
        self.disconnect_handler = disconnect_handler
        self.auto_reconnect = auto_reconnect
        self.timeout = timeout
        self.verbose = verbose
        
        self.connect_poll_fn_name = "port %d tcp connect" % port
        self.data_poll_fn_name = "port %d tcp data" % self.port
        
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # On Unix-ish systems, you need SO_REUSEADDR to give sane socket
        # semantics (to avoid failure to bind to the same port twice in
        # quick succession due to the TIME_WAIT state; see
        # _TCP/IP Illustrated Volume 1_, Chapter 18 "TCP Connection
        # Establishment and Termination", 18.6 "TCP State Transition
        # Diagram", p243).
        #
        # Windows has these sane semantics by default; but has reused the
        # name SO_REUSEADDR to do something completely different and
        # ridiculous that you never want.
        # <https://msdn.microsoft.com/en-us/library/windows/desktop/ms740621%28v=vs.85%29.aspx>
        #
        # Windows (only) has another socket option, SO_EXCLUSIVEADDRUSE, to
        # try to patch around the worst of its SO_REUSEADDR semantics. This
        # seems like a good thing to test for to determine whether we are
        # on a platform with a given combination of socket semantics.
        #
        # (That is: we expect this setsockopt to be executed on Unix and not
        # on Windows.)
        if not hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
            self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # Use 0.0.0.0 to allow connections from any IP address. This may
        # prompt a Windows firewall exception window to appear when you run it.
        # localhost can be used instead to just allow connections from the 
        # machine it is being run on.
        self.s.bind(("0.0.0.0", port))
        self.s.settimeout(self.timeout)
        self.open()

    def open(self):
        self.s.listen(1)
        iprint("Listening on port {}".format(self.port))
        add_poll_function(self.connect_poll_fn_name, self._poll_for_tcp_connection)
        
    def _poll_for_tcp_connection(self):
        try:
            self.conn, self.addr = self.s.accept()
            if self.conn:
                self.conn.settimeout(0.1)
        except socket.timeout:
            return
        if self.verbose:
            iprint('ACCMD TCP connection received from:' + str(self.addr))
        remove_poll_function(self.connect_poll_fn_name)
        add_poll_function(self.data_poll_fn_name, self._poll_for_data)

    def _poll_for_data(self):
        try:
            data = self.conn.recv(1024)
        except socket.timeout:
            pass
        except socket.error as e:
            if e.errno == errno.WSAECONNRESET:
                self.reconnect()
        else:
            if len(data) == 0:
                self.reconnect()
                return
            self.rx_data_handler(data)
            
        if self.tx_data_poll_fn:
            self.tx_data_poll_fn()
        
    def reconnect(self):
        # Remote end closed connection
        # This tcpserver is useless now. Discard?
        if self.verbose:
            iprint('TCP client closed connection')
        # Allow the poll loop to exit if appropriate
        remove_poll_function(self.data_poll_fn_name)
        if self.disconnect_handler:
            self.disconnect_handler()
        if self.auto_reconnect:
            self.open()

    def send(self, data_bytes):
        self.conn.sendall(data_bytes)

    def stop(self):
        try:
            remove_poll_function(self.connect_poll_fn_name)
        except KeyError:
            pass
        try:
            remove_poll_function(self.data_poll_fn_name)
        except KeyError:
            pass
        self.conn.close()
        iprint("Connection closed")

    def poll_loop(self):
        poll_loop()

class TcpClientSocket(object):
    def __init__(self, port, rx_data_handler, 
                 ip_address="localhost",
                 auto_reconnect=True,
                 timeout=0.1,
                 verbose=True):
        
        self.port = port
        self.rx_data_handler = rx_data_handler            
        self.timeout = timeout
        self.verbose = verbose
        self.ip_address = ip_address
        self.auto_reconnect = auto_reconnect
                            
        self.connect_poll_fn_name = "port %d tcp connect" % port
        self.data_poll_fn_name = "port %d tcp data" % self.port
        self.open()
                
    def open(self):
        iprint('Connecting to TCP server {}:{}'.format(self.ip_address, self.port))
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.settimeout(self.timeout)
        if not hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
            self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            self.s.connect((self.ip_address, self.port))
        except socket.timeout:
            raise ConnectionTimeoutException
        
    def close(self):
        self.s.close()
        
    def send(self, data_bytes):
        try:
            self.s.sendall(data_bytes)
        except socket.error as e:
            if e.errno == errno.WSAECONNRESET:
                iprint('TCP server connection lost')
                self.reconnect()
                self.s.sendall(data_bytes)

    def recv(self, expected_length):
        data = self.poll_for_data(expected_length)
        if data is None:
            return bytearray()
        return data
    
    def poll_for_data(self, expected_length=1024):
        try:
            data = self.s.recv(expected_length)
        except socket.timeout:
            pass
        except socket.error as e:
            if e.errno == errno.WSAECONNRESET:
                self.reconnect()
        else:
            if len(data) == 0:
                # Remote end closed connection
                # This tcpserver is useless now. Discard?
                if self.verbose:
                    iprint('TCP client closed connection')
                raise ConnectionClosedException
            return data
    
    def reconnect(self):
        if self.auto_reconnect:
            self.open()
    