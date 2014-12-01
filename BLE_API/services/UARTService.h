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

#ifndef __BLE_UART_SERVICE_H__
#define __BLE_UART_SERVICE_H__

#include "Stream.h"

#include "UUID.h"
#include "BLEDevice.h"

extern const uint8_t UARTServiceBaseUUID[LENGTH_OF_LONG_UUID];
extern const uint16_t UARTServiceShortUUID;
extern const uint16_t UARTServiceTXCharacteristicShortUUID;
extern const uint16_t UARTServiceRXCharacteristicShortUUID;

extern const uint8_t UARTServiceUUID[LENGTH_OF_LONG_UUID];
extern const uint8_t UARTServiceUUID_reversed[LENGTH_OF_LONG_UUID];

extern const uint8_t UARTServiceTXCharacteristicUUID[LENGTH_OF_LONG_UUID];
extern const uint8_t UARTServiceRXCharacteristicUUID[LENGTH_OF_LONG_UUID];

class UARTService : public Stream {
public:
    /**< Maximum length of data (in bytes) that can be transmitted by the UART service module to the peer. */
    static const unsigned GATT_MTU_SIZE_DEFAULT = 23;
    static const unsigned BLE_UART_SERVICE_MAX_DATA_LEN = (GATT_MTU_SIZE_DEFAULT - 3);

public:
    UARTService(BLEDevice &_ble) :
        Stream("blueart"),
        ble(_ble),
        receiveBuffer(),
        sendBuffer(),
        sendBufferIndex(0),
        numBytesReceived(0),
        receiveBufferIndex(0),
        txCharacteristic(UARTServiceTXCharacteristicUUID, receiveBuffer, 1, BLE_UART_SERVICE_MAX_DATA_LEN,
                         GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE),
        rxCharacteristic(UARTServiceRXCharacteristicUUID, sendBuffer, 1, BLE_UART_SERVICE_MAX_DATA_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY) {
        GattCharacteristic *charTable[] = {&txCharacteristic, &rxCharacteristic};
        GattService         uartService(UARTServiceUUID, charTable, sizeof(charTable) / sizeof(GattCharacteristic *));

        ble.addService(uartService);
        ble.onDataWritten(this, &UARTService::onDataWritten);
    }

    /**
     * Note: TX and RX characteristics are to be interpreted from the viewpoint of the GATT client using this service.
     */
    uint16_t getTXCharacteristicHandle() {
        return txCharacteristic.getValueAttribute().getHandle();
    }

    /**
     * Note: TX and RX characteristics are to be interpreted from the viewpoint of the GATT client using this service.
     */
    uint16_t getRXCharacteristicHandle() {
        return rxCharacteristic.getValueAttribute().getHandle();
    }

    /**
     * Following a call to this function, all writes to stdout (such as from
     * printf) get redirected to the outbound characteristic of this service.
     * This might be very useful when wanting to receive debug messages over BLE.
     *
     * @Note: debug messages originating from printf() like calls are buffered
     * before being sent out. A '\n' in the printf() triggers the buffer update
     * to the underlying characteristic.
     *
     * @Note: long messages need to be chopped up into 20-byte updates so that
     * they flow out completely with notifications. The receiver should be
     * prepared to stitch these messages back.
     */
    void retargetStdout() {
        freopen("/blueart", "w", stdout);
    }

    /**
     * This callback allows the UART service to receive updates to the
     * txCharacteristic. The application should forward the call to this
     * function from the global onDataWritten() callback handler; or if that's
     * not used, this method can be used as a callback directly.
     */
    virtual void onDataWritten(const GattCharacteristicWriteCBParams *params) {
        if (params->charHandle == getTXCharacteristicHandle()) {
            uint16_t bytesRead = params->len;
            if (bytesRead <= BLE_UART_SERVICE_MAX_DATA_LEN) {
                numBytesReceived   = bytesRead;
                receiveBufferIndex = 0;
                memcpy(receiveBuffer, params->data, numBytesReceived);
            }
        }
    }

protected:
    /**
     * Override for Stream::write().
     *
     * We attempt to collect bytes before pushing them to the UART RX
     * characteristic--writing to the RX characteristic will then generate
     * notifications for the client. Updates made in quick succession to a
     * notification-generating characteristic will result in data being buffered
     * in the bluetooth stack as notifications are sent out. The stack will have
     * its limits for this buffering; typically a small number under 10.
     * Collecting data into the sendBuffer buffer helps mitigate the rate of
     * updates. But we shouldn't buffer a large amount of data before updating
     * the characteristic otherwise the client will need to turn around and make
     * a long read request; this is because notifications include only the first
     * 20 bytes of the updated data.
     *
     * @param  buffer The received update
     * @param  length Amount of characters to be appended.
     * @return        Amount of characters appended to the rxCharacteristic.
     */
    virtual ssize_t write(const void* _buffer, size_t length) {
        size_t origLength     = length;
        const uint8_t *buffer = static_cast<const uint8_t *>(_buffer);

        if (ble.getGapState().connected) {
            unsigned bufferIndex = 0;
            while (length) {
                unsigned bytesRemainingInSendBuffer = BLE_UART_SERVICE_MAX_DATA_LEN - sendBufferIndex;
                unsigned bytesToCopy = (length < bytesRemainingInSendBuffer) ? length : bytesRemainingInSendBuffer;

                /* copy bytes into sendBuffer */
                memcpy(&sendBuffer[sendBufferIndex], &buffer[bufferIndex], bytesToCopy);
                length          -= bytesToCopy;
                sendBufferIndex += bytesToCopy;
                bufferIndex     += bytesToCopy;

                /* have we collected enough? */
                if ((sendBufferIndex == BLE_UART_SERVICE_MAX_DATA_LEN) ||
                    // (sendBuffer[sendBufferIndex - 1] == '\r')          ||
                    (sendBuffer[sendBufferIndex - 1] == '\n')) {
                    ble.updateCharacteristicValue(getRXCharacteristicHandle(), static_cast<const uint8_t *>(sendBuffer), sendBufferIndex);
                    sendBufferIndex = 0;
                }
            }
        }

        return origLength;
    }

    /**
     * Override for Stream::_putc()
     * @param  c
     *         This function writes the character c, cast to an unsigned char, to stream.
     * @return
     *     The character written as an unsigned char cast to an int or EOF on error.
     */
    virtual int _putc(int c) {
        return (write(&c, 1) == 1) ? 1 : EOF;
    }

    virtual int _getc() {
        if (receiveBufferIndex == numBytesReceived) {
            return EOF;
        }

        return receiveBuffer[receiveBufferIndex++];
    }

    virtual int isatty() {
        return 1;
    }

private:
    BLEDevice          &ble;

    uint8_t             receiveBuffer[BLE_UART_SERVICE_MAX_DATA_LEN]; /**< The local buffer into which we receive
                                                                       *   inbound data before forwarding it to the
                                                                       *   application. */

    uint8_t             sendBuffer[BLE_UART_SERVICE_MAX_DATA_LEN];    /**< The local buffer into which outbound data is
                                                                       *   accumulated before being pushed to the
                                                                       *   rxCharacteristic. */
    uint8_t             sendBufferIndex;
    uint8_t             numBytesReceived;
    uint8_t             receiveBufferIndex;

    GattCharacteristic  txCharacteristic; /**< From the point of view of the external client, this is the characteristic
                                           *   they'd write into in order to communicate with this application. */
    GattCharacteristic  rxCharacteristic; /**< From the point of view of the external client, this is the characteristic
                                           *   they'd read from in order to receive the bytes transmitted by this
                                           *   application. */
};

#endif /* #ifndef __BLE_UART_SERVICE_H__*/

