#pragma once

#include "../../simulator/gsp.h"
#include "libprintf.h"
#include "libos_log.h"

class serial_virtual
{
    struct LibosStream base = { &serial_virtual::putc };

    gsp * processor;
    LibosDebugResolver * resolver;
    LibosElfImage           elfImage;
    LwU64 mmio;
    std::function<void  (LwU64 addr, LwU32 value)> priv_write_raw;
    std::function<LwU32 (LwU64 addr)> priv_read_raw;
    
    libosLogMetadata * metadata = 0;
    std::vector<LibosPrintfArgument> arguments;

    static int putc(struct LibosStream * stream, char ch) {
        printf("%c", ch);
        return 0;
    }
    
    static int printPointer(struct LibosStream * stream, void * p) 
    {
        serial_virtual * pThis= (serial_virtual *)stream;

        const char * name, * file, * dir;
        LwU64 offset = 0, line, col;
        LwBool hasSymbol = LibosDebugResolveSymbolToName(pThis->resolver, (LwU64)p, &name, &offset);
        LwU64 matched;
        LwBool hasLine = LibosDwarfResolveLine(pThis->resolver, (LwU64)p, &dir, &file, &line, &col, &matched);

        if (hasSymbol && hasLine)
            printf("%08llx %s+%d (%s:%d)", p, name, offset, file, line);
        else if (hasSymbol)
            printf("%08llx %s+%d", p, name, offset);
        else if (hasLine)
            printf("%08llx %s:%d", p, file, line);
        else
            printf("%08llx", p);
        return 0;
    }

    LwU32  previous = 0;
    LwBool pending = LW_FALSE;
    void priv_write(LwU64 offset, LwU32 value) 
    {
        if (offset != mmio) {
            priv_write_raw(offset, value); 
            return;
        }

        if (pending) {
            LwU64 v = value | (((LwU64)previous) << 32);        
            pending = LW_FALSE;

            if (!metadata)
                metadata = (libosLogMetadata *)LibosElfMapVirtual(&elfImage, v, sizeof(libosLogMetadata));
            else
            {
                if (arguments.size() < metadata->argumentCount)
                {
                    LibosPrintfArgument arg = {.i = v }; 
                    arguments.push_back(arg);
                }
            }

            if (arguments.size() == metadata->argumentCount)
            {
                LibosTokenPrintf(&this->base, printPointer, LibosElfMapVirtualString(&elfImage, (LwU64)metadata->format), arguments.size() ? &arguments[0] : 0, arguments.size());
                metadata = 0;
                arguments.clear();
            }
        }
        else
        {
            if (!metadata && (value & 0x80000000)) {
                previous = value & 0x7FFFFFFF;
                pending = LW_TRUE;
            }
            else if (metadata) {
                previous = value;
                pending = LW_TRUE;
            }
            else 
            {
                printf("%c", value & 0xFF);
                return;                
            }
        }
    }


    LwU32 priv_read(LwU64 offset) 
    {
        if (offset == mmio) 
            return 0;
        else
            return priv_read_raw(offset);
    }
public:
    serial_virtual(gsp * processor, LwU64 mmio, image * image)
        : processor(processor), mmio(mmio), resolver(&image->resolver), elfImage(image->elfImage)
    {
        priv_read_raw = processor->priv_read_HAL;
        priv_write_raw = processor->priv_write_HAL;

        processor->priv_write_HAL = bind_hal(&serial_virtual::priv_write, this);
        processor->priv_read_HAL  = bind_hal(&serial_virtual::priv_read, this);
    }

};
