/********************************************************************************
  * @file    IAP.c
  * @author  Donovan Bidlack
  * @brief   c file that contains all of the API's needed for doing in app
           programming. All in app programming will be self contained in this 
           c file. In app programming occurs when a Message 0x600 is received
           over CAN. When this message is received in App programming algorithm
           will run.
********************************************************************************/

#include "IAP.h"

// Global Variables
uint8_t IAP_Status;
uint16_t Program_CRC;
uint16_t Address_in_Page;
uint8_t Is_Last_Frame;
uint32_t iteration;
CAN_HandleTypeDef *CAN_Handle;

/**********************************************
  Name: IAP_Status_Check
  Description: checks flags that are saved in 
        flash memory to see if there is an 
        updated firmware already installed or 
        if the IAP needs to be performed.
**********************************************/
void IAP_Status_Check( void )
{
  pFunction JumpToApplication;
  uint32_t JumpAddress;
  uint32_t New_Program_Location;
  if( *(uint32_t*) IAP_IS_PROGRAMMED == IAP_TRUE )
  {
    // Confirm that program exists at IAP_APPLICATION_ADDRESS location
    if (((*(__IO uint32_t*)IAP_APPLICATION_ADDRESS) & 0x2FFE0000 ) == 0x20000000)
    {
      New_Program_Location = *(uint32_t*) IAP_FLASHED_PROGRAM_LOCATION;  
      // Set jump memory location for system memory
      JumpAddress = *(uint32_t*) ( New_Program_Location + 4 );
      JumpToApplication = (pFunction) JumpAddress;
      // Initialize user application's Stack Pointer
      __set_MSP( *(uint32_t*) New_Program_Location );
      // Disable Initialization
      HAL_DeInit();
      // Call the function to jump to new program location
      JumpToApplication(); 
      // Should never hit this
      Error_Handler();
    }
  }  
}

/**********************************************
  Name: IAP_init
  Description: initialized the IAP for
        In app programming.
**********************************************/
HAL_StatusTypeDef IAP_init( CAN_HandleTypeDef *hcan )
{
  CAN_Handle = hcan;
  IAP_Status = IAP_ALL_GOOD;
  iteration = 0;
  Is_Last_Frame = 0;
  Address_in_Page = 0;
  Program_CRC = 0;
  return HAL_OK;
}

/**********************************************
  Name: IAP_Start_STM_Bootloader
  Description: Jumps to STM Bootloader in memory.
**********************************************/
void IAP_Start_STM_Bootloader( void )
{
  uint32_t i=0;
  void (*SysMemBootJump)(void);
  // Set the address of the entry point to bootloader
  uint32_t BootAddr = IAP_STM_BOOTLOADER_LOCATION;
  // Disable Systick timer
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
  // Disable all peripheral clocks
  HAL_RCC_DeInit();    
  // Disable interrupts
  __disable_irq();
  // Clear pending interrupts
  for (i=0;i<5;i++)
  {
    NVIC->ICER[i]=0xFFFFFFFF;
    NVIC->ICPR[i]=0xFFFFFFFF;
  }	
  // Re-enable all interrupts
  __enable_irq(); 
  // Set up the jump to booloader address + 4 
  SysMemBootJump = (void (*)(void)) (*((uint32_t *) ((BootAddr + 4)))); 
  // Set the main stack pointer to the bootloader stack 
  __set_MSP(*(uint32_t *)BootAddr); 
  // Call the function to jump to bootloader location 
  SysMemBootJump();   
  // Should never hit this
  Error_Handler();
}

/**********************************************
  Name: IAP_Route_Messages
  Description: function that sets up the stm
        for in app programming over CAN. 
**********************************************/
HAL_StatusTypeDef IAP_Route_Messages( CAN_RxHeaderTypeDef *pHeader, uint8_t RxMessage[] )
{
  uint32_t destination, firstHalfOfDataFromCAN, secondHalfOfDataFromCAN;
  uint8_t payload[8];
  
  switch( pHeader->DLC )
  {
    case IAP_PROGRAM_START :
      if( RxMessage[0] == IAP_STM_BOOTLOADER )
      {
        IAP_Start_STM_Bootloader();
      }
      else
      {
        if( IAP_Start() != HAL_OK ) 
        {
          payload[0] = IAP_Status;
          payload[1] = payload[2] = payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
          IAP_CAN_Send(CAN_IAP_UPDATE_FIRMWARE, CAN_ID_STD, payload, 1);
        }
      }
      break;
      
    case IAP_SEND_STATUS :
      payload[0] = IAP_Status;
      payload[1] = payload[2] = payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
      IAP_CAN_Send(CAN_IAP_UPDATE_FIRMWARE, CAN_ID_STD, payload, 1);
      break;

    case IAP_WRITE_TO_FLASH :        
      destination = IAP_APPLICATION_ADDRESS + ((iteration + Address_in_Page) << 3);
      firstHalfOfDataFromCAN = (uint32_t) & RxMessage[0];
      secondHalfOfDataFromCAN = (uint32_t) & RxMessage[4];
      __disable_irq();
      IAP_WriteFrameToFlash(destination, (uint32_t*) firstHalfOfDataFromCAN, (uint32_t*) secondHalfOfDataFromCAN) ;
      __enable_irq();
      if( (Address_in_Page > IAP_FRAMES_PER_PAGE - 1) || (Is_Last_Frame == 1) )
      {
        Program_CRC = 0;
        destination = IAP_APPLICATION_ADDRESS + ((iteration) << 3);
        IAP_Calculate_CRC_for_Memory_Frame(destination);
        payload[0] = Program_CRC >> 8;
        payload[1] = Program_CRC & 0xFF;
        payload[2] = payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
        IAP_CAN_Send(CAN_IAP_CRC, CAN_ID_STD, payload, 2);
      }
      Address_in_Page += 1;
      break;
       
    case IAP_LAST_FRAME :
      if(RxMessage[0] == IAP_LAST_FRAME & RxMessage[1] == IAP_LAST_FRAME)
      {
        Is_Last_Frame = 1;
      }  
      break;
        
    case IAP_CRC_FAILED : 
      if(RxMessage[0] == IAP_CRC_FAILED & RxMessage[1] == IAP_CRC_FAILED)
      {
        uint32_t start = IAP_APPLICATION_ADDRESS + ((iteration) << 3);
        uint8_t NbrOfPages = 1;
        if( IAP_Erase_Flash_Memory(start, NbrOfPages) != HAL_OK )
        {
          payload[0] = payload[1] = payload[2] = IAP_ERASE_FAILED;
          payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
          IAP_CAN_Send( CAN_IAP_UPDATE_FIRMWARE, CAN_ID_STD, payload, 3 );
        }
        else
        {
          payload[0] = payload[1] = payload[2] = IAP_READY;
          payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
          IAP_CAN_Send( CAN_IAP_UPDATE_FIRMWARE, CAN_ID_STD, payload, 3 );
        } 
        Address_in_Page = 0;
        Is_Last_Frame = 0;
      }       
      break;

    case IAP_CRC_SUCCEEDED : 
      if(RxMessage[0] == IAP_CRC_SUCCEEDED & RxMessage[1] == IAP_CRC_SUCCEEDED)
      {
        iteration += IAP_FRAMES_PER_PAGE;
        Address_in_Page = 0;
      }       
      break;

    case IAP_LOAD_NEW_PROGRAM :
      if(RxMessage[0] == IAP_PROGRAMM_END)
      {
        IAP_Complete_Programming( );
      }
      else if(RxMessage[0] == IAP_RESET_MARKERS)
      {
        IAP_Reset_IAP_Markers();
      }
      break;
        
    default:
      break;
  }     

  IAP_Status = IAP_ALL_GOOD;
  return HAL_OK;
}

/**********************************************
  Name: IAP_Start
  Description: starts the IAP process by erasing
        user memory from IAP_APPLICATION_ADDRESS
        to end of user memory, pausing all other
        tasks to ensure data integrity, then 
        waiting for data to be sent.
**********************************************/
HAL_StatusTypeDef IAP_Start( void )
{
  uint8_t payload[8];
  
  // Erase User Memory
  uint32_t start = IAP_APPLICATION_ADDRESS;
  uint8_t NbrOfPages = ((FLASH_START_ADDRESS + FLASH_SIZE) - start) / FLASH_PAGE_SIZE;
  if( IAP_Erase_Flash_Memory(start, NbrOfPages) != HAL_OK )
  {
    payload[0] = payload[1] = payload[2] = IAP_ERASE_FAILED;
    payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
    IAP_CAN_Send( CAN_IAP_UPDATE_FIRMWARE, CAN_ID_STD, payload, 3 );
  }
  else
  {
    payload[0] = payload[1] = payload[2] = IAP_READY;
    payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
    IAP_CAN_Send( CAN_IAP_UPDATE_FIRMWARE, CAN_ID_STD, payload, 3 );
  } 
  
  // Reset Variables
  Address_in_Page = 0;
  iteration = 0;
  Program_CRC = 0;
  Is_Last_Frame = 0;
  
  return HAL_OK;  
}

/**********************************************
  Name: IAP_Complete_Programming
  Description: After CAN messages have completed
        (the IAP_PROGRAMM_END payload was sent
        over the CAN) this method is called to 
        update the program pointer to the new
        program location and finalizes the 
        programming.
**********************************************/
HAL_StatusTypeDef IAP_Complete_Programming( void )
{
 
  uint64_t Temp = IAP_TRUE;
  uint64_t Data;
  HAL_StatusTypeDef status = HAL_ERROR;  
  IAP_Status = IAP_WRITE_BUSY;
  Data = Temp << 32;
  uint8_t flashWriteLoopCounter = 0;
  while( status != HAL_OK )
  {
    if( flashWriteLoopCounter > 10 )
    {
      IAP_Status = IAP_WRITE_FAILED;
      return HAL_ERROR;
    }
    uint32_t start = IAP_FLASH_VAR_START_LOCATION;
    uint32_t NbrOfPages = 1;
    IAP_Erase_Flash_Memory( start, NbrOfPages );
     __disable_irq( );
     HAL_FLASH_Unlock( );  
    status = HAL_FLASH_Program( FLASH_TYPEPROGRAM_DOUBLEWORD, IAP_FLASH_VAR_START_LOCATION, Data );
    Data = (uint64_t) IAP_APPLICATION_ADDRESS;
    status = HAL_FLASH_Program( FLASH_TYPEPROGRAM_DOUBLEWORD, IAP_FLASHED_PROGRAM_LOCATION, Data );
    HAL_FLASH_Lock( );
    flashWriteLoopCounter ++;
    IAP_Status = IAP_WRITE_SUCCEEDED;
  }
  __enable_irq( );
         
  NVIC_SystemReset( );
  return status;
}

/**********************************************
  Name: IAP_Calculate_CRC_for_Memory_Frame
  Description: computes the CRC of a CAN frame
            after it has been written to memory
            (computes it from read memory).
**********************************************/
void IAP_Calculate_CRC_for_Memory_Frame( uint32_t Address )
{
  uint16_t i;
  for( i = 0; i < Address_in_Page + 1; i++)
  {
    uint32_t *p = (uint32_t*) (Address + (i * 8));
    Program_CRC = IAP_Calculate_CRC16(Program_CRC, (uint8_t) p[0] & 0xFF);
    Program_CRC = IAP_Calculate_CRC16(Program_CRC, (uint8_t) (p[0] >> 8) & 0xFF);
    Program_CRC = IAP_Calculate_CRC16(Program_CRC, (uint8_t) (p[0] >> 16) & 0xFF);
    Program_CRC = IAP_Calculate_CRC16(Program_CRC, (uint8_t) (p[0] >> 24) & 0xFF);
    Program_CRC = IAP_Calculate_CRC16(Program_CRC, (uint8_t) p[1] & 0xFF);
    Program_CRC = IAP_Calculate_CRC16(Program_CRC, (uint8_t) (p[1] >> 8) & 0xFF);
    Program_CRC = IAP_Calculate_CRC16(Program_CRC, (uint8_t) (p[1] >> 16) & 0xFF);
    Program_CRC = IAP_Calculate_CRC16(Program_CRC, (uint8_t) (p[1] >> 24) & 0xFF);
  }
}

/**********************************************
  Name: IAP_Calculate_CRC16
  Description: While program is transfering
        this function is called to update the 
        CRC to assure that the program that was 
        transfered is valid. 

        Uses CRC16_CCITT_ZERO (XModem)
**********************************************/
uint16_t IAP_Calculate_CRC16( uint16_t crc, uint8_t data )
{
  char j;
  crc = crc ^ (data << 8);
  for(j = 0; j < 8; j++)
  {
    if((crc & 0x8000) > 0)
    {
      crc = (crc << 1) ^ 0x1021;
    }
    else
    {
      crc = crc << 1;
    }
  }
  return crc;
}

/**********************************************
  Name: IAP_WriteFrameToFlash
  Description: Receives CAN message and writes
        CAN payload to flash memory in temporary
        location.
**********************************************/
HAL_StatusTypeDef IAP_WriteFrameToFlash( uint32_t destination, uint32_t *p_source, uint32_t *p_source2 )
{
  uint64_t Data = *p_source;
  uint64_t Data2 = *p_source2;
  HAL_StatusTypeDef status = HAL_ERROR;  
  IAP_Status = IAP_WRITE_BUSY;
  Data = ( Data2 << 32 ) | Data;
  uint8_t flashWriteLoopCounter = 0;
  while ( status != HAL_OK )
  {
    if( flashWriteLoopCounter > 10 )
    {
      IAP_Status = IAP_WRITE_FAILED;
      return HAL_ERROR;
    }
    HAL_FLASH_Unlock();    
    status = HAL_FLASH_Program( FLASH_TYPEPROGRAM_DOUBLEWORD, destination, Data ); 
    HAL_FLASH_Lock();
    flashWriteLoopCounter ++;
    IAP_Status = IAP_WRITE_SUCCEEDED;
  }
  return status;
}

/**********************************************
  Name: IAP_Erase_Flash_Memory
  Description: erases user memory from start for NbrOfPages
        length, pausing all other
        tasks to ensure data integrity.
**********************************************/
HAL_StatusTypeDef IAP_Erase_Flash_Memory( uint32_t start, uint8_t NbrOfPages )
{
  uint8_t flashEraseLoopCounter = 0;
  FLASH_EraseInitTypeDef pEraseInit;
  uint32_t PageEraseStatus = 0;
  pEraseInit.Banks = FLASH_BANK_1;
  pEraseInit.NbPages = NbrOfPages % FLASH_PAGE_NBPERBANK;
  pEraseInit.Page = FLASH_PAGE_NBPERBANK - pEraseInit.NbPages;
  pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
  HAL_FLASH_Unlock();    
  __disable_irq();
  while (PageEraseStatus != PAGE_ERASE_SUCCESS)
  {
    if( flashEraseLoopCounter > 10 )
    {
      IAP_Status = IAP_ERASE_FAILED;
      return HAL_ERROR;
    }
    HAL_FLASHEx_Erase( &pEraseInit, &PageEraseStatus );
    flashEraseLoopCounter ++;
  }
  __enable_irq();
  HAL_FLASH_Lock();
  return HAL_OK;
}
  
/**********************************************
  Name: IAP_Reset_IAP_Markers
  Description: resets the IAP Markers that tell
        the IAP protocol if there is an IAP 
        program to jump to.
**********************************************/
HAL_StatusTypeDef IAP_Reset_IAP_Markers( void )
{
  uint8_t payload[8];
  uint32_t start = IAP_FLASH_VAR_START_LOCATION;
  uint32_t NbrOfPages = ( ( FLASH_START_ADDRESS + FLASH_SIZE ) - start ) / FLASH_PAGE_SIZE;
  if( IAP_Erase_Flash_Memory(start, NbrOfPages) != HAL_OK )
  {
    payload[0] = payload[1] = payload[2] = IAP_ERASE_FAILED;
    payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
    IAP_CAN_Send( CAN_IAP_UPDATE_FIRMWARE, CAN_ID_STD, payload, 3 );
  }
  else
  {
    payload[0] = payload[1] = payload[2] = IAP_READY;
    payload[3] = payload[4] = payload[5] = payload[6] = payload[7] = 0;
    IAP_CAN_Send( CAN_IAP_UPDATE_FIRMWARE, CAN_ID_STD, payload, 3 );
  } 
  return HAL_OK;
}

/**********************************************
  Name: IAP_CAN_Send
  Description: Sends one CAN frame
**********************************************/
void IAP_CAN_Send( uint16_t standardID, uint8_t ide, uint8_t payload[8], uint8_t dlc )
{
  uint32_t TxMailbox;
  CAN_TxHeaderTypeDef   TxHeader;
  TxHeader.StdId = standardID;
  TxHeader.ExtId = 0x01;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = dlc;
  TxHeader.TransmitGlobalTime = DISABLE;
  while( HAL_CAN_GetTxMailboxesFreeLevel(CAN_Handle) == 0 )
  {
    // Waiting for Mailbox to become free.
  }
  if (HAL_CAN_AddTxMessage(CAN_Handle, &TxHeader, payload, &TxMailbox) != HAL_OK)
  {
    /* Transmission request Error */
    Error_Handler();
  }
}

