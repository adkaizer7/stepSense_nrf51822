/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BLE_DEVICE_INSTANCE_BASE__
#define __BLE_DEVICE_INSTANCE_BASE__

/**
 *  The interface for the transport object to be created by the target library's
 *  createBLEDeviceInstance().
 */
class BLEDeviceInstanceBase
{
public:
    virtual const char *getVersion(void)           = 0;
    virtual Gap&        getGap()                   = 0;
    virtual GattServer& getGattServer()            = 0;
    virtual ble_error_t init(void)                 = 0;
    virtual ble_error_t reset(void)                = 0;
    virtual ble_error_t setTxPower(int8_t txPower) = 0;
    virtual void        waitForEvent(void)         = 0;
};

/**
 * BLEDevice uses composition to hide an interface object encapsulating the
 * backend transport.
 *
 * The following API is used to create the singleton interface object. An
 * implementation for this function must be provided by the device-specific
 * library, otherwise there will be a linker error.
 */
extern BLEDeviceInstanceBase *createBLEDeviceInstance(void);

#endif // ifndef __BLE_DEVICE_INSTANCE_BASE__
