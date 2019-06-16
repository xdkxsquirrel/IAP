## HeadAutomatedTest.py
 # Author: Donovan Bidlack
 # Origin Date: 3/01/2019
 #
 # This program if for test automation of the In Application Programming
 # 
 # The Komodo CAN Solo is the CAN analyzer that is required for this software.
 # Written for Python 2.7

import time
import Komodo
import binascii
import sys
from array import array

with open('YOURFILEHERE.bin', 'rb') as f:
    program = binascii.hexlify(f.read())

def CRC16_Calculate(crc, data):
    crc ^= data << 8
    for j in range(0,8):
        if (crc & 0x8000) > 0:
           crc =(crc << 1) ^ 0x1021
        else:
           crc = crc << 1
    return crc & 0xFFFF

# Constants from IN_APP_PRGRM.h
IAP_FRAMES_PER_PAGE = 250
CAN_IAP_UPDATE_FIRMWARE = 0x600
CAN_IAP_CRC = 0x601

IAP_PROGRAM_START       = 0x05
IAP_PROGRAMM_END        = 0xCC
IAP_SEND_STATUS         = 0x00
IAP_LOAD_NEW_PROGRAM    = 0x02
IAP_WRITE_TO_FLASH      = 0x08
IAP_CRC_SUCCEEDED       = 0x03
IAP_CRC_FAILED          = 0x07
IAP_ERASE_FAILED        = 0x22
IAP_LAST_FRAME          = 0x04

# Variables used in IN_APP_PRGRM.c
Program_CRC = 0
Address_in_Page = 0
IAP_handle_iteration = 0

# Variables used in IAPAutomatedTest.py
komodoReset = 0
komodo_port = Komodo.connect()
sleeptime = .1
longsleeptime = 2

###############################################################################
######################## PROGRAM CODE STARTS HERE #############################
###############################################################################

# Send IAP_PROGRAM_START to run IAP_Start() 
print 'Sending IAP_PROGRAM_START.....'
response = Komodo.request(komodo_port, CAN_IAP_UPDATE_FIRMWARE, IAP_PROGRAM_START, 1, array('B', [3, 3, 3, 3, 3, 3, 3]), CAN_IAP_UPDATE_FIRMWARE)
if response[0][0] + response[0][1]== format(IAP_ERASE_FAILED, '04X'): 
    print response[0][0] + response[0][1], format(IAP_ERASE_FAILED, '04X')
    print '!!!!!!!!! Memory Erase Failed !!!!!!!!'
    Komodo.close(komodo_port)
    sys.exit()
print 'IAP_PROGRAM_START Successful'
time.sleep(longsleeptime)
print 'Send Page #', IAP_handle_iteration
while((IAP_handle_iteration + Address_in_Page) < len(program)/16):
    # Reset the Komodo before it sends ~60 messages in a row or it will freeze
    if komodoReset > 25:
        Komodo.close(komodo_port)
        time.sleep(sleeptime)
        komodo_port = Komodo.connect()
        komodoReset = 0
    else: 
        komodoReset += 1
    
    # Initialize the frame of program we are going to send over CAN
    tempA = array('B', [int(program[0+((IAP_handle_iteration + Address_in_Page)*16)]  +\
                        program[1+((IAP_handle_iteration + Address_in_Page)*16)],16),\
                        int(program[2+((IAP_handle_iteration + Address_in_Page)*16)]  +\
                        program[3+((IAP_handle_iteration + Address_in_Page)*16)],16),\
                        int(program[4+((IAP_handle_iteration + Address_in_Page)*16)]  +\
                        program[5+((IAP_handle_iteration + Address_in_Page)*16)],16),\
                        int(program[6+((IAP_handle_iteration + Address_in_Page)*16)]  +\
                        program[7+((IAP_handle_iteration + Address_in_Page)*16)],16),\
                        int(program[8+((IAP_handle_iteration + Address_in_Page)*16)]  +\
                        program[9+((IAP_handle_iteration + Address_in_Page)*16)],16),\
                        int(program[10+((IAP_handle_iteration + Address_in_Page)*16)] +\
                        program[11+((IAP_handle_iteration + Address_in_Page)*16)],16),\
                        int(program[12+((IAP_handle_iteration + Address_in_Page)*16)] +\
                        program[13+((IAP_handle_iteration + Address_in_Page)*16)],16),\
                        int(program[14+((IAP_handle_iteration + Address_in_Page)*16)] +\
                        program[15+((IAP_handle_iteration + Address_in_Page)*16)],16)])
    
    Program_CRC = CRC16_Calculate(Program_CRC, tempA[0])
    Program_CRC = CRC16_Calculate(Program_CRC, tempA[1]) 
    Program_CRC = CRC16_Calculate(Program_CRC, tempA[2])
    Program_CRC = CRC16_Calculate(Program_CRC, tempA[3])
    Program_CRC = CRC16_Calculate(Program_CRC, tempA[4]) 
    Program_CRC = CRC16_Calculate(Program_CRC, tempA[5]) 
    Program_CRC = CRC16_Calculate(Program_CRC, tempA[6])
    Program_CRC = CRC16_Calculate(Program_CRC, tempA[7])
    
    print 'Iteration:', format(IAP_handle_iteration + Address_in_Page, '04d'), ' Output: ',\
          format(tempA[0], '02X'), format(tempA[1], '02X'), format(tempA[2], '02X'),\
          format(tempA[3], '02X'), format(tempA[4], '02X'), format(tempA[5], '02X'), format(tempA[6], '02X'),\
          format(tempA[7], '02X'), ' CRC: ', format(Program_CRC, '04X')
    
    # Either just send the CAN frame, or send CAN frame then wait for CRC response   
    if( (Address_in_Page < IAP_FRAMES_PER_PAGE) and ((IAP_handle_iteration + Address_in_Page) < (len(program)/16)-1) ):
        send = Komodo.send(komodo_port, CAN_IAP_UPDATE_FIRMWARE, IAP_WRITE_TO_FLASH, tempA)        
        Address_in_Page += 1 
      
    else:
        if (IAP_handle_iteration + Address_in_Page) == (len(program)/16)-1 :
            send = Komodo.send(komodo_port, CAN_IAP_UPDATE_FIRMWARE, IAP_LAST_FRAME,\
                               array('B', [IAP_LAST_FRAME, IAP_LAST_FRAME, IAP_LAST_FRAME, IAP_LAST_FRAME]))
            time.sleep(sleeptime)
        response = Komodo.request(komodo_port, CAN_IAP_UPDATE_FIRMWARE, IAP_WRITE_TO_FLASH, 1, tempA, CAN_IAP_CRC)
        if response[0][0] + response[0][1] + response[0][3] + response[0][4] != format(Program_CRC, '04X'):
            print 'Python CRC of ', format(Program_CRC, '04X'), ' != STM CRC of ',\
                  str(response[0][0] + response[0][1] + response[0][3] + response[0][4])
            print '!!!!!!!!!!!! CRC FAILED !!!!!!!!!!!'
            send = Komodo.send(komodo_port, CAN_IAP_UPDATE_FIRMWARE, IAP_CRC_FAILED, array('B', [7, 7, 7, 7, 7, 7, 7]))
            Address_in_Page = 0
            time.sleep(longsleeptime)               
        else:
            time.sleep(sleeptime)
            send = Komodo.send(komodo_port, CAN_IAP_UPDATE_FIRMWARE, IAP_CRC_SUCCEEDED, array('B', [3, 3, 3]))
            IAP_handle_iteration += IAP_FRAMES_PER_PAGE
            print 'Python CRC = ', format(Program_CRC, '04X'), ' = ',\
                  str(response[0][0] + response[0][1] + response[0][3] + response[0][4]), ' STM CRC'
        Address_in_Page = 0
        Program_CRC = 0
        print ''
        if (IAP_handle_iteration + Address_in_Page) != (len(program)/16)-1 :
            print 'Send Page #', IAP_handle_iteration / IAP_FRAMES_PER_PAGE
    time.sleep(sleeptime)
        
# Finished Sending Program | Send IAP_LOAD_NEW_PROGRAM to run IAP_Complete_Programming()
send = Komodo.send(komodo_port, CAN_IAP_UPDATE_FIRMWARE, IAP_LOAD_NEW_PROGRAM, array('B', [IAP_PROGRAMM_END, IAP_PROGRAMM_END]))
time.sleep(longsleeptime)
Komodo.close(komodo_port)
print 'DONE'
