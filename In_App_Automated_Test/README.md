# In App Automated Test with CAN Komodo Solo

## Softwares Purpose:

The purpose of the IAP Automated Testing program is to send a binary embedded program over the Control Area Network (CAN) to test the in app program system and to update firmware to a new version over the CAN and run on the STM32L432KC chip.

### Hardware Required for Use:

 1. Komodo CAN Solo
 2. STM32L432KC (or any microcontroller that supports CAN)
 3. MCP2562

### Software Required for Use:

 1. Python 2.7
 2. [Komodo CAN Solo .dll](komodo.dll) 
 3. [Komodo CAN Solo Functions](komodo_py.py)
 4. [Komodo CAN Solo Custom Functions](Komodo.py)
 5. IAP Software in parent folder running on the STM32L432KC

### Process to setup and run IAP Automated Test:

 1. Connect Komodo Can Solo to your computer and the CAN that is connected to the STM board you want to update.
 2. Change the name of the binary file that will be read onto the CAN from LED.bin to your file name.bin
 ![Where to change the file name](https://github.com/xdkxsquirrel/IAP/blob/master/In_App_Automated_Test/images/namechange.jpg)
 3. Run program
 4. Wait. It will print Done when completed.

### Program Diagram:
![Program Diagram](https://github.com/xdkxsquirrel/IAP/blob/master/In_App_Automated_Test/images/diagram.jpg)