/*
 * Copyright (c) 2012, Jens Nielsen
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the <organization> nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL JENS NIELSEN BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ds18b20.h"
#include "ownet.h"
#include "owlink.h"
#include <string.h> /*for memset*/
#include <stdint.h>
#include <stdbool.h>

#define MAX_NUM_DEVICES 1

typedef struct
{
    uint8_t serial[ 8 ];
    int16_t lastVal;
    float lastTemp;
}device_t;

static device_t devices[ MAX_NUM_DEVICES ];
static uint8_t attachedDevices = 0;
static ds18b20_resolution_t resolution = DS18B20_RESOLUTION_12BIT;
static bool updateResolution = false;

typedef enum state_e
{
    STATE_SCAN,
    STATE_CONVERT,
    STATE_WAIT_CONVERT,
    STATE_FETCH_TEMPS,
    STATE_DONE,
}state_t;

static state_t state;


static void ds18b20_scan(void);
static bool ds18b20_isParasite(void);
static void ds18b20_startConvert(void);
static void ds18b20_fetchTemp( uint8_t device );
static void ds18b20_configureResolution(ds18b20_resolution_t resolution);

void ds18b20_init(void)
{
    memset( devices, 0, sizeof(devices) );
    attachedDevices = 0;

    state = STATE_SCAN;
}

bool ds18b20_work(void)
{
    static uint8_t fetchDevice = 0;
    switch ( state )
    {
        case STATE_SCAN:
            ds18b20_scan();
            if ( attachedDevices > 0 )
            {
                state = STATE_CONVERT;
            }
            break;

        case STATE_CONVERT:
            if (updateResolution)
            {
                ds18b20_configureResolution(resolution);
                updateResolution = false;
            }
            ds18b20_startConvert();
            state = STATE_WAIT_CONVERT;
            break;

        case STATE_WAIT_CONVERT:
            if ( owReadBit() != 0 )
            {
                state = STATE_FETCH_TEMPS;
                fetchDevice = 0;
            }
            break;

        case STATE_FETCH_TEMPS:
            ds18b20_fetchTemp( fetchDevice );
            fetchDevice++;
            if ( fetchDevice >= attachedDevices )
            {
                state = STATE_CONVERT; //STATE_DONE for single conversion
                return true;
            }
            break;

        case STATE_DONE:
            return true;
    }
    return false;
}

float ds18b20_getTemp( uint8_t device )
{
    if ( device < attachedDevices)
    {
        return devices[ device ].lastTemp;
    }
    else
    {
        return 0;
    }
}

static void ds18b20_scan(void)
{
    uint8_t found;
    uint8_t i;

    attachedDevices = 0;
    found = owFirst( 0, 1, 0 );
    if ( found )
    {
        attachedDevices++;
        owSerialNum( 0, devices[ 0 ].serial, 1 );
    }

    for ( i = 1; found && i < MAX_NUM_DEVICES; i++ )
    {
        found = owNext( 0, 1, 0 );
        if ( found )
        {
            attachedDevices++;
            owSerialNum( 0, devices[ i ].serial, 1 );
        }
    }
}

static bool ds18b20_isParasite(void)
{
    /*issue Read Power Supply to all devices, any parasite powered devices will pull bus low*/
    owTouchReset();
    owWriteByte(0xCC);
    owWriteByte(0xB4);
    return ( owReadBit() == 0 );
}

static void ds18b20_startConvert(void)
{
    /*issue convert temp to all devices*/
    owTouchReset();
    owWriteByte(0xCC);
    owWriteByte(0x44);
}

static void ds18b20_fetchTemp( uint8_t device )
{
    if ( device < attachedDevices && devices[ device ].serial[ 0 ] != 0 )
    {
        uint8_t i;
        uint8_t b1, b2;

        owTouchReset();
#if MAX_NUM_DEVICES == 1
        /*only one device allowed, we can save time by sending skip rom*/
        owWriteByte(0xCC);
#else
        /*address specified device*/
        owWriteByte(0x55);
        for (i = 0; i < 8; i++)
        {
            owWriteByte(devices[ device ].serial[ i ]);
        }
#endif

        /*read the first two bytes of scratchpad*/
        owWriteByte(0xBE);
        b1 = owReadByte();
        b2 = owReadByte();

        devices[ device ].lastVal = ( (int16_t) b2 << 8 ) | ( b1 & 0xFF );
        devices[ device ].lastTemp = devices[ device ].lastVal * 0.0625;
    }
}

//for debug
volatile uint8_t scratchpad[5];
void ds18b20_readScratchpad(void)
{
    uint8_t i;
    owTouchReset();
    owWriteByte(0xCC);
    owWriteByte(0xBE);
    for (i = 0; i < 5; i++)
    {
        scratchpad[i] = owReadByte();
    }
}

void ds18b20_setResolution(ds18b20_resolution_t newResolution)
{
    resolution = newResolution;
    updateResolution = true;
}

static void ds18b20_configureResolution(ds18b20_resolution_t resolution)
{
    /*issue write scratchpad to all devices*/
    owTouchReset();
    owWriteByte(0xCC);
    owWriteByte(0x4E);

    owWriteByte(0x00);
    owWriteByte(0x00);
    owWriteByte(resolution << 5);
}
