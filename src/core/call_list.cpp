#include "call_list.hpp"
#include "ini.hpp"
#include <fstream>
#include <algorithm>
#include <cstdlib>

std::vector<std::string> gCalls;

// Built-in fallback callsign pool used when calls.bin is absent
static const char* kBuiltinCalls[] = {
    "AA1A","AA2Z","AA3B","AA4R","AA5NT","AA6E","AA7A","AA8R","AA9A",
    "AB1B","AB2C","AB3D","AB4E","AB5F","AB6G","AB7H","AB8I","AB9J",
    "AC0A","AC1C","AC2D","AC3E","AC4F","AC5G","AC6H","AC7I","AC8J",
    "AD0B","AD1D","AD2E","AD3F","AD4G","AD5H","AD6I","AD7J","AD8K",
    "AE0C","AE1E","AE2F","AE3G","AE4H","AE5I","AE6J","AE7K","AE8L",
    "AF0D","AF1F","AF2G","AF3H","AF4I","AF5J","AF6K","AF7L","AF8M",
    "AG0E","AG1G","AG2H","AG3I","AG4J","AG5K","AG6L","AG7M","AG8N",
    "AH0F","AH1H","AH2I","AH3J","AH4K","AH6L","AH7M","AH8N","AH9O",
    "AI0G","AI1I","AI2J","AI3K","AI4L","AI5M","AI6N","AI7O","AI8P",
    "AJ0H","AJ1J","AJ2K","AJ3L","AJ4M","AJ5N","AJ6O","AJ7P","AJ8Q",
    "DL1ABC","DL2XY","DL3ZZ","DL4AB","DL5CD","DL6EF","DL7GH","DL8IJ",
    "DK1AA","DK2BB","DK3CC","DK4DD","DK5EE","DK6FF","DK7GG","DK8HH",
    "DJ1AA","DJ2BB","DJ3CC","DJ4DD","DJ5EE","DJ6FF","DJ7GG","DJ8HH",
    "G0ABC","G1XYZ","G3ZZZ","G4ABC","G0DEF","G3GHI","G4JKL","G0MNO",
    "GM0ABC","GW0XYZ","GI0DEF","GD0GHI","GJ0JKL","GU0MNO","GH0PQR",
    "F1ABC","F2XYZ","F3DEF","F4GHI","F5JKL","F6MNO","F8PQR","F9STU",
    "ON4ABC","ON5XYZ","ON6DEF","ON7GHI","OO4JKL","OT5MNO","OQ6PQR",
    "PA0ABC","PA1XYZ","PA2DEF","PA3GHI","PA4JKL","PD0MNO","PE1PQR",
    "SM0ABC","SM1XYZ","SM2DEF","SM3GHI","SM4JKL","SM5MNO","SM6PQR",
    "OH0AB","OH1XY","OH2ZZ","OH3AB","OH4CD","OH5EF","OH6GH","OH7IJ",
    "LA0A","LA1B","LA2C","LA3D","LA4E","LA5F","LA6G","LA7H","LA8I",
    "OZ1ABC","OZ2XYZ","OZ3DEF","OZ4GHI","OZ5JKL","OZ6MNO","OZ7PQR",
    "SP1ABC","SP2XYZ","SP3DEF","SP4GHI","SP5JKL","SP6MNO","SP7PQR",
    "UR0A","UR1B","UR2C","UR3D","UR4E","UR5F","UR6G","UR7H","UR8I",
    "UA0A","UA1B","UA2C","UA3D","UA4E","UA6F","UA9G","RA0H","RA3I",
    "JA0ABC","JA1XYZ","JA2DEF","JA3GHI","JA4JKL","JA5MNO","JA6PQR",
    "JH0ABC","JH1XYZ","JH2DEF","JH3GHI","JH4JKL","JH5MNO","JH6PQR",
    "JR0ABC","JR1XYZ","JR2DEF","JR3GHI","JR4JKL","JR5MNO","JR6PQR",
    "VK1AA","VK2BB","VK3CC","VK4DD","VK5EE","VK6FF","VK7GG","VK8HH",
    "ZL1AA","ZL2BB","ZL3CC","ZL4DD","ZL1XYZ","ZL2ABC","ZL3DEF",
    "VE1AA","VE2BB","VE3CC","VE4DD","VE5EE","VE6FF","VE7GG","VE9HH",
    "VA1AA","VA2BB","VA3CC","VA4DD","VA5EE","VA6FF","VA7GG",
    "W0AA","W1BB","W2CC","W3DD","W4EE","W5FF","W6GG","W7HH","W8II","W9JJ",
    "K0AA","K1BB","K2CC","K3DD","K4EE","K5FF","K6GG","K7HH","K8II","K9JJ",
    "N0AA","N1BB","N2CC","N3DD","N4EE","N5FF","N6GG","N7HH","N8II","N9JJ",
    "WA0AA","WA1BB","WA2CC","WA3DD","WA4EE","WA5FF","WA6GG","WA7HH",
    "WB0AA","WB1BB","WB2CC","WB3DD","WB4EE","WB5FF","WB6GG","WB7HH",
    "WC0AA","WC1BB","WC2CC","WC3DD","WC4EE","WC5FF","WC6GG","WC7HH",
    "WD0AA","WD1BB","WD2CC","WD3DD","WD4EE","WD5FF","WD6GG","WD7HH",
    "KA0AA","KA1BB","KA2CC","KA3DD","KA4EE","KA5FF","KA6GG","KA7HH",
    "KB0AA","KB1BB","KB2CC","KB3DD","KB4EE","KB5FF","KB6GG","KB7HH",
    "KC0AA","KC1BB","KC2CC","KC3DD","KC4EE","KC5FF","KC6GG","KC7HH",
    "KD0AA","KD1BB","KD2CC","KD3DD","KD4EE","KD5FF","KD6GG","KD7HH",
    "KE0AA","KE1BB","KE2CC","KE3DD","KE4EE","KE5FF","KE6GG","KE7HH",
    "KF0AA","KF1BB","KF2CC","KF3DD","KF4EE","KF5FF","KF6GG","KF7HH",
    "KG0AA","KG1BB","KG2CC","KG3DD","KG4EE","KG5FF","KG6GG","KG7HH",
    "I0ABC","I1XYZ","I2DEF","I3GHI","I4JKL","I5MNO","I6PQR","I7STU",
    "IK0ABC","IK1XYZ","IK2DEF","IK3GHI","IK4JKL","IK5MNO","IK6PQR",
    "EA1ABC","EA2XYZ","EA3DEF","EA4GHI","EA5JKL","EA6MNO","EA7PQR",
    "EB1ABC","EC2XYZ","ED3DEF","EE4GHI","EF5JKL","EG6MNO","EH7PQR",
    "CT1ABC","CT2XYZ","CT3DEF","CU1GHI","CU2JKL","CU3MNO",
    "HB0A","HB9ABC","HB9XYZ","HB9DEF","HB9GHI","HB9JKL","HB9MNO",
    "OE1ABC","OE2XYZ","OE3DEF","OE4GHI","OE5JKL","OE6MNO","OE7PQR",
    "YO2ABC","YO3XYZ","YO4DEF","YO5GHI","YO6JKL","YO7MNO","YO8PQR",
    "LZ1ABC","LZ2XYZ","LZ3DEF","LZ4GHI","LZ5JKL","LZ6MNO","LZ7PQR",
    "SV0ABC","SV1XYZ","SV2DEF","SV3GHI","SV4JKL","SV5MNO","SV6PQR",
    "TA0ABC","TA1XYZ","TA2DEF","TA3GHI","TA4JKL","TA5MNO","TA7PQR",
    "4X1ABC","4X2XYZ","4X3DEF","4X4GHI","4X5JKL","4X6MNO","4Z5ABC",
    "ZS1ABC","ZS2XYZ","ZS3DEF","ZS4GHI","ZS5JKL","ZS6MNO","ZS9PQR",
    "PY0ABC","PY1XYZ","PY2DEF","PY3GHI","PY4JKL","PY5MNO","PY6PQR",
    "LU0ABC","LU1XYZ","LU2DEF","LU3GHI","LU4JKL","LU5MNO","LU6PQR",
    "CE0ABC","CE1XYZ","CE2DEF","CE3GHI","CE4JKL","CE5MNO","CE6PQR",
    "XE0ABC","XE1XYZ","XE2DEF","XE3GHI","XF1JKL","XF4MNO",
    "TF0ABC","TF1XYZ","TF2DEF","TF3GHI","TF4JKL","TF5MNO",
    "EI0ABC","EI1XYZ","EI2DEF","EI3GHI","EI4JKL","EI5MNO","EI7PQR",
    "9A0ABC","9A1XYZ","9A2DEF","9A3GHI","9A4JKL","9A5MNO","9A6PQR",
    "S50ABC","S51XYZ","S52DEF","S53GHI","S54JKL","S55MNO","S56PQR",
    "OM0ABC","OM1XYZ","OM2DEF","OM3GHI","OM4JKL","OM5MNO","OM6PQR",
    "OK0ABC","OK1XYZ","OK2DEF","OK3GHI","OK4JKL","OK5MNO","OK6PQR",
    "YU0ABC","YU1XYZ","YU2DEF","YU3GHI","YU4JKL","YU5MNO","YU6PQR",
    "HA0ABC","HA1XYZ","HA2DEF","HA3GHI","HA4JKL","HA5MNO","HA6PQR",
    "HG0ABC","HG1XYZ","HG2DEF","HG3GHI","HG4JKL","HG5MNO","HG6PQR",
};

void loadCallList(const std::string& path) {
    gCalls.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        // Load built-in fallback
        for (auto* c : kBuiltinCalls)
            gCalls.emplace_back(c);
        return;
    }

    // Read index: (37^2+1) integers = 1370 ints = 5480 bytes
    constexpr int CHRCOUNT   = 37;
    constexpr int INDEXSIZE  = CHRCOUNT * CHRCOUNT + 1;
    constexpr int INDEXBYTES = INDEXSIZE * static_cast<int>(sizeof(int));

    std::vector<int> idx(INDEXSIZE);
    f.read(reinterpret_cast<char*>(idx.data()), INDEXBYTES);
    if (!f) {
        for (auto* c : kBuiltinCalls) gCalls.emplace_back(c);
        return;
    }

    if (idx[0] != INDEXBYTES) {
        for (auto* c : kBuiltinCalls) gCalls.emplace_back(c);
        return;
    }

    f.seekg(0, std::ios::end);
    int fileSize = static_cast<int>(f.tellg());
    if (idx[INDEXSIZE - 1] != fileSize) {
        for (auto* c : kBuiltinCalls) gCalls.emplace_back(c);
        return;
    }

    f.seekg(INDEXBYTES);
    int dataLen = fileSize - INDEXBYTES;
    std::vector<char> data(dataLen);
    f.read(data.data(), dataLen);
    if (!f) {
        for (auto* c : kBuiltinCalls) gCalls.emplace_back(c);
        return;
    }

    const char* p  = data.data();
    const char* pe = p + dataLen;
    while (p < pe) {
        std::string call(p);
        p += call.size() + 1;
        if (!call.empty())
            gCalls.push_back(std::move(call));
    }

    std::sort(gCalls.begin(), gCalls.end());
    gCalls.erase(std::unique(gCalls.begin(), gCalls.end()), gCalls.end());
}

std::string pickCall() {
    if (gCalls.empty()) {
        // Reload fallback if exhausted
        for (auto* c : kBuiltinCalls) gCalls.emplace_back(c);
    }

    int idx = std::rand() % static_cast<int>(gCalls.size());
    std::string result = gCalls[idx];

    if (Ini::instance().runMode == RunMode::Hst)
        gCalls.erase(gCalls.begin() + idx);

    return result;
}
