#pragma once

#include "../../simulator/gsp.h"

class serial_virtual
{
    gsp * processor;
    LwU64 mmio;
    std::function<void  (LwU64 addr, LwU32 value)> priv_write_raw;
    std::function<LwU32 (LwU64 addr)> priv_read_raw;

    void priv_write(LwU64 offset, LwU32 value) 
    {
        if (offset == mmio) 
            printf("%c", value & 0xFF);
        else
            priv_write_raw(offset, value);
    }

    LwU32 priv_read(LwU64 offset) 
    {
        if (offset == mmio) 
            return 0;
        else
            return priv_read_raw(offset);
    }
public:
    serial_virtual(gsp * processor, LwU64 mmio)
        : processor(processor), mmio(mmio)
    {
        priv_read_raw = processor->priv_read_HAL;
        priv_write_raw = processor->priv_write_HAL;

        processor->priv_write_HAL = bind_hal(&serial_virtual::priv_write, this);
        processor->priv_read_HAL  = bind_hal(&serial_virtual::priv_read, this);
    }

};
