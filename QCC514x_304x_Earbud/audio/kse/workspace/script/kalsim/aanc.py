#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
# pylint: skip-file
# @PydevCodeAnalysisIgnore
'''
This is a simple script file that does the following:

1. Download the AANC bundle
2. Register a callback to print AANC events
3. Create the graph
4. Run the graph
5. Tear down the graph and unload the bundle

The script uses koperators to simulate the ANC HW:

                                                                          --------------------
                    ---------------------------------------------------> | External Mic Input |
                  |                                                       --------------------
  -------      -------      -----------      --------------      ---      -------------------- 
 | Input | -> | Split | -> | FIR (ANC) | -> | FIR (S-Path) | -> | + | -> | Internal Mic Input |
  -------      -------      -----------      --------------      ---      --------------------
                  |               -------------                   |
                   ------------- | FIR (P-Path | -----------------
                                  -------------
The ANC FIR is registered to receive unsolicited messages from the AANC operator
and in turn changes the gain in its path. For this simulation all filters are
set to pass-through and the gain at 1, so the AANC operator should converge on
a gain of 128 (=1).
'''

import argparse
import time
import sys

from kse.framework.library.file_util import load
from kse.kymera.kymera.generic.accmd import ACCMD_CMD_ID_MESSAGE_FROM_OPERATOR_REQ

START_TIME = 0                # Track the time for processing the simulation
ACCMD_CMD_ID_AANC_TRIGGER = 7 # Unsolicited message ID for AANC
PAYLOAD_OFFSET = 4            # Offset into the payload to start reading data
FF_FINE_GAIN_OFFSET = 3       # Offset into the data for FF fine gain
PREVIOUS_GAIN = 0             # Previous gain value

def cb_adaptive_gain(data):
    '''
    Callback to handle unsolicited messages.
    
    If the message comes from AANC print the time and gain value.

    Args:
        data (list[int]): accmd data received
    '''
    global PREVIOUS_GAIN
    cmd_id, _, payload = kymera._accmd.receive(data)

    if cmd_id == ACCMD_CMD_ID_MESSAGE_FROM_OPERATOR_REQ:
        if payload[1:3] == [0, ACCMD_CMD_ID_AANC_TRIGGER]:
            event_time = uut.timer_get_time() - START_TIME
            ff_fine_gain = payload[PAYLOAD_OFFSET + FF_FINE_GAIN_OFFSET]
            if ff_fine_gain != PREVIOUS_GAIN:
                print('%0.03f: Gain Update -> %d' % (event_time, ff_fine_gain))
                PREVIOUS_GAIN = ff_fine_gain

def run_graph(args):
    '''
    Run the AANC graph.
    
    Args:
        args (argparser.args): parsed command-line arguments
    '''
    global START_TIME
    print('downloading file %s' % (args.dkcs_file))

    id = hydra_cap_download.download(args.dkcs_file)
    print('Bundle downloaded. Id is %s' % (id))

    data = load(args.graph_file)
    graph.load(data)
    print('Graph loaded')
    
    kymera._accmd.register_receive_callback(cb_adaptive_gain)
    print('Callback registered')
    
    graph.create()
    print('Graph created')
    graph.config()
    print('Graph configured')
    graph.connect()
    print('Graph connected')
    
    graph.start()
    START_TIME = uut.timer_get_time()
    print('Graph started')

    while graph.check_active():
        time.sleep(0.5)

    print('Graph Finished!')

    graph.stop()
    graph.disconnect()
    graph.destroy()
    print('Graph destroyed')
    
    hydra_cap_download.remove(id)
    print('Bundle id %s removed' % (id))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='AANC capability startup kalsim shell script')
    parser.add_argument('dkcs_file', type=str, help='Input bundle file (dkcs file)')
    parser.add_argument('graph_file', type=str, help='Graph configuration file (cfg.json file)')
    args = parser.parse_args()
    run_graph(args)
    