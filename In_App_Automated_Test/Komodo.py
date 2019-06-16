# Komodo.py
 # Author: Donovan Bidlack
 # Origin Date: 2/08/2019
 #
 # This program is for controlling the Komodo CAN solo over USB

from __future__ import division, with_statement, print_function
from komodo_py import *
import sys
import time

MAX_PKT_SIZE = 8
NUM_GPIOS    = 8


###############################################################################
#########      DETECTS FOR CONNECTED CAN DEVICES                       ########
###############################################################################
def connect():
    (num, ports, unique_ids) = km_find_devices_ext(16, 16)

    if num > 0:
        #print("%d ports(s) found:" % num)
        for i in range(num):
            port      = ports[i]
            unique_id = unique_ids[i]
            inuse = "(avail)"
            if (port & KM_PORT_NOT_FREE):
                inuse = "(in-use)"
                port  = port & ~KM_PORT_NOT_FREE

            #print("    port = %d   %s  (%04d-%06d)" %
            #      (port, inuse, unique_id // 1000000, unique_id % 1000000))

    if inuse != '(avail)':
        print('Port is not available')
        sys.exit(1)

    km = km_open(port)
    if km <= 0:
        print('Unable to open Komodo on port %d' % port)
        print('Error code = %d' % km)
        return 0
    
    BIT_RATE = 1000000
    power   = KM_TARGET_POWER_ON
    ret = km_acquire(km, KM_FEATURE_CAN_A_CONFIG | KM_FEATURE_CAN_A_CONTROL | KM_FEATURE_CAN_A_LISTEN)
    ret = km_can_bitrate(km, KM_CAN_CH_A, BIT_RATE)
    km_timeout(km, 1000)
    km_can_target_power(km, KM_CAN_CH_A, power)
    
    ret = km_enable(km)
    if (ret != KM_OK):
        print('Unable to enable Komodo')
    
    #print('Komodo is connected')
        
    return km


###############################################################################
#########      SEND MESSAGE ON CAN                                     ########
###############################################################################
def send(km, can_id, dlc, data):
    pkt       = km_can_packet_t()
    pkt.dlc   = dlc
    pkt.id    = can_id
    can_ch = KM_CAN_CH_A
    km_can_async_submit(km, can_ch, 0, pkt, data)
    return True

    
###############################################################################
#########     Requests data from specified CAN ID and returns the received data
###############################################################################
def request(km, can_id, dlc, response_frame_number, data_out, expected_id):
    counter = 0
    failed = False
    ofTheJedi = list()
    ret = km_disable(km)
    info      = km_can_info_t()
    data_in   = array('B', [0, 0, 0, 0, 0, 0, 0, 0])
    
    pkt       = km_can_packet_t()
    pkt.dlc   = dlc
    pkt.id    = can_id
    
    ret = km_enable(km)
    if ret != KM_OK:
        print('Unable to enable Komodo')
        return
    
    km_can_write(km, KM_CAN_CH_A, 0, pkt, data_out)
    
    for _ in range(10):
        for i in range(response_frame_number):
            ret = 0
            data_in   = array('B', [0]*len(data_out))
            while(ret == 0):
                (ret, info, pkt, data_in) = km_can_read(km, data_in)

                counter += 1
                if counter > 50:
                    ofTheJedi.insert(i, 'No response from CAN device')
                    failed = True
                    break
                current_message = ''
        
            if not failed :
                for j in range(7):
                    current_message = current_message + format(data_in[j], '02X') + ' '
                    ofTheJedi.insert(i, current_message)
        if (pkt.id != expected_id):
            print('Not our message.  ID: {} != {} (expected)', pkt.id, expected_id)
        else:
            break

    return ofTheJedi

###############################################################################
#########      Monitors CAN data that Komodo receives                ##########
###############################################################################
def monitor(km, max_events, timeout):
    count = 0
    ofTheJedi = list()
    ret = km_disable(km)
    info      = km_can_info_t()
    data   = array('B', [0]*8)
    km_timeout(km, timeout)
    ret = km_enable(km)
    if ret != KM_OK:
        print('Unable to enable Komodo')
        return
        
    while((max_events == 0) or (count < max_events)):
        (ret, info, pkt, data) = km_can_read(km, data)
        current_message = ''
        if ret < 0:
            print('error=%d' % ret)
            continue
        if ((info.status == KM_OK) and not info.events):
            current_message = '<' + format(pkt.id, '02X') + '> '
        
        if not pkt.remote_req:
            for i in range(ret):
                current_message = current_message + format(data[i], '02X') + ' '
        ofTheJedi.insert(count, current_message)
        #sys.stdout.flush()
        count += 1
        
    return ofTheJedi
            
###############################################################################
#########      CLOSE PORT                                              ########
###############################################################################
def close(port):
    # Close ports and exit
    km_close(port)
    return True
