Summary of the work completed so far, focusing on architecture, major code changes, and the resulting system behavior.

---

# 1. Removed the Old CAN Library Layer

Removed the wrapper around the ESP32 CAN driver:

```text
esp32_can_builtin.cpp
esp32_can_builtin.h
```

Reason:

```text
extra tasks
internal queues
complex driver restart logic
difficult debugging
```

Replaced with **direct TWAI driver usage**.

---

# 2. Introduced a Minimal TWAI Driver Layer

Created:

```text
can_driver.cpp
can_driver.h
```

Responsibilities:

```text
TWAI initialization
TWAI start/stop
CAN speed change
frame transmit
```

Example functions:

```cpp
can_init()
can_stop()
can_set_speed()
can_send()
```

Key configuration:

```cpp
g_config.rx_queue_len = 128
g_config.tx_queue_len = 16
```

This significantly improves burst handling.

---

# 3. Built a Deterministic CAN Processing Pipeline

Implemented a dedicated architecture in **can_manager.cpp**.

Pipeline:

```text
TWAI
↓
canRxTask
↓
ring buffer
↓
transportTask
↓
GVRET
↓
Serial / WiFi
↓
SavvyCAN
```

Goals achieved:

```text
decouple CAN reception from transport
prevent blocking serial/WiFi from affecting CAN capture
avoid frame loss during bursts
```

---

# 4. Implemented a Lock-Free Ring Buffer

Structure:

```cpp
struct RingItem
{
    uint32_t timestamp;
    uint8_t bus;
    CAN_FRAME frame;
};
```

Buffer size:

```text
CAN_RING_SIZE = 1024
```

Push operation:

```text
canRxTask writes
transportTask reads
```

Key safety improvement:

```cpp
__sync_synchronize();
```

Ensures frame memory is written before `ringHead` is updated.

---

# 5. Dedicated FreeRTOS Tasks

Two core tasks were introduced.

### CAN RX Task

```text
Core: 1
Priority: 3
```

Responsibilities:

```text
read TWAI frames
convert to CAN_FRAME
push into ring buffer
update statistics
```

Burst-drain logic:

```cpp
do
{
    process frame
}
while (twai_receive(&msg, 0) == ESP_OK);
```

---

### Transport Task

```text
Core: 0
Priority: 2
```

Responsibilities:

```text
pop frames from ring buffer
forward to GVRET / LAWICEL
handle WiFi / Serial transport
print runtime statistics
update RGB LED indicators
```

---

# 6. Removed Duplicate RX Task

Found unused code:

```cpp
can_driver.cpp → can_rx_task()
```

This task was not used and created confusion.

Removed it so the only RX path is:

```text
can_manager.cpp → canRxTask()
```

---

# 7. Moved CAN Task Creation Into CAN Manager

Previously in `main.cpp`:

```cpp
xTaskCreatePinnedToCore(canRxTask)
xTaskCreatePinnedToCore(transportTask)
```

Moved into:

```cpp
CANManager::setup()
```

Now `main.cpp` remains clean:

```cpp
wifiManager.setup();
canManager.setup();
```

---

# 8. Added Runtime Diagnostics

Statistics printed every second:

```text
FPS
ring usage
overflow count
heap
uptime
```

Example output:

```text
Uptime=0d 00:13:45 Heap:90652 FPS0=820 Overflow=0 Used=35 Head=420 Tail=385
```

Additional metrics added:

```text
ringHighWater
transportFrames
twaiRxQueueHigh
twaiRxQueueFullEvents
```

These help detect where frame loss might occur.

---

# 9. Improved TWAI Receive Logic

RX task drains the TWAI queue efficiently:

```cpp
if (twai_receive(&msg, pdMS_TO_TICKS(1)) == ESP_OK)
{
    do
    {
        process frame
    }
    while (twai_receive(&msg, 0) == ESP_OK);
}
```

Benefits:

```text
minimal interrupt overhead
lower RX latency
better burst handling
```

---

# 10. Clean Final Initialization

Current CAN setup:

```cpp
void CANManager::setup()
{
    can_init(settings.canSettings[0].nomSpeed);

    busLoad setup

    delay(200);

    start CAN_RX task (core 1)
    start transport task (core 0)
}
```

This ensures:

```text
TWAI initialized first
tasks start after driver stabilization
```

---

# Current System Architecture

```text
main.cpp
   ↓
CANManager::setup()
   ↓
TWAI driver init
   ↓
CAN_RX task (core1)
   ↓
lock-free ring buffer (1024)
   ↓
transportTask (core0)
   ↓
GVRET protocol
   ↓
Serial / WiFi
   ↓
SavvyCAN
```

---

# Performance Characteristics

Current configuration:

```text
TWAI RX queue : 128
ring buffer   : 1024
RX task       : core1 priority 3
transport     : core0 priority 2
```

Expected capability:

```text
800–1500 CAN frames/sec sustained
```

with no frame loss under normal conditions.

---

# Current Status

System is now:

```text
stable
simpler than original ESP32RET
deterministic pipeline
good debugging visibility
```

The firmware is essentially a **clean high-performance CAN sniffer implementation** for ESP32-S3.
