#include "MMC5983MA.h"
#include <cinttypes>

#define MAG_DEBUG 1

/**
 * @brief Construct a new MMC5983MA Base::MMC5983MA Base object
 */
MMC5983MA_Base::MMC5983MA_Base()
{
}

void MMC5983MA_Base::enableTemp(bool enable)
{
    tempEnable = enable;
}

/**
 * @brief Construct a new MMC5983MA_I2C::MMC5983MA_I2C object
 * The I2C address is 0x30 (see DATA TRANSFER section, pg 12, paragraph 2)
 * 
 * @param sda
 * @param scl
 * @param address
 */
MMC5983MA_I2C::MMC5983MA_I2C(PinName sda, PinName scl, uint8_t address) : 
    i2c(sda, scl), addr(address)
{
    //TODO
}

/**
 * @brief Construct a new MMC5983MA_SPI::MMC5983MA_SPI object
 * 
 * @param mosi
 * @param miso
 * @param sclk
 * @param ssel
 */
MMC5983MA_SPI::MMC5983MA_SPI(PinName mosi, PinName miso, PinName sclk, PinName ssel) : 
    spi(mosi, miso, sclk, ssel, use_gpio_ssel)
{
    /**
     * Mode	    Polarity	Phase
     * 0	    0	        0
     * 1	    0	        1
     * 2	    1	        0
     * 3	    1	        1
     * 
     * 00 --> clk active low, sampling on pos edge
     * 01 --> clk active low, samling on neg edge
     * 10 --> clock active high, sampling on neg edge
     * 11 --> clock active high, sampling on pos edge
     */
    spi.format(8, 0);                   // Datasheet says to use mode 3... but that doesn't work lol. Mode 0 is what worked!
    spi.set_default_write_value(0x1F);  // Setting the default write value to 0x1F because it's a unused register value and a 0x0 dummy byte also writes to Xout0
    spi.frequency(100'000);          // SPI INTERFACE I/O CHARACTERISTICS section
}

bool MMC5983MA_I2C::init()
{
    //TODO
    return false;
}

void MMC5983MA_I2C::readMagData()
{
    //TODO
    return;
}

void MMC5983MA_I2C::readRegisterI2C(Register reg, char* data, uint8_t numOfRead)
{
    //TODO
    return;
}

void MMC5983MA_I2C::writeRegisterI2C(Register reg, char* data, uint8_t numOfWrite)
{
    //TODO
    return;
}

bool MMC5983MA_SPI::init()
{
    // Read the product ID
    char data[1];
    readRegisterSPI(Register::ProductID_1, data, 1);

    if(MAG_DEBUG)
    {
        printf("data: %x\n", data[0]);
    }

    // If we read 0x30, we successfully init'd (properly read the product ID)
    // Note that 0x30 is both the prod. ID and i2c address
    if(data[0] == 0x30)
    {
        return true;
    }

    // Otherwise, we failed to init
    return false;
}

void MMC5983MA_SPI::readMagData()
{
    // The Meas_M_Done bit should be checked before reading the output. (first bit)
    // readStatusSPI();
    
    // When the new measurement command is occurred, the Meas_M_Done bit turns to 0
    uint8_t toSend = 0b0000'0000;
    writeRegisterSPI(Register::InternalControl_0, toSend | MASK_TM_M, 1);
    
    // The Meas_M_Done bit should be checked before reading the output. 
    // When the new measurement command is occurred, this bit turns to 0. When
    // the measurement is finished, this bit will remain 1 till next measurement.
    char data[7];
    readRegisterSPI(Register::Xout0, data, 7);

    // Writing “1” will let the device to read the OTP data again. This bit will be automatically reset to 0
    // after the shadow registers for OTP are refreshed. (Pg. 15)
    // writeRegisterSPI(Register::InternalControl_0, toSend | MASK_OTP_Read, 1);

    int32_t temp1 = ((static_cast<uint32_t>(data[0]) << 10) | (static_cast<uint16_t>(data[1]) << 2) | ((static_cast<uint8_t>(data[6]) >> 6) & 0x3));
    int32_t temp2 = ((static_cast<uint32_t>(data[2]) << 10) | (static_cast<uint16_t>(data[3]) << 2) | ((static_cast<uint8_t>(data[6]) >> 4) & 0x3));
    int32_t temp3 = ((static_cast<uint32_t>(data[4]) << 10) | (static_cast<uint16_t>(data[5]) << 2) | ((static_cast<uint8_t>(data[6]) >> 2) & 0x3));

    // convert to float and multiply by 0.0625 to get magnetic field in milliGauss
    mag18.x = static_cast<float>(temp1) * 0.0625;
    mag18.y = static_cast<float>(temp2) * 0.0625;
    mag18.z = static_cast<float>(temp3) * 0.0625;

    printf("X: %f, Y: %f, Z: %f\n", mag18.x, mag18.y, mag18.z);

    // readInternalControl_0SPI();

    return;
}

void MMC5983MA_SPI::readRegisterSPI(Register reg, char* data, uint8_t numOfRead)
{
    // Be aware that if we use async transfers, we need to use the CacheAlignedBuffer
    // You can declare it like this:
    // CacheAlignedBuffer<uint8_t, 2> response;

    // Must select the SPI object to bring the CS low
    spi.select();
    // bitwise OR with 0x80 because 8th bit being a 1 indiciates it being a read. whilst a 0 indicates it being a write
    spi.write(0x80 | static_cast<uint8_t>(reg));

    spi.write(nullptr, 0, data, numOfRead);

    // Deselecting the SPI object to bring the CS back high
    spi.deselect();

    return;
}

void MMC5983MA_SPI::writeRegisterSPI(Register reg, uint8_t toSend, uint8_t numOfReads)
{
    // bit 0: WRITE bit. The value is 0.
    // bit 1: don’t care
    // bit 2 -7: address AD(5:0). This is the address field of the indexed register.
    uint8_t txData[2] = {
        static_cast<uint8_t>(reg),
        toSend
    };

    spi.write(txData, 2, nullptr, 0);
}

void MMC5983MA_SPI::readStatusSPI()
{
    char status[1];
    readRegisterSPI(Register::Status, status, 1);

    if(MAG_DEBUG)
    {
        printf("status: %x\n", *reinterpret_cast<uint8_t*>(status));
    }
    return;
}



void MMC5983MA_SPI::readTempSPI()
{
    // check value first
    readStatusSPI();
    uint8_t toSend = 0b0000'0000;
    toSend = toSend | MASK_TM_T;

    if(MAG_DEBUG)
    {
        printf("toSend: %x \n", toSend);
    }

    writeRegisterSPI(Register::InternalControl_0, toSend, 1);

    readInternalControl_0SPI();
    
    char temp[1];
    readRegisterSPI(Register::Tout, temp, 1);

    if(MAG_DEBUG)
    {
        printf("temp: %x\n", *reinterpret_cast<uint8_t*>(temp));
    }

return;
}

void MMC5983MA_SPI::readInternalControl_0SPI()
{
    char internal[1];
    readRegisterSPI(Register::InternalControl_0, internal, 1);

    if(MAG_DEBUG)
    {
        printf("internal: %x\n", internal[0]);
    }
}

void MMC5983MA_SPI::SET_RESET() 
{
    // set
    uint8_t toSend = 0b0000'0000;
    toSend = toSend | MASK_Set;
    writeRegisterSPI(Register::InternalControl_0, toSend, 1);
    // reset
    toSend = 0b0000'0000;
    toSend = toSend | MASK_Reset;
    writeRegisterSPI(Register::InternalControl_0, toSend, 1);

    // Writing “1” will let the device to read the OTP data again. This bit will be automatically reset to 0
    // after the shadow registers for OTP are refreshed. (Pg. 15)
    // writeRegisterSPI(Register::InternalControl_0, toSend | MASK_OTP_Read, 1);

    if(MAG_DEBUG)
    {
        printf("MMC5983 Successfully Reset.\n");
    }
}