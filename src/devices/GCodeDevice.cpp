#include "GCodeDevice.h"

GCodeDevice *GCodeDevice::device;

GCodeDevice *GCodeDevice::getDevice() {
    return device;
}
void GCodeDevice::setDevice(GCodeDevice *dev) {
    device = dev;
}