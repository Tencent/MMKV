
#include "MMKV.h"

int main() {
    MMKV::initializeMMKV("/tmp/mmkv");
    auto mmkv = MMKV::defaultMMKV();
    mmkv->setBool(true, "bool");
    auto value = mmkv->getBoolForKey("bool");
    printf("bool:%d", value);
    return 0;
}