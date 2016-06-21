# Erasure Coding NIC Offload library
Erasure coding (EC) is a method of data protection in which data is broken into fragments,  
expanded and encoded with redundant data pieces and stored across a set of different locations or storage media.  
Data encoding/decoding is very CPU intensive and can be a major overhead when using Erasure coding.  
By using Mellanox EC Offload library, the calculation is done by the HCA which reduce dramatically the CPU consumption.  
Erasure Coding NIC Offload library performs Erasure Coding calculations in GF(2^4).  
This library also contains a plugin for Hadoop Distributed File System (HDFS).

### Prerequisites
1. Mellanox ConnectX®-4 or ConnectX®-4 Lx.
2. MLNX_OFED_LINUX-3.3-1.0.0.0 (or later).
2. Firmware - v12.16.1006 for ConnectX®-4 (or later), v14.16.1006 for ConnectX®-4 Lx  (or later).
3. Jerasure library v2.0. (https://github.com/tsuraan/Jerasure).

### Recommendations
1. install Jerasure & GF-Complete using the following parameters:  
    
        ./configure --prefix=/usr/ --libdir=/usr/lib64/
2. It is highly recommended to install ConnectX®-4 on PCIe3.0 x16, and ConnectX®-4 Lx on PCIe3.0 x8 slot for better performance.

### Limitations
1. Buffers must be align to 64 bytes.
2. Thread safety - Single thread per encoder/decoder.
3. Using mlx5_0 device as default.

### Build
1. make
2. sudo make install

### Tests

**Build**  
1. cd tests  
2. make

**ibv_ec_capability_test**

Checking EC offload capabilities for IB devices.

*Usage*  

        ./ibv_ec_capability_test
        Usage = ./ec_capability_test <device_name>
        Available devices : <List of available IB devices>

**ibv_ec_encoder**

Perform encode operations on input files using Erasure Coding NIC Offload library, Erasure Coding Offload
Experimental Verbs API and Jerasure library.  
Galois field GF(2^w) must be equal to GF(2^4).  
The test will encode the input file three times (all the results should be equal):
1. using Erasure Coding NIC Offload library - the results will be written into <inputFile>.encode.code.eco
2. using Experimental Verbs API - the results will be written into <inputFile>.encode.code.verbs
3. using Jerasure Library - the results will be written into <inputFile>.encode.code.sw

*Usage*  

    Usage:
    ./ibv_ec_encoder            start EC encoder
    
    Options:
      -i, --ib-dev=<dev>         use IB device <dev> (default first device found)
      -k, --data_blocks=<blocks> Number of data blocks
      -m, --code_blocks=<blocks> Number of code blocks
      -w, --gf=<gf>              Galois field GF(2^w)
      -D, --datafile=<name>      Name of input file to encode
      -s, --frame_size=<size>    size of EC frame
      -d, --debug                print debug messages
      -v, --verbose              add verbosity
      -h, --help                 display this output

**ibv_ec_decoder**

Perform decode operations on input files using Erasure Coding NIC Offload library and Erasure Coding Offload
Experimental Verbs API.  
Galois field GF(2^w) must be equal to GF(2^4).  
The test will decode the input file two times (The code result should be equal to the input code file, data results should be equal to the input file):
1. using  Erasure Coding NIC Offload library - the data results will be written into <inputFile>.decode.data.eco  
                                               the code results will be written into <inputFile>.decode.code.eco
2. using Experimental Verbs API - the data results will be written into <inputFile>.decode.data.verbs

*Usage*  

    Usage:
      ./ibv_ec_decoder            start EC decoder
    
    Options:
      -i, --ib-dev=<dev>         use IB device <dev> (default first device found)
      -k, --data_blocks=<blocks> Number of data blocks
      -m, --code_blocks=<blocks> Number of code blocks
      -w, --gf=<gf>              Galois field GF(2^w)
      -D, --datafile=<name>      Name of input data file
      -C, --codefile=<name>      Name of input code file
      -E, --erasures=<erasures>  Comma separated failed blocks
      -s, --frame_size=<size>    size of EC frame
      -d, --debug                print debug messages
      -v, --verbose              add verbosity
      -h, --help                 display this output

