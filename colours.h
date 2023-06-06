#ifndef FRACTALFUN_COLOURS_H
#define FRACTALFUN_COLOURS_H

#include <bit>

#include <cstdint>
#include <cstddef>

struct ThreeColour {
    uint8_t blue, green, red;
};

class Colour {
private:
    const struct {
        uint8_t redVal;
        uint8_t greenVal;
        uint8_t blueVal;
        uint8_t alphaVal;
    } c_struct ;
    const uint32_t packedVal;

public:
    [[nodiscard]] uint8_t red()     const {return c_struct.redVal;};
    [[nodiscard]] uint8_t green()   const {return c_struct.blueVal;};
    [[nodiscard]] uint8_t blue()    const {return c_struct.greenVal;};
    [[nodiscard]] uint8_t alpha()   const {return c_struct.alphaVal;};
    [[nodiscard]] uint32_t packed() const {return packedVal;};

    Colour(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) : c_struct{red, green, blue, alpha}, packedVal(std::bit_cast<uint32_t>(c_struct)) {};
    explicit Colour(uint32_t packed) : c_struct(std::bit_cast<decltype(c_struct)>(packed)), packedVal{packed} {};
};

const Colour inside_colour{0x00, 0x00, 0x00, 0xFF}; //Black

#endif //FRACTALFUN_COLOURS_H
