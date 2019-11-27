
#include "MMKV.h"
#include <cstdio>

int main() {
    MMKV::initializeMMKV("/tmp/mmkv");
    auto mmkv = MMKV::defaultMMKV();
    mmkv->setBool(true, "bool");
    auto value = mmkv->getBoolForKey("bool");
    printf("bool:%d", value);
    fflush(stdout);
    mmkv->setBool(true, "bool");
    return 0;
}
