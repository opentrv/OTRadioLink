/***************************************************************************//**
 * @file
 * @brief I2C simple poll-based master mode driver for the DK/STK.
 * @version 5.1.1
 *******************************************************************************
 * @section License
 * <b>Copyright 2015 Silicon Labs, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/
#if 1
extern "C" {
#include <stddef.h>
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_assert.h"
}
#include "i2c_driver.h"

// Global i2c instance on bus 0
 I2CSPM i2c0;


/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

void I2C0_enter_DefaultMode_from_RESET() {

	// $[I2C0 I/O setup]
	/* Set up SCL */
	I2C0->ROUTEPEN = I2C0->ROUTEPEN | I2C_ROUTEPEN_SCLPEN;
	I2C0->ROUTELOC0 = (I2C0->ROUTELOC0 & (~_I2C_ROUTELOC0_SCLLOC_MASK))
			| I2C_ROUTELOC0_SCLLOC_LOC14;
	/* Set up SDA */
	I2C0->ROUTEPEN = I2C0->ROUTEPEN | I2C_ROUTEPEN_SDAPEN;
	I2C0->ROUTELOC0 = (I2C0->ROUTELOC0 & (~_I2C_ROUTELOC0_SDALOC_MASK))
			| I2C_ROUTELOC0_SDALOC_LOC16;
	// [I2C0 I/O setup]$

	// $[I2C0 initialization]
	I2C_Init_TypeDef init = I2C_INIT_DEFAULT;

	init.enable = 1;
	init.master = 1;
	init.freq = I2C_FREQ_STANDARD_MAX;
	init.clhr = i2cClockHLRStandard;
	I2C_Init(I2C0, &init);
	// [I2C0 initialization]$

}



/***************************************************************************//**
 * @brief
 *   Initalize I2C peripheral
 *
 * @details
 *   This driver supports master mode only, single bus-master. In addition
 *   to configuring the I2C peripheral module, it also configures DK/STK
 *   specific setup in order to use the I2C bus.
 *
 * @param[in] init
 *   reference to I2C initialization structure
 ******************************************************************************/
void I2CSPM::init()
{
    // Reset the peripheral.
    I2C0_enter_DefaultMode_from_RESET();

    int i;
    CMU_Clock_TypeDef i2cClock;
    I2C_Init_TypeDef i2cInit;

    CMU_ClockEnable(cmuClock_HFPER, true);

    /* Select I2C peripheral clock */
    if (false) {
#if defined( I2C0 )
  }
  else if (config.port == I2C0)
  {
    i2cClock = cmuClock_I2C0;
#endif
#if defined( I2C1 )
  }
  else if (config.port == I2C1)
  {
    i2cClock = cmuClock_I2C1;
#endif
  }
  else
  {
    /* I2C clock is not defined */
    EFM_ASSERT(false);
    return;
  }
  CMU_ClockEnable(i2cClock, true);

  /* Output value must be set to 1 to not drive lines low. Set
     SCL first, to ensure it is high before changing SDA. */
  GPIO_PinModeSet(config.sclPort, config.sclPin, gpioModeWiredAndPullUp, 1);
  GPIO_PinModeSet(config.sdaPort, config.sdaPin, gpioModeWiredAndPullUp, 1);

  /* In some situations, after a reset during an I2C transfer, the slave
     device may be left in an unknown state. Send 9 clock pulses to
     set slave in a defined state. */
  for (i = 0; i < 9; i++)
  {
    GPIO_PinOutSet(config.sclPort, config.sclPin);
    GPIO_PinOutClear(config.sclPort, config.sclPin);
  }

  /* Enable pins and set location */
#if defined (_I2C_ROUTEPEN_MASK)
  config.port->ROUTEPEN  = I2C_ROUTEPEN_SDAPEN | I2C_ROUTEPEN_SCLPEN;
  config.port->ROUTELOC0 = (config.portLocationSda << _I2C_ROUTELOC0_SDALOC_SHIFT)
                          | (config.portLocationScl << _I2C_ROUTELOC0_SCLLOC_SHIFT);
#else
  config.port.ROUTE = I2C_ROUTE_SDAPEN |
                      I2C_ROUTE_SCLPEN |
                      (config.portLocation << _I2C_ROUTE_LOCATION_SHIFT);
#endif

  /* Set emlib init parameters */
  i2cInit.enable = true;
  i2cInit.master = true; /* master mode only */
  i2cInit.freq = config.i2cMaxFreq;
  i2cInit.refFreq = config.i2cRefFreq;
  i2cInit.clhr = config.i2cClhr;

  I2C_Init(config.port, &i2cInit);
}

/**
 * @brief   Read from an I2C device.
 *          This driver works by writing and then reading. Haven't worked out how to get a write then read to work yet (DE20170815)
 * @param   addr: 7 byte address for the i2c slave. Should be right adjusted (XAAAAAAA) as is internally shifted left.
 * @param   cmd: Pointer to buffer to write to slave. The buffer must exist until the transfer is completed.
 * @param   cmdLen: number of bytes in message to be written.
 * @param   rxBuf: Pointer to buffer to store the response from the slave. The buffer must exist until the transfer is completed.
 * @param   rxLen: number of bytes in message to be written.
 */
I2C_TransferReturn_TypeDef I2CSPM::read(uint16_t addr, uint8_t * const cmd, uint16_t cmdLen, uint8_t * const rxBuf, uint16_t rxLen)
{
    if(nullptr == cmd || 0 == cmdLen) { return (i2cTransferUsageFault); }
    if(nullptr == rxBuf || 0 == rxLen) { return (i2cTransferUsageFault); }

    I2C_TransferSeq_TypeDef seq;
    seq.addr = (addr << 1);
    seq.flags = I2C_FLAG_WRITE_READ;
    // First this command is transmitted
    seq.buf[0].data = cmd;
    seq.buf[0].len  = cmdLen;
    // Then the response is placed in this buffer
    seq.buf[1].data = rxBuf;
    seq.buf[1].len  = rxLen;

    return (i2c0.transfer(seq));
}


/**
 * @brief   Write to an I2C device.
 * @param   addr: 7 byte address for the i2c slave. Should be right adjusted (XAAAAAAA) as is internally shifted left.
 * @param   cmd: Pointer to buffer to write to slave. The buffer must exist until the transfer is completed.
 * @param   len: number of bytes in message to be written.
 */
I2C_TransferReturn_TypeDef I2CSPM::write(uint16_t addr, uint8_t * const cmd, uint16_t len)
{
    if(nullptr == cmd || 0 == len) { return (i2cTransferUsageFault); }

    I2C_TransferSeq_TypeDef seq;
    seq.addr = (addr << 1);
    seq.flags = I2C_FLAG_WRITE;
    seq.buf[0].data = cmd;
    seq.buf[0].len  = len;

    return (i2c0.transfer(seq));
}


/***************************************************************************//**
 * @brief
 *   Perform I2C transfer
 *
 * @details
 *   This driver only supports master mode, single bus-master. It does not
 *   return until the transfer is complete, polling for completion.
 *
 * @param[in] seq
 *   Reference to sequence structure defining the I2C transfer to take place. The
 *   referenced structure must exist until the transfer has fully completed.
 ******************************************************************************/
I2C_TransferReturn_TypeDef I2CSPM::transfer(I2C_TransferSeq_TypeDef &seq)
{
    // config.port is already a pointer.
    I2C_TypeDef *i2c = config.port;
    I2C_TransferReturn_TypeDef ret;
    uint32_t timeout = I2CSPM_TRANSFER_TIMEOUT;
    /* Do a polled transfer */
    ret = I2C_TransferInit(i2c, &seq);
    while (ret == i2cTransferInProgress && timeout--) {
        ret = I2C_Transfer(i2c);
    }
    return ret;
}
#endif
