#include <stdio.h>
#include <stdlib.h>
#include <string.h>


bool verbose = false;

static void parseDebug(char *traceFilename, FILE *outputFile)
{
    char buf[500];
    char tmp[100], fmt[100];
    char *sub, *fmt_ptr=fmt;
    unsigned int adr, val, param[4];
    int state = -1;

    FILE* traceFile = fopen(traceFilename, "r");
    if(!traceFile) {
        printf("Cannot find trace file %s\n", traceFilename);
        exit(-1);
    }
    //    stdCHECK( traceFile, (msgOpenInputFailed, traceFilename) );  

    while(fgets(buf, 500, traceFile)) {
        buf[499] = '\0';
        //
        // SEC2 uses LW_CSEC_CAP_REG3(0x00020400), as a debug register
        // to store print logs in fmodel. Hence the same register is
        // grepped below to decode the prints.
        //
        if(strstr(buf, "STX") && (sub=strstr(buf, "0x00020400")) ) {
            if(verbose) fprintf(outputFile, "%s", buf);
            sscanf(sub, "%x}, %s {%x}\n", &adr, tmp, &val);
            //            printf("find target: %s, val=0x%08x\n", buf, val);
            switch(state){
            case -1 : if( (val >> 16) == 0xbeef ) state = 0; break; // start of the msg
            case 0  : 
            case 1  : 
            case 2  : 
            case 3  : param[state] = val; state++; break;  // 4 params
            case 4  : {
                for(int i=0;i<4; i++){
                    *fmt_ptr = (val & 0xFF);
                    val >>= 8;
                    if(*fmt_ptr=='\0') {
                        fprintf(outputFile, fmt, param[0], param[1], param[2], param[3]);
                        fmt_ptr = fmt;
                        state = -1;
                        break;
                    }
                    else fmt_ptr++;
                }
                break;
            }
            }
        }
    }
    fclose(traceFile);
}

int main(int argc, char **argv) {
    if(argc<2 || argc>3) {
        printf("Usage: falc_debug [-v] <trace file>\n");
        exit(-1);
    }
    if( argc==3 && (strcmp(argv[1], "-v")==0) ){
        verbose = true;
        parseDebug(argv[2], stdout);
    }
    else parseDebug(argv[1], stdout);
}
