/*
             LUFA Library
     Copyright (C) Dean Camera, 2009.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2009  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, and distribute this software
  and its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appear in all
  copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Header file for Descriptors.c.
 */
 
#ifndef _DESCRIPTORS_H_
#define _DESCRIPTORS_H_
		
	/* Includes: */
		#include <LUFA/Drivers/USB/USB.h>
		#include <avr/pgmspace.h>

		#if (USE_INTERNAL_SERIAL == NO_DESCRIPTOR)
			#warning USE_INTERNAL_SERIAL is not available on this AVR - please manually construct a device serial descriptor.
		#endif


	/* Macros: */
		#define IN_EP                       1
		#define OUT_EP                      2
		#define IN_EP_SIZE                  64
		#define OUT_EP_SIZE                 64

	/* Type Defines: */
		typedef struct
		{
			USB_Descriptor_Configuration_Header_t Config;
			USB_Descriptor_Interface_t            Interface;
			USB_Descriptor_Endpoint_t             DataINEndpoint;
			USB_Descriptor_Endpoint_t             DataOUTEndpoint;
		} USB_Descriptor_Configuration_t;

	/* External Variables: */
		extern USB_Descriptor_Configuration_t ConfigurationDescriptor;

	/* Function Prototypes: */
		uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue, const uint8_t wIndex, void** const DescriptorAddress)
											ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(3);
#endif
