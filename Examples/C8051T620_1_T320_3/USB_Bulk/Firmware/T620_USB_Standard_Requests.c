//-----------------------------------------------------------------------------
// T620_USB_Standard_Requests.c
//-----------------------------------------------------------------------------
// Copyright 2009 Silicon Laboratories, Inc.
// http://www.silabs.com
//
// Program Description:
//
// This file includes service routine for USB standard device requests.  This
// code will most likely not need to change and is not application-specific.
//
//
// How To Test:    See Readme.txt
//
//
// Target:         C8051T620/1 or C8051T320/1/2/3
// Tool chain:     Keil / Raisonance
//                 Silicon Laboratories IDE version 3.4x
// Command Line:   See Readme.txt
// Project Name:   T620_USB_Bulk
//
//
// Release 1.4
//    -All changes by TP
//    -01 JUL 2008
//    -Updated for 'T620/1 by moving the file storage to XRAM and limiting the
//      size to 512 bytes
//
// Release 1.3.1
//    -21 FEB 2008 (TP)
//    -Updated line 497 for Keil 8.x
//
// Release 1.3
//    -All changes by GP
//    -21 NOV 2005
//    -Changed revision number to match project revision
//     No content changes to this file
//    -Modified file to fit new formatting guidelines
//    -Changed file name from usb_stdreq.c
//
// Release 1.2
//    -Initial Revision (JS)
//    -XX AUG 2003
//

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "compiler_defs.h"
#include "C8051T620_defs.h"
#include "T620_USB_Registers.h"
#include "T620_USB_Structs.h"
#include "T620_USB_Main.h"
#include "T620_USB_Descriptors.h"
#include "T620_USB_Config.h"
#include "T620_USB_Request.h"

//-----------------------------------------------------------------------------
// Extern Global Variables
//-----------------------------------------------------------------------------

extern code DESCRIPTORS gDescriptorMap;
extern DEVICE_STATUS gDeviceStatus;
extern EP_STATUS gEp0Status;
extern EP_STATUS gEp2OutStatus;
extern EP_STATUS gEp1InStatus;
extern EP0_COMMAND gEp0Command;

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

U8 bEpState;
U16 uNumBytes;
PIF_STATUS pIfStatus;

//------------------------------------------------------------------------
//  Standard Request Routines
//------------------------------------------------------------------------
//
// These functions should be called when an endpoint0 command has
// been received with a corresponding "bRequest" field.
//
// - Each routine performs all command field checking, and
//   modifies fields of the Ep0Status structure accordingly
//
// After a call to a standard request routine, the calling routine
// should check Ep0Status.bEpState to determine the required action
// (i.e., send a STALL for EP_ERROR, load the FIFO for EP_TX).
// For transmit status, the data pointer (Ep0Status.pData),
// and data length (Ep0Status.uNumBytes) are prepared before the
// standard request routine returns. The calling routine must write
// the data to the FIFO and handle all data transfer

//-----------------------------------------------------------------------------
// SetAddressRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void SetAddressRequest (void)
{
   // Index and Length fields must be zero
   // Device state must be default or addressed
   if ((gEp0Command.wIndex.U16) || (gEp0Command.wLength.U16) ||
       (gDeviceStatus.bDevState == DEV_CONFIG))
   {
      bEpState = EP_ERROR;
   }

   else
   {
      // Assign new function address
      UWRITE_BYTE(FADDR, gEp0Command.wValue.U8[LSB]);
      if (gDeviceStatus.bDevState == DEV_DEFAULT &&
      gEp0Command.wValue.U8[LSB] != 0)
      {
         gDeviceStatus.bDevState = DEV_ADDRESS;
      }
      if (gDeviceStatus.bDevState == DEV_ADDRESS &&
      gEp0Command.wValue.U8[LSB] == 0)
      {
         gDeviceStatus.bDevState = DEV_ADDRESS;
      }
      bEpState = EP_IDLE;
   }
   gEp0Status.bEpState = bEpState;
}

//-----------------------------------------------------------------------------
// SetFeatureRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void SetFeatureRequest (void)
{
   // Length field must be zero
   // Device state must be configured, or addressed with Command Index
   // field == 0
   if ((gEp0Command.wLength.U16 != 0) ||
       (gDeviceStatus.bDevState == DEV_DEFAULT) ||
       (gDeviceStatus.bDevState == DEV_ADDRESS && gEp0Command.wIndex.U16 != 0))
   {
      bEpState = EP_ERROR;
   }

   // Handle based on recipient
   switch(gEp0Command.bmRequestType & CMD_MASK_RECIP)
   {
      // Device Request - Return error as remote wakeup is not supported
      case CMD_RECIP_DEV:
         bEpState = EP_ERROR;
         break;

      // Endpoint Request
      case CMD_RECIP_EP:
         if (gEp0Command.wValue.U16 == ENDPOINT_HALT)
         {
            bEpState = HaltEndpoint(gEp0Command.wIndex.U16);
            break;
         }
         else
         {
            bEpState = EP_ERROR;
            break;
         }
      default:
         bEpState = EP_ERROR;
         break;
   }
   gEp0Status.bEpState = bEpState;
}

//-----------------------------------------------------------------------------
// ClearFeatureRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void ClearFeatureRequest (void)
{
   // Length field must be zero
   // Device state must be configured, or addressed with Command Index
   // field == 0
   if ((gEp0Command.wLength.U16 != 0) ||
       (gDeviceStatus.bDevState == DEV_DEFAULT) ||
       (gDeviceStatus.bDevState == DEV_ADDRESS && gEp0Command.wIndex.U16 != 0))
   {
      bEpState = EP_ERROR;
   }

   // Handle based on recipient
   switch(gEp0Command.bmRequestType & CMD_MASK_RECIP)
   {
      // Device Request
      case CMD_RECIP_DEV:
         // Remote wakeup not supported
         bEpState = EP_ERROR;
         break;

      // Endpoint Request
      case CMD_RECIP_EP:
         if (gEp0Command.wValue.U16 == ENDPOINT_HALT)
         {
            // Enable the selected endpoint.
            bEpState = EnableEndpoint(gEp0Command.wIndex.U16);
            break;
         }
         else
         {
           bEpState = EP_ERROR;
           break;
         }
      default:
         bEpState = EP_ERROR;
         break;
   }
   gEp0Status.bEpState = bEpState;
}

//-----------------------------------------------------------------------------
// SetConfigurationRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void SetConfigurationRequest (void)
{
   // Index and Length fields must be zero
   // Device state must be addressed or configured
   if ((gEp0Command.wIndex.U16) || (gEp0Command.wLength.U16) ||
       (gDeviceStatus.bDevState == DEV_DEFAULT))
   {
      bEpState = EP_ERROR;
   }

   else
   {
      // Make sure assigned configuration exists
      if (gEp0Command.wValue.U8[LSB] >
      gDescriptorMap.bStdDevDsc[std_bNumConfigurations])
      {
         bEpState = EP_ERROR;
      }

      // Handle zero configuration assignment
      else if  (gEp0Command.wValue.U8[LSB] == 0)
         gDeviceStatus.bDevState = DEV_ADDRESS;

      // Select the assigned configuration
      else
         bEpState = SetConfiguration(gEp0Command.wValue.U8[LSB]);
   }
   gEp0Status.bEpState = bEpState;
}


//-----------------------------------------------------------------------------
// SetInterfaceRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void SetInterfaceRequest (void)
{
   /*
   // Length field must be zero
   if ((gEp0Command.wLength.U16) || (gDeviceStatus.bDevState != DEV_CONFIG))
      bEpState = EP_ERROR;

   else
   {
      // Check that target interface exists for this configuration
      if(gEp0Command.wIndex.U16 > gDeviceStatus.bNumInterf - 1)
         bEpState = EP_ERROR;

      else
      {
         // Get pointer to interface status structure
         pIfStatus = (PIF_STATUS)&gDeviceStatus.IfStatus;

         // Check that alternate setting exists for the interface
         if (gEp0Command.wValue.U16 > pIfStatus->bNumAlts)
            bEpState = EP_ERROR;

         // Assign alternate setting
         else
         {
            pIfStatus->bCurrentAlt = gEp0Command.wValue.U16;
            bEpState = SetInterface(pIfStatus);
         }
      }
   }
   gEp0Status.bEpState = bEpState;
   */
}

//-----------------------------------------------------------------------------
// GetStatusRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void GetStatusRequest (void)
{
   // Value field must be zero; Length field must be 2
   if ((gEp0Command.wValue.U16 != 0) || (gEp0Command.wLength.U16 != 0x02) ||
       (gDeviceStatus.bDevState == DEV_DEFAULT) ||
       (gDeviceStatus.bDevState == DEV_ADDRESS && gEp0Command.wIndex.U16 != 0))
   {
      bEpState = EP_ERROR;
   }

   else
   {
      // Check for desired status (device, interface, endpoint)
      switch (gEp0Command.bmRequestType & CMD_MASK_RECIP)
      {
         // Device
         case CMD_RECIP_DEV:
            // Index must be zero for a Device status request
            if (gEp0Command.wIndex.U16 != 0)
               bEpState = EP_ERROR;
            else
            {
               // Prepare data_out for transmission
               gEp0Status.wData.U8[LSB] = 0;
               gEp0Status.wData.U8[MSB] = gDeviceStatus.bRemoteWakeupStatus;
               gEp0Status.wData.U8[MSB] |= gDeviceStatus.bSelfPoweredStatus;
            }
            break;

         // Interface
         case CMD_RECIP_IF:
            // Prepare data_out for transmission
            gEp0Status.wData.U16 = 0;
            break;

         // Endpoint
         case CMD_RECIP_EP:
            // Prepare data_out for transmission
            gEp0Status.wData.U16 = 0;
            if (GetEpStatus(gEp0Command.wIndex.U16) == EP_HALTED)
               gEp0Status.wData.U8[MSB] |= 0x01;
            break;

         // Other cases unsupported
         default:
            bEpState = EP_ERROR;
            break;
      }

      // Endpoint0 state assignment
      bEpState = EP_TX;

      // Point ep0 data pointer to transmit data_out
      gEp0Status.pData = (U8 *)&gEp0Status.wData.U16;
      gEp0Status.uNumBytes = 2;
   }
   gEp0Status.bEpState = bEpState;
}

//-----------------------------------------------------------------------------
// GetDescriptorRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void GetDescriptorRequest (void)
{
   UU16 wTempInt;

   // This request is valid in all device states
   // Switch on requested descriptor (Value field)
   switch (gEp0Command.wValue.U8[MSB])
   {
      // Device Descriptor Request
      case DSC_DEVICE:
         // Get size of the requested descriptor
         uNumBytes = STD_DSC_SIZE;
         // Prep to send the requested length
         if (uNumBytes > gEp0Command.wLength.U16)
         {
            uNumBytes = gEp0Command.wLength.U16;
         }
         // Point data pointer to the requested descriptor
         gEp0Status.pData = (void*)&gDescriptorMap.bStdDevDsc;
         bEpState = EP_TX;
         break;

      // Configuration Descriptor Request
      case DSC_CONFIG:
         // Make sure requested descriptor exists
         if (gEp0Command.wValue.U8[LSB] >
         gDescriptorMap.bStdDevDsc[std_bNumConfigurations])
         {
            bEpState = EP_ERROR;
         }
         else
         {
            // Get total length of this configuration descriptor
            // (includes all associated interface and endpoints)
            wTempInt.U8[LSB] = gDescriptorMap.bCfg1[cfg_wTotalLength_lsb];
            wTempInt.U8[MSB] = gDescriptorMap.bCfg1[cfg_wTotalLength_msb];
            uNumBytes = wTempInt.U16;

            // Prep to transmit the requested length
            if (uNumBytes > gEp0Command.wLength.U16)
            {
               uNumBytes = gEp0Command.wLength.U16;
            }
            // Point data pointer to requested descriptor
            gEp0Status.pData = &gDescriptorMap.bCfg1;
            bEpState = EP_TX;
         }
         break;
   }
   gEp0Status.uNumBytes = uNumBytes;
   gEp0Status.bEpState = bEpState;
}

//-----------------------------------------------------------------------------
// GetConfigurationRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void GetConfigurationRequest (void)
{
   // Length field must be 1; Index field must be 0;
   // Value field must be 0
   if ((gEp0Command.wLength.U16 != 1) || (gEp0Command.wIndex.U16) ||
       (gEp0Command.wValue.U16) || (gDeviceStatus.bDevState == DEV_DEFAULT))
   {
      bEpState = EP_ERROR;
   }

   else if (gDeviceStatus.bDevState == DEV_ADDRESS)
   {
      // Prepare data_out for transmission
      gEp0Status.wData.U16 = 0;
      // Point ep0 data pointer to transmit data_out
      gEp0Status.pData = (U8 *)&gEp0Status.wData.U16;
      // ep0 state assignment
      bEpState = EP_TX;
   }

   else
   {
      // Index to desired field
      gEp0Status.pData = (void *)&gDescriptorMap.bCfg1[cfg_bConfigurationValue];

      // ep0 state assignment
      bEpState = EP_TX;
   }
   gEp0Status.uNumBytes = 1;
   gEp0Status.bEpState = bEpState;
}

//-----------------------------------------------------------------------------
// GetInterfaceRequest
//-----------------------------------------------------------------------------
//
// Return Value : None
// Parameters   : None
//
//-----------------------------------------------------------------------------
void GetInterfaceRequest (void)
{
   // Value field must be 0; Length field must be 1
   if ((gEp0Command.wValue.U16) || (gEp0Command.wLength.U16 != 1) ||
       (gDeviceStatus.bDevState != DEV_CONFIG))
   {
      bEpState = EP_ERROR;
   }

   else
   {
      // Make sure requested interface exists
      if (gEp0Command.wIndex.U16 > gDeviceStatus.bNumInterf - 1)
         bEpState = EP_ERROR;
      else
      {
         // Get current interface setting
         gEp0Status.pData = (void *)(&gDeviceStatus.IfStatus)->bCurrentAlt;

         // Length must be 1
         gEp0Status.uNumBytes = 1;
         bEpState = EP_TX;
      }
   }
   gEp0Status.bEpState = bEpState;
}

//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------