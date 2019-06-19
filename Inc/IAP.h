/********************************************************************************
  * @file    IAP.h
  * @author  Donovan Bidlack
  * @brief   header file that contains all of the API's needed for doing in app
           programming. All in app programming will be self contained in this 
           header file. In app programming occurs when a Message 0xC is received
           over can. When this message is received in App programming algorithm
           will run.

********************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __IN_APP_PRGRM__
#define __IN_APP_PRGRM__

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include "can.h"
#include "main.h"

/* IAP DEFINES*/
#define IAP_QUEUE_BUFF_SIZE             2
#define IAP_TRUE                        0x12345678
#define IAP_TX_QUEUE_ERROR              0x05
#define IAP_RX_QUEUE_ERROR              0x04

// CAN ID / Arbitration Field
#define CAN_IAP_UPDATE_FIRMWARE         0x600
#define CAN_IAP_CRC                     0x601

// CAN DLC Field Send
#define IAP_CRC_RESPONSE                0x02

// CAN DLC Field Receive
#define IAP_PROGRAM_START               0x05
#define IAP_PROGRAMM_END                0xCC
#define IAP_SEND_STATUS                 0x00
#define IAP_LOAD_NEW_PROGRAM            0x02
#define IAP_WRITE_TO_FLASH              0x08
#define IAP_CRC_SUCCEEDED               0x03
#define IAP_CRC_FAILED                  0x07
#define IAP_LAST_FRAME                  0x04

// CAN Data Field Send
#define IAP_ALL_GOOD                    0x0
#define IAP_FAIL_READ                   0x1
#define IAP_WRITE_BUSY                  0x10
#define IAP_WRITE_SUCCEEDED             0x11
#define IAP_WRITE_FAILED                0x21
#define IAP_ERASE_FAILED                0x22
#define IAP_READY                       0xAA

// CAN Data Field Receive
#define IAP_STM_BOOTLOADER              0xAB
#define IAP_RESET_MARKERS               0xBB

// Flash Memory
#define IAP_APPLICATION_ADDRESS         (uint32_t)0x08008000

// STM32L432KC Specific
#define FLASH_START_ADDRESS             0x08000000
#define FLASH_PAGE_NBPERBANK            256
#define FLASH_BANK_NUMBER               2
#define PAGE_ERASE_SUCCESS              0xFFFFFFFF
#define IAP_FLASH_VAR_START_LOCATION    0x0803E000
#define IAP_START_IAP_PROCESS           0x0803E000
#define IAP_IS_PROGRAMMED               0x0803E004
#define IAP_FLASHED_PROGRAM_LOCATION    0x0803E008
#define IAP_STM_BOOTLOADER_LOCATION     0x1FFF0000
#define IAP_FRAMES_PER_PAGE             250  // 2000 bytes per page / 8 bytes per CAN frame

/* IAP Types -----------------------------------------------------------------*/
typedef  void (*pFunction)( void );

/* Function Prototypes  ------------------------------------------------------*/

/**********************************************
  Name: IAP_Status_Check
  Description: checks flags that are saved in 
        flash memory to see if there is an 
        updated firmware already installed or 
        if the IAP needs to be performed.
**********************************************/
void IAP_Status_Check( void );

/**********************************************
  Name: IAP_init
  Description: initialized the IAP_handle for
        In app programming. uses a rtos queue
        to signal the inapp programming that 
        is desired
**********************************************/
HAL_StatusTypeDef IAP_init( CAN_HandleTypeDef *hcan );

/**********************************************
  Name: IAP_Start_STM_Bootloader
  Description: Jumps to STM Bootloader in memory.
**********************************************/
void IAP_Start_STM_Bootloader( void );

/**********************************************
  Name: IAP_Route_Messages
  Description: function that is called when a 
        CAN IAP id is sent which will take the
        CAN frame and write it to memory. The 
        function handles routing for the other
        IAP functions.
**********************************************/
HAL_StatusTypeDef IAP_Route_Messages( CAN_RxHeaderTypeDef *pHeader, uint8_t RxMessage[] );

/**********************************************
  Name: IAP_Start
  Description: initialized the IAP_handle for
        In app programming. uses a rtos queue
        to signal the inapp programming that 
        is desired
**********************************************/
HAL_StatusTypeDef IAP_Start( void );

/**********************************************
  Name: IAP_Complete_Programming
  Description: After CAN messages have completed
        (the IAP_PROGRAMM_END payload was sent
        over the CAN) this method is called to 
        update the program pointer to the new
        program location and finalizes the 
        programming.
**********************************************/
HAL_StatusTypeDef IAP_Complete_Programming( void );

/**********************************************
  Name: IAP_Calculate_CRC_for_Memory_Frame
  Description: computes the CRC of a CAN frame
            after it has been written to memory
            (computes it from read memory).
**********************************************/
void IAP_Calculate_CRC_for_Memory_Frame( uint32_t Address );

/**********************************************
  Name: IAP_Calculate_CRC16
  Description: While program is transfering
        this function is called to update the 
        CRC to assure that the program that was 
        transfered is valid. 

        Uses CRC16_CCITT_ZERO (XModem)
**********************************************/
uint16_t IAP_Calculate_CRC16( uint16_t crc, uint8_t data );

/**********************************************
  Name: IAP_WriteFrameToFlash
  Description: Receives CAN message and writes
        CAN payload to flash memory in temporary
        location.
**********************************************/
HAL_StatusTypeDef IAP_WriteFrameToFlash( uint32_t destination, uint32_t *p_source, uint32_t *p_source2 );

/**********************************************
  Name: IAP_Erase_Flash_Memory
  Description: erases user memory from start for NbrOfPages
        length, pausing all other
        tasks to ensure data integrity.
**********************************************/
HAL_StatusTypeDef IAP_Erase_Flash_Memory( uint32_t start, uint8_t NbrOfPages );

/**********************************************
  Name: IAP_Reset_IAP_Markers
  Description: resets the IAP Markers that tell
        the IAP protocol if there is an IAP 
        program to jump to.
**********************************************/
HAL_StatusTypeDef IAP_Reset_IAP_Markers( void );

/**********************************************
  Name: IAP_CAN_Send
  Description: Sends one CAN frame
**********************************************/
void IAP_CAN_Send( uint16_t standardID, uint8_t ide, uint8_t payload[8], uint8_t dlc );

#endif /* __IN_APP_PRGRM__ */