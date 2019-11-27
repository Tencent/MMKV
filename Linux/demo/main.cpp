
#include "MMKV.h"
#include <cstdio>

int main() {
    MMKV::initializeMMKV("/tmp/mmkv");
    auto mmkv = MMKV::defaultMMKV();
    mmkv->setBool(true, "bool");
    auto value = mmkv->getBoolForKey("bool");
    printf("bool value:%d", value);
    fflush(stdout);
    return 0;
}
