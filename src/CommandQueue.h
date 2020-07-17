#pragma once


#include <Arduino.h>

template< uint8_t LEN_LINES = 16, uint16_t LEN_BYTES = 0 >
class CommandQueue {
public:
    CommandQueue(): buf{""} {}

    void clear() {
      head = tailSend = tailAck;
    }
    
    int getFreeSlots() {
        int freeSlots = LEN_LINES - 1;

        int next = tailAck;
        while (next != head) {
            --freeSlots;
            next = nextSlot(next);
        }

        return freeSlots;
    }

    uint16_t getRemoteFreeSpace() {
        return remoteBufFree;
    }

    // Tries to Add a command to the queue, returns true if possible
    bool push(const String command) {
        int next = nextSlot(head);
        if (next == tailAck || command == "")
            return false;

        buf[head] = command;
        head = next;

        return true;
    }

    // Check if buffer is empty
    bool isEmpty() {
      return head == tailAck;
    }

    bool hasUnsent() {
        return head != tailSend;
    }

    // If there is a command pending to be sent returns it
    String peekSend() {
      return (tailSend == head) ? String() : buf[tailSend];
    }

    // Returns marked command, and advances to the next
    String markSent() {
        if (tailSend == head)
            return String();

        const String command = buf[tailSend];
        const int ln = command.length();
        if(LEN_BYTES>0) {
            if(remoteBufFree<ln) 
                return String();
            remoteBufFree -= ln;
        }
        
        tailSend = nextSlot(tailSend);
        
        return command;
    }

    // Returns true if the command to be sent was the last sent (so there is no pending response)
    bool allAcknowledged() {
      return tailAck == tailSend;
    }

    // Returns the last command sent if it was received by the printer, otherwise returns empty
    String markAcknowledged() {
        if (allAcknowledged())
            return String();

        const String command = buf[tailAck];
        if(LEN_BYTES>0)  remoteBufFree += command.length();
        tailAck = nextSlot(tailAck);

        return command;
    }


private:
    int head, tailSend, tailAck;
    String buf[LEN_LINES];
    uint16_t remoteBufFree = LEN_BYTES;

    // Returns the next buffer slot (after index slot) if it's in between the size of the buffer
    int nextSlot(int index) {
      int next = index + 1;
  
      return next >= LEN_LINES ? 0 : next;
    }

};
