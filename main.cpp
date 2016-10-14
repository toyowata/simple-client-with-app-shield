#include "mbed.h"
#include "security.h"
#include "easy-connect.h"
#include "simple-mbed-client.h"
#include "C12832.h"
#include "FXOS8700Q.h"

// Using Arduino pin notation
C12832 lcd(D11, D13, D12, D7, D10);

I2C i2c(PTE25, PTE24);
FXOS8700QAccelerometer acc(i2c, FXOS8700CQ_SLAVE_ADDR1);
FXOS8700QMagnetometer mag(i2c, FXOS8700CQ_SLAVE_ADDR1);

Serial pc(USBTX, USBRX);
SimpleMbedClient client;

DigitalOut led(LED1);
DigitalOut blinkLed(LED2);
InterruptIn btn(MBED_CONF_APP_BUTTON);
Semaphore updates(0);

void patternUpdated(string v) {
    printf("New pattern: %s\n", v.c_str());
}

void lcdTextUpdated(string v) {
    if (v.length() > 30) {
        v.erase(v.begin() + 30, v.end());
    }
    printf("New text is %s\r\n", v.c_str());

    lcd.cls();
    lcd.locate(0, 3);
    lcd.printf(v.c_str());
}

SimpleResourceInt btn_count = client.define_resource("button/0/clicks", 0, M2MBase::GET_ALLOWED);
SimpleResourceString pattern = client.define_resource("led/0/pattern", "500:500:500:500:500:500:500", &patternUpdated);

SimpleResourceString lcd_text = client.define_resource("lcd/0/text",
    "Hello from the cloud", M2MBase::GET_PUT_ALLOWED, true, &lcdTextUpdated);

void fall() {
    updates.release();
}

void toggleLed() {
    led = !led;
}

void registered() {
    pc.printf("Registered\r\n");
}
void unregistered() {
    pc.printf("Unregistered\r\n");
}

void play(void* args) {
    stringstream ss(pattern);
    string item;
    while(getline(ss, item, ':')) {
        wait_ms(atoi((const char*)item.c_str()));
        blinkLed = !blinkLed;
    }
}

SimpleResourceInt aX = client.define_resource("accel/0/x", 0, M2MBase::GET_ALLOWED);
SimpleResourceInt aY = client.define_resource("accel/0/y", 0, M2MBase::GET_ALLOWED);
SimpleResourceInt aZ = client.define_resource("accel/0/z", 0, M2MBase::GET_ALLOWED);

void readAccel() {
    acc.enable();
    
    motion_data_counts_t acc_data;

    while (1) {
        acc.getAxis(acc_data);

        aX = acc_data.x;
        aY = acc_data.y;
        aZ = acc_data.z;

        pc.printf("ACC: X=%d Y=%d Z=%d\r\n", acc_data.x, acc_data.y, acc_data.z);
        wait_ms(2000);
    }
}

int main() {
    pc.baud(115200);

    lcdTextUpdated(static_cast<string>(lcd_text).c_str());

    Thread accelThread;
    accelThread.start(&readAccel);

    btn.fall(&fall);

    Ticker t;
    t.attach(&toggleLed, 1.0f);

    client.define_function("led/0/play", &play);

    NetworkInterface* network = easy_connect(true);
    if (!network) {
        return 1;
    }

    struct MbedClientOptions opts = client.get_default_options();
    opts.ServerAddress = MBED_SERVER_ADDRESS;
    opts.DeviceType = "JansAccel";
    bool setup = client.setup(opts, network);
    if (!setup) {
        printf("Client setup failed\n");
        return 1;
    }

    client.on_registered(&registered);
    client.on_unregistered(&unregistered);

    while (1) {
        int v = updates.wait(25000);
        if (v == 1) {
            btn_count = btn_count + 1;
            printf("Button count is now %d\n", static_cast<int>(btn_count));
        }
        client.keep_alive();
    }
}
