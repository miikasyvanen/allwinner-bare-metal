/**************************************************************************/
/*!
    @file     tusb.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"

#if TUSB_OPT_HOST_ENABLED || TUSB_OPT_DEVICE_ENABLED
#define _TINY_USB_SOURCE_FILE_

#include "tusb.h"

static bool _initialized = false;

// TODO clean up
#if TUSB_OPT_DEVICE_ENABLED
#include "device/usbd_pvt.h"
#endif

bool tusb_init(void)
{
  // skip if already initialized
  if (_initialized) return true;

#if TUSB_OPT_HOST_ENABLED
  TU_VERIFY( usbh_init() ); // init host stack
#endif

#if TUSB_OPT_DEVICE_ENABLED
  TU_VERIFY ( usbd_init() ); // init device stack
#endif

  _initialized = true;

  return TUSB_ERROR_NONE;
}

#if CFG_TUSB_OS == OPT_OS_NONE
void tusb_task(void)
{
  #if TUSB_OPT_HOST_ENABLED
  usbh_task(NULL);
  #endif

  #if TUSB_OPT_DEVICE_ENABLED
  usbd_task(NULL);
  #endif
}
#endif


/*------------------------------------------------------------------*/
/* Debug
 *------------------------------------------------------------------*/
#if CFG_TUSB_DEBUG
char const* const tusb_strerr[TUSB_ERROR_COUNT] = { ERROR_TABLE(ERROR_STRING) };
#endif

#endif // host or device enabled
