/**
 * @file MMC5983MA.h
 * @author Reo Tseng
 * @brief Driver for the MMC5983MA magnetometer
 * @date 2023-12-31
 * 
 * 
 */

#include <mbed.h>

#ifndef MMC5983MA_H
#define MMC5983MA_H

struct mag_data_16
{
    int16_t x;
    int16_t y;
    int16_t z;
};

/**
 * @brief The reason that this struct exists is because the MMC5983MA
 * outputs a maximum of 18 bits of data per axis (and ideally we'd want more
 * data because of precision purposes). So we need to store the data in a
 * 32 bit integer, and then shift it to the right by 14 bits to get the
 * 18 bits of data that we want. This will be handled by internal functions;
 * the user can just use the mag_data_18 struct! :)
 * 
 */
struct mag_data_18
{
    float x;
    float y;
    float z;
};

class MMC5983MA_Base
{
    public:
        // The values in here are implicitly const!
        enum class Register : uint8_t
        {
            Xout0 = 0x00,               // Xout [17:10]
            Xout1 = 0x01,               // Xout [9:2]
            Yout0 = 0x02,               // Yout [17:10]
            Yout1 = 0x03,               // Yout [9:2]
            Zout0 = 0x04,               // Zout [17:10]
            Zout1 = 0x05,               // Zout [9:2]
            XYZout2 = 0x06,             // Xout[1:0], Yout[1:0], Zout[1:0]
            Tout = 0x07,                // Temperature output
            Status = 0x08,              // Device status
            InternalControl_0 = 0x09,   // Control register 0
            InternalControl_1 = 0x0a,   // Control register 1
            InternalControl_2 = 0x0b,   // Control register 2
            InternalControl_3 = 0x0c,   // Control register 3
            ProductID_1 = 0x2f          // Product ID
        };

        /**
         * @brief These masks are responsible for configuring the internalControl0 Register in the IC
         * 
         * Bit Name       | Description
         * ---------------|------------------------------------------------------------------------------------
         * TM_M           = Take magnetic field measurement, set ‘1’ will initiate measurement. 
         *                  This bit will be automatically reset to 0 at the end of each measurement.
         * TM_T           = Take Temperature measurement, set ‘1’ will initiate measurement. 
         *                  This bit will be automatically reset to 0 at the end of each measurement. 
         *                  This bit and TM_M cannot be high at the same time.
         * INT_meas_done  = Writing “1” will enable the interrupt for completed measurements. 
         *                  Once a measurement is finished, either magnetic field or temperature, 
         *                  an interrupt will be sent to the host.
         * Set            = Writing “1” will cause the chip to do the Set operation, 
         *                  which will allow large set current to flow through the sensor coils for 500ns. 
         *                  This bit is self-cleared at the end of Set operation.
         * Reset          = Writing “1” will cause the chip to do the Reset operation, 
         *                  which will allow large reset current to flow through the sensor coils for 500ns. 
         *                  This bit is self-cleared at the end of Reset operation.
         * Auto_SR_en     = Writing “1” will enable the feature of automatic set/reset.
         * OTP Read       = Writing “1” will let the device read the OTP data again. 
         *                  This bit will be automatically reset to 0 after the shadow registers for OTP are refreshed.
         */

        const uint8_t MASK_TM_M                = 0b0000'0001; // 0b00000000; 0x0
        const uint8_t MASK_TM_T                = 0b0000'0010; // 0x2
        const uint8_t MASK_INT_meas_done_en    = 0b0000'0100; // 0x4
        const uint8_t MASK_Set                 = 0b0000'1000; // 0x8
        const uint8_t MASK_Reset               = 0b0001'0000; // 0x10
        const uint8_t MASK_Auto_SR_en          = 0b0010'0000; // 0x20
        const uint8_t MASK_OTP_Read            = 0b0100'0000; // 1 << 7;

    public:
        /**
         * @brief Construct a new MMC5983MA Base object
         */
        MMC5983MA_Base();

        /**
         * @brief Init the magnetometer
         * 
         * @return true if success, false if failed
         */
        virtual bool init() = 0;

        virtual void readMagData() = 0;

        void enableTemp(bool enable);

    protected:
        bool tempEnable = 0;
        mag_data_16 mag16 = {0, 0, 0};
        mag_data_18 mag18 = {0, 0, 0};
        uint8_t tempReading = 0;
};

class MMC5983MA_I2C : public MMC5983MA_Base
{
    public:
        /**
         * @brief Constructs a new MMC5983MA_I2C object
         * 
         * @param sda data line
         * @param scl clock line
         * @param addr I2C address
         */
        MMC5983MA_I2C(PinName sda, PinName scl, uint8_t addr);

        /**
         * @brief Init the magnetometer
         * 
         * @return true if success, false if failed
         */
        bool init() override;

        /**
         * @brief Read magnetometer data
         * 
         * @param data 
         */
        void readMagData() override;

    protected:
        /**
         * @brief Read register
         * 
         * @param reg 
         * @param data The data to read into, must be at least numToRead bytes
         * @param numToRead The number of bytes to read
         */
        void readRegisterI2C(Register reg, char* data, uint8_t numOfRead);

        /**
         * @brief Write register
         * 
         * @param reg 
         * @param data The data to write, must be at least numToWrite bytes
         * @param numToWrite The number of bytes to write
         */
        void writeRegisterI2C(Register reg, char* data, uint8_t numOfWrite);
        
    private:
        I2C i2c;
        uint8_t addr;
};

class MMC5983MA_SPI : public MMC5983MA_Base
{
    public:
        /**
         * @brief Construct a new MMC5983MA_SPI object
         * 
         * @param mosi 
         * @param miso 
         * @param sclk 
         * @param ssel 
         */
        MMC5983MA_SPI(PinName mosi, PinName miso, PinName sclk, PinName ssel);

        /**
         * @brief Init the magnetometer
         * 
         * @return true if success, false if failed
         */
        bool init() override;

        /**
         * @brief Read magnetometer data
         * 
         * @param data 
         */
        void readMagData() override;

        /**
         * @brief Reads the status register
         */
        void readStatusSPI();

        /**
         * @brief Reads the temperature register
         */
        void readTempSPI();

        /**
         * @brief Read the internal control 0 register
         * 
         */
        void readInternalControl_0SPI();

        /**
         * @brief MMC5983 integrated SET/RESET function to restore/set the 
         * sensor characteristics
         */
        void SET_RESET();

    protected:
        /**
         * @brief Function to read from a register via SPI
         * 
         * @param reg 
         * @param data The data to read into, must be at least numToRead bytes
         * @param numToRead The number of bytes to read
         */
        void readRegisterSPI(Register reg, char* data, uint8_t numOfRead);

        /**
         * @brief Function to write to a register via SPI
         * 
         * @param reg 
         * @param data The data to write, must be at least numToWrite bytes
         * @param numToWrite The number of bytes to write 
         */
        void writeRegisterSPI(Register reg, uint8_t toSend, uint8_t numOfReads);

    private:
        SPI spi;
};

#endif // MMC5983MA_H