#ifndef __MAKEHEX_H__
#define __MAKEHEX_H__

#include <vector>
#include <Arduino.h>

struct RemoteCommand {
    int device;
    int subdevice;
    int function;
};

struct ProtocolDefinition {
    const char* name;
    const char* def;
};

namespace MakeHex {
    std::vector<float> encodeRemoteCommand(const RemoteCommand& cmd, const char* protocolString, int& frequency);
}

#endif
