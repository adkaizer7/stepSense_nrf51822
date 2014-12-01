#include "mbed.h"
#include "BLEDevice.h"

BLEDevice  ble;
DigitalOut led1(LED1);
DigitalOut led2(LED2);

const uint8_t LED1_UUID[LENGTH_OF_LONG_UUID] = {
    0xfb, 0x71, 0xbc, 0xc0, 0x5a, 0x0c, 0x11, 0xe4,
    0x91, 0xae, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b
};
const uint8_t BUTTON_UUID[LENGTH_OF_LONG_UUID] = {
    0x7a, 0x77, 0xbe, 0x20, 0x5a, 0x0d, 0x11, 0xe4,
    0xa9, 0x5e, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b
};

const uint8_t XBEE_UUID[LENGTH_OF_LONG_UUID] = {
    0x7a, 0x77, 0xbe, 0x21, 0x5b, 0x0e, 0x12, 0xe5,
    0xa9, 0x5e, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b
};
const uint8_t TEST_SERVICE_UUID[LENGTH_OF_LONG_UUID] = {
    0xb0, 0xbb, 0x58, 0x20, 0x5a, 0x0d, 0x11, 0xe4,
    0x93, 0xee, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b
};

const static char DEVICE_NAME[] = "SensorTag";
static volatile bool is_button_pressed = false;
static volatile uint16_t led1_handler;

Serial kl25z(P0_9,P0_11);

uint8_t led_state, button_state;
int valRec;

void disconnectionCallback(Gap::Handle_t handle, Gap::DisconnectionReason_t reason)
{
    ble.startAdvertising(); // restart advertising
}

void changeLED(const GattCharacteristicWriteCBParams *eventDataP) {
    // eventDataP->charHandle is just uint16_t
    // it's used to dispatch the callbacks
    if (eventDataP->charHandle == led1_handler) {
        led1 = eventDataP->data[0] % 2;
    }
}

void button1Pressed() {
    button_state = 1;
    is_button_pressed = true;
}
void button2Pressed() {
    button_state = 2;
    is_button_pressed = true;
}

int main(void)
{
    // button initialization
    InterruptIn button1(BUTTON1);
    InterruptIn button2(BUTTON2);
    button1.mode(PullUp);
    button2.mode(PullUp);
    button1.rise(&button1Pressed);
    button2.rise(&button2Pressed);
    led1 = 0;
    led2 = 0;

    // just a simple service example
    // o led1 characteristics, you can write from the phone to control led1
    // o button characteristics, you can read and get notified
    GattCharacteristic led1_characteristics(
        LED1_UUID, &led_state, sizeof(led_state), sizeof(led_state),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE);
    led1_handler = led1_characteristics.getValueAttribute().getHandle();

    GattCharacteristic button_characteristics(
        BUTTON_UUID, &button_state, sizeof(button_state), sizeof(button_state),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);

    GattCharacteristic xbee_characteristics(
        XBEE_UUID, (uint8_t *)&valRec, sizeof(valRec), sizeof(valRec),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);

    const uint8_t TEST_SERVICE_UUID[LENGTH_OF_LONG_UUID] = {
        0xb0, 0xbb, 0x58, 0x20, 0x5a, 0x0d, 0x11, 0xe4,
        0x93, 0xee, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b};
    GattCharacteristic *charTable[] = {&led1_characteristics, &button_characteristics,&xbee_characteristics};
    GattService testService(TEST_SERVICE_UUID, charTable,
                            sizeof(charTable) / sizeof(GattCharacteristic *));

    // BLE setup, mainly we add service and callbacks
    ble.init();
    ble.addService(testService);
    ble.onDataWritten(&changeLED);
    ble.onDisconnection(disconnectionCallback);

    // setup advertising
    ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED |
                                     GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME,
                                     (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.setAdvertisingInterval(1600); /* 1000ms; in multiples of 0.625ms. */
    ble.startAdvertising();

    while (true) {
    	if (kl25z.readable()){
    		led1 = !led1;
    		valRec = kl25z.getc();
            ble.updateCharacteristicValue(xbee_characteristics.getValueAttribute().getHandle(),
                                          (uint8_t *)&valRec, sizeof(valRec));

    	}
    	else if (is_button_pressed) {
            // if button pressed, we update the characteristics
            led2 = !led2;
            //int valRec = kl25z.getc();
            //uint8_t  *address = (uint8_t *)(&valRec);
            ble.updateCharacteristicValue(button_characteristics.getValueAttribute().getHandle(),
                                          &button_state, sizeof(button_state));
            is_button_pressed = false;

        } else {
            ble.waitForEvent();
        }
    }
}
