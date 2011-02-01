//
// bpacketsync_example.c
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "liquid.h"

int callback(unsigned char * _payload,
             int _payload_valid,
             unsigned int _payload_len,
             void * _userdata)
{
    printf("callback invoked, payload (%u bytes) : %s\n",
            _payload_len,
            _payload_valid ? "valid" : "INVALID!");

    // copy data if valid
    if (_payload_valid) {
        unsigned char * msg_dec = (unsigned char*) _userdata;

        memmove(msg_dec, _payload, _payload_len*sizeof(unsigned char));
    }

    return 0;
}

// print usage/help message
void usage()
{
    unsigned int i;
    printf("bpacketsync_example [options]\n");
    printf("  u/h   : print usage\n");
    printf("  n     : input data size (number of uncoded bytes): 8 default\n");
    printf("  d     : received sequence delay [bits], default: 13\n");
    printf("  v     : data integrity check: crc32 default\n");
    // print all available CRC schemes
    for (i=0; i<LIQUID_NUM_CRC_SCHEMES; i++)
        printf("          [%s] %s\n", crc_scheme_str[i][0], crc_scheme_str[i][1]);
    printf("  c     : coding scheme (inner): h74 default\n");
    printf("  k     : coding scheme (outer): none default\n");
    // print all available FEC schemes
    for (i=0; i<LIQUID_NUM_FEC_SCHEMES; i++)
        printf("          [%s] %s\n", fec_scheme_str[i][0], fec_scheme_str[i][1]);
}


int main(int argc, char*argv[]) {
    // options
    unsigned int n=8;                   // original data message length
    crc_scheme check = CRC_32;          // data integrity check
    fec_scheme fec0 = FEC_HAMMING128;   // inner code
    fec_scheme fec1 = FEC_NONE;         // outer code
    unsigned int delay = 13;            // number of bits in delay

    // read command-line options
    int dopt;
    while((dopt = getopt(argc,argv,"uhn:d:v:c:k:")) != EOF){
        switch (dopt) {
        case 'h':
        case 'u': usage(); return 0;
        case 'n': n = atoi(optarg);     break;
        case 'd': delay = atoi(optarg); break;
        case 'v':
            // data integrity check
            check = liquid_getopt_str2crc(optarg);
            if (check == CRC_UNKNOWN) {
                fprintf(stderr,"error: unknown/unsupported CRC scheme \"%s\"\n\n",optarg);
                exit(1);
            }
            break;
        case 'c':
            // inner FEC scheme
            fec0 = liquid_getopt_str2fec(optarg);
            if (fec0 == FEC_UNKNOWN) {
                fprintf(stderr,"error: unknown/unsupported inner FEC scheme \"%s\"\n\n",optarg);
                exit(1);
            }
            break;
        case 'k':
            // outer FEC scheme
            fec1 = liquid_getopt_str2fec(optarg);
            if (fec1 == FEC_UNKNOWN) {
                fprintf(stderr,"error: unknown/unsupported outer FEC scheme \"%s\"\n\n",optarg);
                exit(1);
            }
            break;
        default:
            fprintf(stderr,"error: unknown/invalid option\n");
            exit(1);
        }
    }

    // validate input
    if (n == 1) {
        fprintf(stderr,"error: %s, packet length must be greater than zero\n", argv[0]);
        exit(1);
    }

    // create packet generator
    bpacketgen pg = bpacketgen_create(0, n, check, fec0, fec1);
    bpacketgen_print(pg);

    unsigned int i;

    // compute packet length
    unsigned int k = bpacketgen_get_packet_len(pg);

    unsigned int dbytes = delay / 8;    // number of bytes in delay
    unsigned int dbits = delay % 8;     // number of additional bits in delay
    unsigned int w = k + dbytes + (dbits != 0);

    // initialize arrays
    unsigned char msg_org[n];   // original message
    unsigned char msg_enc[k];   // encoded message
    unsigned char msg_rec[w];   // recieved message
    unsigned char msg_dec[n];   // decoded message

    // create packet synchronizer
    bpacketsync ps = bpacketsync_create(0, callback, (void*)msg_dec);

    // initialize original data message
    for (i=0; i<n; i++)
        msg_org[i] = rand() % 256;

    // encode packet
    bpacketgen_encode(pg,msg_org,msg_enc);

    // add delay and error(s)
    for (i=0; i<dbytes; i++)
        msg_rec[i] = rand() & 0xff;
    liquid_rmemmove(&msg_rec[dbytes], msg_enc, k*sizeof(unsigned char), dbits);
#if 0
    // add random errors
    for (i=0; i<w; i++) {
        if (randf() < 0.02f)
            msg_rec[i] ^= 1 << (rand()%8);
    }
#endif

    // run packet synchronizer
    for (i=0; i<w; i++)
        bpacketsync_execute(ps, msg_rec[i]);

    // count errors
    unsigned int num_bit_errors = 0;
    for (i=0; i<n; i++)
        num_bit_errors += count_bit_errors(msg_org[i], msg_dec[i]);
    printf("number of bit errors received:    %4u / %4u\n", num_bit_errors, n*8);

    // clean up allocated objects
    bpacketgen_destroy(pg);
    bpacketsync_destroy(ps);

    return 0;
}
