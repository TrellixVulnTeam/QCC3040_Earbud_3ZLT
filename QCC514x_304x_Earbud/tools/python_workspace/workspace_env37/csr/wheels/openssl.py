############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import os
import platform
import sys
from ctypes import cdll, c_int, c_void_p, c_ulong, c_ubyte, \
                   pointer, POINTER, Structure, \
                   addressof, cast


class OpensslError(RuntimeError):
    pass

class Aes128Cbc(object):
    def __init__(self):
        # Load the library only once regardless of how many times we instantiate the class
        try:
            self._lib = Aes128Cbc._lib
        except AttributeError:
            system = platform.system()
            is_32bit = sys.maxsize == (1 << 31) - 1
            if system == "Windows":
                if is_32bit:
                    lib_path = os.path.join(
                                   os.path.dirname(__file__),
                                   "openssl", "win32",
                                   "libcrypto-1_1.dll")
                else:
                    lib_path = os.path.join(
                                   os.path.dirname(__file__),
                                   "openssl", "win64",
                                   "libcrypto-1_1-x64.dll")
            elif system == "Linux":
                if is_32bit:
                    lib_path = os.path.join(
                                   os.path.dirname(__file__),
                                   "openssl", "linux32",
                                   "libcrypto.so")
                else:
                    lib_path = os.path.join(
                                   os.path.dirname(__file__),
                                   "openssl", "linux64",
                                   "libcrypto.so")
            else:
                raise OpensslError("OS %s is not supported" % system)
            Aes128Cbc._lib = cdll.LoadLibrary(lib_path)
            self._lib = Aes128Cbc._lib
            self._lib.EVP_aes_128_cbc.restype = c_void_p
            self._lib.EVP_aes_128_cbc.argtypes = None
            self._lib.EVP_CIPHER_CTX_new.restype = c_void_p
            self._lib.EVP_CIPHER_CTX_new.argtypes = None
            self._lib.EVP_CIPHER_CTX_free.restype = None
            self._lib.EVP_CIPHER_CTX_free.argtypes = [c_void_p]
            self._lib.EVP_CIPHER_CTX_set_padding.restype = c_int
            self._lib.EVP_CIPHER_CTX_set_padding.argtypes = [c_void_p, c_int]
            self._lib.EVP_EncryptInit_ex.restype = c_int
            self._lib.EVP_EncryptInit_ex.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p, c_void_p]
            self._lib.EVP_EncryptUpdate.restype = c_int
            self._lib.EVP_EncryptUpdate.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p, c_int]
            self._lib.EVP_EncryptFinal_ex.restype = c_int
            self._lib.EVP_EncryptFinal_ex.argtypes = [c_void_p, c_void_p, c_void_p]
            self._lib.EVP_DecryptInit_ex.restype = c_int
            self._lib.EVP_DecryptInit_ex.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p, c_void_p]
            self._lib.EVP_DecryptUpdate.restype = c_int
            self._lib.EVP_DecryptUpdate.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p, c_int]
            self._lib.EVP_DecryptFinal_ex.restype = c_int
            self._lib.EVP_DecryptFinal_ex.argtypes = [c_void_p, c_void_p, c_void_p]
        self._cipher = self._lib.EVP_aes_128_cbc()
        if self._cipher == 0:
            raise OpensslError("error creating cipher")
        self._iv_len_bytes = 16
        self._key_len_bytes = 16
        self._block_len_bytes = 16
    def _get_ciphertext_size_bytes(self, cleartext_size_bytes):
        tmp = self._block_len_bytes
        return ((int(cleartext_size_bytes) + (tmp - 1)) // tmp) * tmp
    def _get_cbuf(self, cbuf_len):
        return (c_ubyte * cbuf_len)()
    def _get_cbuf_from_list(self, l_bytes, cbuf_len):
        cbuf = self._get_cbuf(cbuf_len)
        for i in range(cbuf_len):
            if i < len(l_bytes):
                cbuf[i] = l_bytes[i]
            else:
                cbuf[i] = 0
        return cbuf
    def _get_list_from_cbuf(self, cbuf):
        return [b for b in cbuf]
    def encrypt(self, iv, key, cleartext):
        # Create an EVP_CIPHER_CTX variable
        ctx = self._lib.EVP_CIPHER_CTX_new()
        if ctx == 0:
            raise OpensslError("error creating cipher context")
        iv = self._get_cbuf_from_list(iv, self._iv_len_bytes)
        key = self._get_cbuf_from_list(key, self._key_len_bytes)
        cleartext = self._get_cbuf_from_list(
                    cleartext, self._get_ciphertext_size_bytes(len(cleartext)))
        ciphertext = self._get_cbuf(len(cleartext))
        length = c_int()
        if 1 != self._lib.EVP_EncryptInit_ex(ctx,
                                             self._cipher,
                                             None, # ENGINE
                                             key,
                                             iv):
            raise OpensslError("error initialising the encryption operation")
        # Disable padding
        if 1 != self._lib.EVP_CIPHER_CTX_set_padding(ctx, 0):
            raise OpensslError("error disabling padding")
        if 1 != self._lib.EVP_EncryptUpdate(ctx,
                                            ciphertext,
                                            pointer(length),
                                            cleartext,
                                            len(cleartext)):
            raise OpensslError("error encrypting")
        assert(length.value == len(cleartext))
        ptr = cast(addressof(ciphertext) + length.value, POINTER(c_ubyte))
        if 1 != self._lib.EVP_EncryptFinal_ex(ctx,
                                              ptr,
                                              pointer(length)):
            raise OpensslError("error finalising encryption")
        assert(length.value == 0)
        # Clean up
        self._lib.EVP_CIPHER_CTX_free(ctx)
        return self._get_list_from_cbuf(ciphertext)
    def decrypt(self, iv, key, ciphertext):
        # Create an EVP_CIPHER_CTX variable
        ctx = self._lib.EVP_CIPHER_CTX_new()
        if ctx == 0:
            raise OpensslError("error creating cipher context")
        iv = self._get_cbuf_from_list(iv, self._iv_len_bytes)
        key = self._get_cbuf_from_list(key, self._key_len_bytes)
        assert(self._get_ciphertext_size_bytes(len(ciphertext)) ==
               len(ciphertext))
        ciphertext = self._get_cbuf_from_list(ciphertext, len(ciphertext))
        cleartext = self._get_cbuf(len(ciphertext))
        length = c_int()
        #import pdb; pdb.set_trace()
        if 1 != self._lib.EVP_DecryptInit_ex(ctx,
                                             self._cipher,
                                             None, # ENGINE
                                             key,
                                             iv):
            raise OpensslError("error initialising the decryption operation")
        # Disable padding
        if 1 != self._lib.EVP_CIPHER_CTX_set_padding(ctx, 0):
            raise OpensslError("error disabling padding")
        if 1 != self._lib.EVP_DecryptUpdate(ctx,
                                            cleartext,
                                            pointer(length),
                                            ciphertext,
                                            len(ciphertext)):
            raise OpensslError("error decrypting")
        assert(length.value == len(ciphertext))
        ptr = cast(addressof(ciphertext) + length.value, POINTER(c_ubyte))
        if 1 != self._lib.EVP_DecryptFinal_ex(ctx,
                                              ptr,
                                              pointer(length)):
            raise OpensslError("error finalising decryption")
        assert(length.value == 0)
        # Clean up
        self._lib.EVP_CIPHER_CTX_free(ctx)
        return self._get_list_from_cbuf(cleartext)

