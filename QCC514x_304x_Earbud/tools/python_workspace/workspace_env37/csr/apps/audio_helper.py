############################################################################
# CONFIDENTIAL
#
# %%fullcopyright(2016)
#
############################################################################
DEFAULT_CAP_TABLE = {
 # key : (cap_id, max_sources, max_sinks)
 'CAP_ID_AAC_DECODER': (0x00000018, 0x00000002, 0x00000001),
 'CAP_ID_AEC_REFERENCE_1MIC': (0x00000040, 0x0000000d, 0x0000000c),
 'CAP_ID_AEC_REFERENCE_2MIC': (0x00000041, 0x0000000d, 0x0000000c),
 'CAP_ID_AEC_REFERENCE_3MIC': (0x00000042, 0x0000000d, 0x0000000c),
 'CAP_ID_AEC_REFERENCE_4MIC': (0x00000043, 0x0000000d, 0x0000000c),
 'CAP_ID_APTX_DECODER': (0x00000019, 0x00000002, 0x00000001),
 'CAP_ID_BASIC_PASS': (0x00000001, 0x00000008, 0x00000008),
 'CAP_ID_COMPANDER': (0x00000092, 0x00000008, 0x00000008),
 'CAP_ID_CVCHF1MIC_SEND_NB': (0x0000001c, 0x00000001, 0x00000002),
 'CAP_ID_CVCHF1MIC_SEND_WB': (0x0000001e, 0x00000001, 0x00000002),
 'CAP_ID_CVCHF2MIC_SEND_NB': (0x00000020, 0x00000001, 0x00000003),
 'CAP_ID_CVCHF2MIC_SEND_WB': (0x00000021, 0x00000001, 0x00000003),
 'CAP_ID_CVCHS1MIC_SEND_NB': (0x00000023, 0x00000001, 0x00000002),
 'CAP_ID_CVCHS1MIC_SEND_WB': (0x00000024, 0x00000001, 0x00000002),
 'CAP_ID_CVCHS2MIC_BINAURAL_SEND_NB': (0x00000027, 0x00000001, 0x00000003),
 'CAP_ID_CVCHS2MIC_BINAURAL_SEND_WB': (0x00000028, 0x00000001, 0x00000003),
 'CAP_ID_CVCHS2MIC_MONO_SEND_NB': (0x00000025, 0x00000001, 0x00000003),
 'CAP_ID_CVCHS2MIC_MONO_SEND_WB': (0x00000026, 0x00000001, 0x00000003),
 'CAP_ID_CVCSPKR1MIC_SEND_NB': (0x00000029, 0x00000001, 0x00000002),
 'CAP_ID_CVCSPKR1MIC_SEND_WB': (0x0000002a, 0x00000001, 0x00000002),
 'CAP_ID_CVCSPKR2MIC_SEND_NB': (0x0000002d, 0x00000001, 0x00000003),
 'CAP_ID_CVCSPKR2MIC_SEND_WB': (0x0000002e, 0x00000001, 0x00000003),
 'CAP_ID_CVCSPKR3MIC_SEND_NB': (0x00000044, 0x00000001, 0x00000004),
 'CAP_ID_CVCSPKR3MIC_SEND_WB': (0x00000045, 0x00000001, 0x00000004),
 'CAP_ID_CVCSPKR4MIC_SEND_NB': (0x00000046, 0x00000001, 0x00000005),
 'CAP_ID_CVCSPKR4MIC_SEND_WB': (0x00000047, 0x00000001, 0x00000005),
 'CAP_ID_CVC_RECEIVE_FE': (0x0000001b, 0x00000001, 0x00000001),
 'CAP_ID_CVC_RECEIVE_NB': (0x0000001d, 0x00000001, 0x00000001),
 'CAP_ID_CVC_RECEIVE_WB': (0x0000001f, 0x00000001, 0x00000001),
 'CAP_ID_DBE': (0x0000002f, 0x00000002, 0x00000002),
 'CAP_ID_DBE_FULLBAND': (0x00000090, 0x00000002, 0x00000002),
 'CAP_ID_DBE_FULLBAND_BASSOUT': (0x00000091, 0x00000002, 0x00000002),
 'CAP_ID_IIR_RESAMPLER': (0x00000094, 0x00000008, 0x00000008),
 'CAP_ID_MIXER': (0x0000000a, 0x00000006, 0x0000000c),
 'CAP_ID_PEQ': (0x00000049, 0x00000008, 0x00000008),
 'CAP_ID_RINGTONE_GENERATOR': (0x00000037, 0x00000008, 0x00000000),
 'CAP_ID_RTP_DECODE': (0x00000098, 0x00000001, 0x00000001),
 'CAP_ID_SBC_DECODER': (0x00000016, 0x00000002, 0x00000001),
 'CAP_ID_SCO_RCV': (0x00000004, 0x00000001, 0x00000001),
 'CAP_ID_SCO_SEND': (0x00000003, 0x00000001, 0x00000001),
 'CAP_ID_SPDIF_DECODE': (0x00000036, 0x00000009, 0x0000000a),
 'CAP_ID_SPLITTER': (0x00000013, 0x00000010, 0x00000008),
 'CAP_ID_VOL_CTRL_VOL': (0x00000048, 0x00000008, 0x00000010),
 'CAP_ID_VSE': (0x0000004a, 0x00000002, 0x00000002),
 'CAP_ID_WBS_DEC': (0x00000006, 0x00000001, 0x00000001),
 'CAP_ID_WBS_ENC': (0x00000005, 0x00000001, 0x00000001),
 'CAP_ID_XOVER': (0x00000033, 0x00000010, 0x00000008)}

class AudioOperator():
    def __init__(self, aud, cap_id, num_sinks, num_sources):
        self.sinks = []
        self.sources = []
        self.id = aud.create_operator(cap_id)
        for i in range(num_sinks):
            self.sinks.append(aud.sink_from_operator_terminal(self.id, i))
        for i in range(num_sources):
            self.sources.append(aud.source_from_operator_terminal(self.id, i))

class AudioHelper():
    def __init__(self, aud, audio0=None):
       self.aud = aud
       self.op_list = []
       self.cap_table = self._get_cap_table(audio0)

    def start_audio(self):
        if self.aud.is_started() == 0:
            self.own_accmd = True
            self.aud.start()
        else:
            self.own_accmd = False

    def create_op(self, cap_id_string):
        op = AudioOperator(self.aud,
                           self.cap_table[cap_id_string][0],
                           self.cap_table[cap_id_string][2],
                           self.cap_table[cap_id_string][1])
        self.op_list.append(op)
        return op

    def send_op_message(self, op, msg_words):
        self.aud.send_operator_message(op.id, msg_words)

    def send_pcm_stream_configure(self, ep, sample_rate, clock_rate_multiplier=128):
        self.aud.stream_configure(ep, "STREAM_PCM_SYNC_RATE", sample_rate)
        self.aud.stream_configure(ep, "STREAM_PCM_MASTER_CLOCK_RATE", clock_rate_multiplier * sample_rate)
        self.aud.stream_configure(ep, "STREAM_PCM_SAMPLE_FORMAT", 1)
        self.aud.stream_configure(ep, "STREAM_PCM_SLOT_COUNT", 4)
        self.aud.stream_configure(ep, "STREAM_PCM_MASTER_MODE", 1)
        self.aud.stream_configure(ep, "STREAM_PCM_SAMPLE_RISING_EDGE_ENABLE", 1)

    def start_ops(self):
        self.aud.start_operators([op.id for op in self.op_list])

    def stop_ops(self):
        self.aud.stop_operators([op.id for op in self.op_list])

    def destroy_ops(self):
        self.aud.destroy_operators([op.id for op in self.op_list])
        del self.op_list[:]

    def send_tones(self, tone_op):
        '''
        Sends some tones to a given tone operator
        '''
        RINGTONE_TONE_SEQ=1
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x90c8, 0xa0ff, 0xb000, 0x2184, 0x3fa0, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9960, 0xa0ff, 0xb000, 0x2401, 0x3f83, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9960, 0xa0ff, 0xb000, 0x2601, 0x3f8e, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9960, 0xa0ff, 0xb000, 0x2781, 0x3f83, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9960, 0xa0ff, 0xb000, 0x2901, 0x3f83, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9960, 0xa0ff, 0xb000, 0x2a01, 0x3f83, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9960, 0xa0ff, 0xb000, 0x2b01, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9960, 0xa0ff, 0xb000, 0x2c01, 0x3f83, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9960, 0xa0ff, 0xb000, 0x2d01, 0x3f83, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x90c8, 0xa0ff, 0xb000, 0x2e04, 0x3fa0, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9190, 0xa0ff, 0xb000, 0x1c04, 0x3fa0, 0x1f84, 0x3fa0, 0x2204, 0x3fa0, 0x2404, 0x3f90, 0x8000])
        self.aud.send_operator_message(tone_op, [RINGTONE_TONE_SEQ, 0x9190, 0xa0ff, 0xb000, 0x2404, 0x3fa0, 0x2204, 0x3fa0, 0x1f84, 0x3fa0, 0x1c04, 0x3f90, 0x8000])

    def stop_audio(self):
        if self.own_accmd:
            self.aud.stop()

    def _get_cap_table(self, audio0=None):
        if audio0 is not None and hasattr(audio0.fw, "env"):
             cap_table = {
                 audio0.fw.env.enums["CAP_ID"][c.deref.id.value] : 
                     (c.deref.id.value, 
                      c.deref.max_sources.value, 
                      c.deref.max_sinks.value) 
                          for c in audio0.fw.gbl.capability_data_table}
        else:
            cap_table = DEFAULT_CAP_TABLE
         
        return cap_table
