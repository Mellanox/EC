## HDFS Erasure Coding Offload Plugin
Hadoop Distributed File System (HDFS) by defaults using replication to store its blocks. namely, each given block is replicated several times and stored in the HDFS Datanodes. Replication provides a simple from to deal with most failure scenarios.  
Using Erasure Coding, we can reduce the storage overhead by approximately 50% while maintaining the same data durability.  
Erasure Coding operations are very CPU intensive actions and can be a major overhead compared to the replication approach.

### Prerequisites
1. Erasure Coding NIC Offload library.
2. Hadoop 3.0.0 based on commit 5b7078d.  
   including the following patches:  
        a. HADOOP-11996-v3.patch (https://issues.apache.org/jira/browse/HADOOP-11996).  
        b. HADOOP-11540-v2.patch (https://issues.apache.org/jira/browse/HADOOP-11540).  
        c. HDFS-8668-v2.patch (https://issues.apache.org/jira/browse/HDFS-8668).

### Limitations
1. Erasure Coding NIC Offload library limitations.
2. (Decode) The number of NULL input buffers must be equal to the number of the output buffers. 

### Installation and Usage
1. cd HDFS
2. ant build -DHADOOP_HOME=/path/to/hadoop/home/dir
3. Place your MellanoxECOffload JAR file on every node in the cluster (located in build/jar/MellanoxECOffload.jar) :  
The preferable location is where all the Hadoop Common JARs are already found:

        $HADOOP_HOME/share/hadoop/common/lib
4. Place your libHdfsEcOffload.so file on every node in the cluster (located in build/lib/libHdfsEcOffload.so) :  
The preferable location is where the Hadoop native libraries are already found:

        $HADOOP_HOME/lib/native/
5. Configure HDFS Erasure Coding Offload plugin in hdfs-site.xml as follows :

        <property>
            <name>io.erasurecode.codec.rs.rawcoder</name>
            <value>com.mellanox.erasurecode.rawcoder.MellanoxRSRawErasureCoderFactory</value>
        </property>
### Tests

**RawErasureCoderValidationTest**

Perfrom encode/decode operations on input files using Hadoop coders.

**Usage**  

    $HADOOP_HOME/bin/hadoop jar $HADOOP_HOME/share/hadoop/common/lib/MellanoxECOffload.jar com/mellanox/erasurecode/rawcoder/RawErasureCoderValidationTest --help

    RawErasureCoderValidationTest usage:
    Encode : encode <coderIndex> <num data blocks> <num code blocks> <chunkSize-in-B> <input file>
    Decode : decode <coderIndex> <num data blocks> <num code blocks> <chunkSize-in-B> <input file> <encoded file> <comma separated erasures>

    Available coders with coderIndex:
    0:Dummy coder
    1:Reed-Solomon Java coder
    2:ISA-L native coder
    3:Mellanox Ec Offloader

**Usage Example**

_Encode_  

    $HADOOP_HOME/bin/hadoop jar $HADOOP_HOME/share/hadoop/common/lib/MellanoxECOffload.jar com/mellanox/erasurecode/rawcoder/RawErasureCoderValidationTest encode 3 6 3 64 file_to_encode.txt
        
    16/06/14 15:55:53 INFO rawcoder.MellanoxECLibraryLoader: Using Mellanox Erasure Coding Offload plugin
    Performing encode with the following parameters :
    coderIndex = 3, numData = 6, numCode = 3, chunkSizeB = 64, inputFile = file_to_encode.txt
    Test Complete
    
The test will allocate  _numData_ buffers used for the data, _numCode_ used for the encoded data (chunkSizeB bytes each) and encoder specified by the user.   
Then, it will read  _numData_ * _chunkSizeB_ bytes from  _inputFile_ and calculate the encoded data.   
The encoded data will be writen into <inputFile>.<coderIndex>.encode.code.
    
_Decode_ 

    $HADOOP_HOME/bin/hadoop jar $HADOOP_HOME/share/hadoop/common/lib/MellanoxECOffload.jar com/mellanox/erasurecode/rawcoder/RawErasureCoderValidationTest decode 3 6 3 64 file_to_encode.txt file_to_encode.txt.3.encode.code 1,0,0,1,0,0,0,1
        
    16/06/14 16:07:24 INFO rawcoder.MellanoxECLibraryLoader: Using Mellanox Erasure Coding Offload plugin
    Performing decode with the following parameters :
    coderIndex = 3, numData = 6, numCode = 3, chunkSizeB = 64, inputFile = file_to_encode.txt , encodedFile = file_to_encode.txt.3.encode.code, erasures = 1,0,0,1,0,0,0,1
    Test Complete

The test will allocate  _numData_ buffers used for the data, _numCode_ used for the encoded data (chunkSizeB bytes each) and decoder specified by the user.   
Then, it will read  _numData_ * _chunkSizeB_ bytes from  _inputFile_ and  _numCode_ * _chunkSizeB_ bytes from  _encodedFile_.
Then, it will erase the blocks corresponding to _erasures_ and compute them.

The decoded data will be written into <inputFile>.<coderIndex>.decode.data (Should be equal to the _inputFile_).  
The decoded code data will be written into <inputFile>.<coderIndex>.decode.code (Should be equal to the _encodedFile_).

