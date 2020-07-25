#pragma once


#include <Arduino.h>
#include <message_buffer.h>

template< uint8_t LEN_LINES = 16, uint16_t LEN_BYTES = 0, uint16_t MAX_LINE_LEN = 100 >
class SizedQueue {
public:
    SizedQueue() {
        freeLines = LEN_LINES;
        freeBytes = LEN_BYTES;
        buf = xMessageBufferCreateStatic(LEN_BYTES, data, &bufStruct);
    }

    void clear() {
        xMessageBufferReset(buf);
        havePeekedLine = false;
        peekedLineLen = 0;
        freeLines = LEN_LINES;
        freeBytes = LEN_BYTES;
    }

    bool canPush(size_t len) const {
        if(len>MAX_LINE_LEN) len = MAX_LINE_LEN;
        return freeBytes>len && freeLines>0;
    }

    bool push(char* msg, size_t len)  {
        if(!canPush(len)) return false;
        if(len>MAX_LINE_LEN) len = MAX_LINE_LEN;
        xMessageBufferSend(buf, msg, len, 0);
        freeLines --;
        freeBytes -= len+1;
        return true;
    }

    inline size_t size() const {
        return LEN_LINES-freeLines;
    }

    inline size_t getFreeLines() const {
        return freeLines;
    }

    inline size_t bytes() const {
        return LEN_BYTES-freeBytes;
    }

    inline size_t getFreeBytes() const {
        return freeBytes;
    }

    size_t peek(char* &msg) {
        if(size() == 0) return 0;
        if(!havePeekedLine) {
            loadLineFromBuf();
        }
        //len = peekedLineLen;
        msg = peekedLine;
        return peekedLineLen;
    }

    size_t pop(char * msg, size_t maxLen ) {
        if(size() == 0) return 0;
        if(!havePeekedLine) {
            loadLineFromBuf();
        }
        size_t ret = peekedLineLen;
        size_t minLen = std::min(maxLen, peekedLineLen);
        memcpy( msg, peekedLine, minLen );
        msg[minLen] = 0;
        freeLines++;
        freeBytes+=peekedLineLen+1;
        havePeekedLine = false;
        peekedLineLen = 0;
        return ret;
    }

    void pop() {
        if(size() == 0) return;
        freeLines ++;
        if(havePeekedLine) {
            freeBytes += peekedLineLen+1;
            havePeekedLine = false;
            peekedLineLen = 0;
        } else {
            size_t r = xMessageBufferReceive(buf, peekedLine, MAX_LINE_LEN, 0);
            freeBytes += r+1;
        }

    }


private:
    MessageBufferHandle_t buf;
    StaticMessageBuffer_t bufStruct;
    uint8_t data[LEN_BYTES];

    size_t freeLines;
    size_t freeBytes;
    char peekedLine[MAX_LINE_LEN+1];
    size_t peekedLineLen;
    bool havePeekedLine;

    bool loadLineFromBuf() {
        peekedLineLen = xMessageBufferReceive(buf, peekedLine, MAX_LINE_LEN, 0);
        havePeekedLine = peekedLineLen>0;
        peekedLine[peekedLineLen]=0;
        return havePeekedLine;
    }
};

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
    virtual bool canSend()=0;
    virtual String markSent()=0;
    bool allAcknowledged() { return !hasUnacknowledged(); };
    virtual bool hasUnacknowledged()=0;
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

    bool canSend() override {
        if (tailSend == head) return false;
        if(LEN_BYTES>0) {
            const String command = buf[tailSend];
            return remoteBufFree>=command.length(); 
        }
        return true;
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
    bool hasUnacknowledged() override  {
      return tailAck != tailSend;
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
    virtual bool canSend()      { if(queue0.hasUnsent()) return queue0.canSend();    else return queue1.canSend();    }
    virtual String peekUnsent() { if(queue0.hasUnsent()) return queue0.peekUnsent(); else return queue1.peekUnsent(); }
    virtual String markSent()   { if(queue0.hasUnsent()) return queue0.markSent();   else return queue1.markSent();   }
    virtual bool hasUnacknowledged()    { return queue0.hasUnacknowledged() && queue1.hasUnacknowledged(); }
    virtual String peekUnacknowledged() { if(queue0.hasUnacknowledged()) return queue0.peekUnacknowledged(); else return queue1.peekUnacknowledged(); }
    virtual String markAcknowledged()   { if(queue0.hasUnacknowledged()) return queue0.markAcknowledged();   else return queue1.markAcknowledged(); }

private:
    CommandQueue<LEN_LINES, LEN_BYTES> queue1;
    CommandQueue<LEN_LINES_PRIORITY, 0> queue0;
};