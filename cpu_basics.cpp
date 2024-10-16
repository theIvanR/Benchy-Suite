#include <iostream>
#include <cstring>

void cpuid(int eax, int ecx, int* cpuInfo) {
    __asm__ (
        "cpuid"
        : "=a" (cpuInfo[0]), "=b" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
        : "a" (eax), "c" (ecx)
    );
}

void printCPUBrand() {
    int cpuInfo[4];
    char brand[0x40];
    std::memset(brand, 0, sizeof(brand));

    cpuid(0x80000002, 0, cpuInfo);
    std::memcpy(brand, cpuInfo, sizeof(cpuInfo));
    cpuid(0x80000003, 0, cpuInfo);
    std::memcpy(brand + 16, cpuInfo, sizeof(cpuInfo));
    cpuid(0x80000004, 0, cpuInfo);
    std::memcpy(brand + 32, cpuInfo, sizeof(cpuInfo));

    std::cout << "CPU Brand String: " << brand << std::endl;
}

int main() {
    printCPUBrand();
    return 0;
}
