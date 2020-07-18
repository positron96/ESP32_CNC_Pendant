#pragma once


#include <Arduino.h>

class AbstractQueue {
public:
    virtual void clear() = 0;
    virtual int getFreeSlots()=0;
    virtual uint16_t getRemoteFreeSpace()=0;
    virtual bool push(const String command)=0;
    virtual bool pushPriority(const String command) { return push(command); } ;
    virtual bool isEmpty()=0;
    virtual bool hasUnsent()=0;
    virtual String peekUnsent()=0;
    virtual String markSent()=0;
    virtual bool allAcknowledged()=0;
    virtual String markAcknowledged()=0;
    virtual String peekUnacknowledged()=0;
};

template< uint8_t LEN_LINES = 16, uint16_t LEN_BYTES = 0 >
class CommandQueue: public AbstractQueue {
public:
    CommandQueue(): buf{""} {}

    void clear() override {
      head = tailSend = tailAck;
    }
    
    int getFreeSlots() override {
        int freeSlots = LEN_LINES - 1;

        int next = tailAck;
        while (next != head) {
            --freeSlots;
            next = nextSlot(next);
        }

        return freeSlots;
    }

    uint16_t getRemoteFreeSpace() override  {
        return remoteBufFree;
    }

    // Tries to Add a command to the queue, returns true if possible
    bool push(const String command) override  {
        int next = nextSlot(head);
        if (next == tailAck || command == "")
            return false;

        buf[head] = command;
        head = next;

        return true;
    }

    // Check if buffer is empty
    bool isEmpty() override  {
      return head == tailAck;
    }

    bool hasUnsent() override  {
        return head != tailSend;
    }

    // If there is a command pending to be sent returns it
    String peekUnsent()  override {
      return (tailSend == head) ? String() : buf[tailSend];
    }

    // Returns marked command, and advances to the next
    String markSent()  override {
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
    bool allAcknowledged() override  {
      return tailAck == tailSend;
    }

    // Returns the last command sent if it was received by the printer, otherwise returns empty
    String markAcknowledged() override  {
        if (allAcknowledged())
            return String();

        const String command = buf[tailAck];
        if(LEN_BYTES>0)  remoteBufFree += command.length();
        tailAck = nextSlot(tailAck);

        return command;
    }

    // Returns the last command sent if it was received by the printer, otherwise returns empty
    String peekUnacknowledged() override  {
        if (allAcknowledged())
            return String();

        return buf[tailAck];
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


template< uint8_t LEN_LINES = 16, uint16_t LEN_BYTES = 0, uint8_t LEN_LINES_PRIORITY=0 >
class DoubleCommandQueue: public AbstractQueue {

public:

    virtual void clear() { queue0.clear(); queue1.clear(); }
    virtual int getFreeSlots() { return queue1.getFreeSlots(); }
    virtual uint16_t getRemoteFreeSpace() { return queue1.getRemoteFreeSpace();  }
    virtual bool push(const String command) { return queue1.push(command); }
    virtual bool pushPriority(const String command) {  return queue0.push(command); } 
    virtual bool isEmpty() { return queue0.isEmpty() && queue1.isEmpty(); }
    virtual bool hasUnsent() { return queue0.hasUnsent() && queue1.hasUnsent(); }
    virtual String peekUnsent() { if(queue0.hasUnsent()) return queue0.peekUnsent(); else return queue1.peekUnsent(); }
    virtual String markSent() { if(queue0.hasUnsent()) return queue0.markSent(); else return queue1.markSent(); }
    virtual bool allAcknowledged() { return queue0.allAcknowledged() && queue1.allAcknowledged(); }
    virtual String markAcknowledged() { if(!queue0.allAcknowledged()) return queue0.markAcknowledged(); else return queue1.markAcknowledged(); }
    virtual String peekUnacknowledged() { if(!queue0.allAcknowledged()) return queue0.peekUnacknowledged(); else return queue1.peekUnacknowledged(); }

private:
    CommandQueue<LEN_LINES, LEN_BYTES> queue1;
    CommandQueue<LEN_LINES_PRIORITY, 0> queue0;
};