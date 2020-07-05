#pragma once


#include <Arduino.h>

template< uint8_t LEN_LINES = 16, uint16_t LEN_BYTES = 0 >
class CommandQueue {
public:
    CommandQueue(): buf{""} {}

    // Check if buffer is empty
    bool isEmpty() {
      return head == tail;
    }

    // Returns true if the command to be sent was the last sent (so there is no pending response)
    bool isAckEmpty() {
      return tail == sendTail;
    }

    void clear() {
      head = sendTail = tail;
    }

    // If there is a command pending to be sent returns it
    String peekSend() {
      return (sendTail == head) ? String() : buf[sendTail];
    }
    
    int getFreeSlots() {
        int freeSlots = LEN_LINES - 1;

        int next = tail;
        while (next != head) {
            --freeSlots;
            next = nextBufferSlot(next);
        }

        return freeSlots;
    }

    // Tries to Add a command to the queue, returns true if possible
    bool push(const String command) {
        int next = nextBufferSlot(head);
        if (next == tail || command == "")
            return false;

        buf[head] = command;
        head = next;

        return true;
    }

    // Returns the next command to be sent, and advances to the next
    String popSend() {
        if (sendTail == head)
            return String();

        const String command = buf[sendTail];
        sendTail = nextBufferSlot(sendTail);
        
        return command;
    }

    // Returns the last command sent if it was received by the printer, otherwise returns empty
    String popAcknowledge() {
        if (isAckEmpty())
            return String();

        const String command = buf[tail];
        tail = nextBufferSlot(tail);

        return command;
    }


private:
    int head, sendTail, tail;
    String buf[LEN_LINES];

    // Returns the next buffer slot (after index slot) if it's in between the size of the buffer
    int nextBufferSlot(int index) {
      int next = index + 1;
  
      return next >= LEN_LINES ? 0 : next;
    }

};
