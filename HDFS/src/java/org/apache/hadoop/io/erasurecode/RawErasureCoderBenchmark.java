/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * <p/>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p/>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.hadoop.io.erasurecode;

import com.google.common.base.Preconditions;
import com.mellanox.erasurecode.rawcoder.MellanoxRSRawErasureCoderFactory;

import org.apache.hadoop.io.erasurecode.rawcoder.CoderOption;
import org.apache.hadoop.io.erasurecode.rawcoder.DummyRawErasureCoderFactory;
import org.apache.hadoop.io.erasurecode.rawcoder.RSRawErasureCoderFactory;
import org.apache.hadoop.io.erasurecode.rawcoder.RawErasureCoder;
import org.apache.hadoop.io.erasurecode.rawcoder.RawErasureCoderFactory;
import org.apache.hadoop.io.erasurecode.rawcoder.RawErasureDecoder;
import org.apache.hadoop.io.erasurecode.rawcoder.RawErasureEncoder;
import org.apache.hadoop.io.erasurecode.rawcoder.NativeRSRawErasureCoderFactory;
import org.apache.hadoop.util.StopWatch;

import java.nio.ByteBuffer;
import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

/**
 * This is a modified version of RawErasureCoderBenchmark (HADOOP-11588) which supports MellanoxRSRawErasureCoderFactory.
 * The changes are:
 *	1. Add Mellanox EC Offloader to the available coder list.
 *	2. Create encoder/decoder for each client instead one encoder/decoder for all clients.
 *	3. initialize coders using 64 bytes arrays (because of the buffer alignment limitation).
 *
 * A benchmark tool to test the performance of different erasure coders.
 * The tool launches multiple threads to encode/decode certain amount of data,
 * and measures the total throughput. It only focuses on performance and doesn't
 * validate correctness of the encoded/decoded results.
 * User can specify the data size each thread processes, as well as the chunk
 * size to use for the coder.
 * Different coders are supported. User can specify the coder by a coder index.
 * The coder is shared among all the threads.
 */
public final class RawErasureCoderBenchmark {

  private RawErasureCoderBenchmark(){
    // prevent instantiation
  }

  // target size of input data buffer
  private static final int TARGET_BUFFER_SIZE_MB = 126;

  private static final int MAX_CHUNK_SIZE =
      TARGET_BUFFER_SIZE_MB / BenchData.NUM_DATA_UNITS * 1024;

  private static final List<RawErasureCoderFactory> CODER_MAKERS =
      Collections.unmodifiableList(
          Arrays.asList(new DummyRawErasureCoderFactory(),
              new RSRawErasureCoderFactory(),
              new NativeRSRawErasureCoderFactory(),
              new MellanoxRSRawErasureCoderFactory()));

  private static final List<String> CODER_NAMES = Collections.unmodifiableList(
      Arrays.asList("Dummy coder", "Reed-Solomon Java coder", "ISA-L native coder", "Mellanox Ec Offloader"));

  static {
    Preconditions.checkArgument(CODER_MAKERS.size() == CODER_NAMES.size());
  }

  private static void printAvailableCoders() {
    StringBuilder sb = new StringBuilder("Available coders with coderIndex:\n");
    for (int i = 0; i < CODER_NAMES.size(); i++) {
      sb.append(i).append(":").append(CODER_NAMES.get(i)).append("\n");
    }
    System.out.println(sb.toString());
  }

  private static void usage(String message) {
    if (message != null) {
      System.out.println(message);
    }
    System.out.println(
        "Usage: RawErasureCoderBenchmark <encode/decode> <coderIndex> " +
            "[numClients] [dataSize-in-MB] [chunkSize-in-KB]");
    printAvailableCoders();
    System.exit(1);
  }

  public static void main(String[] args) throws Exception {
    String opType = null;
    int coderIndex = 0;
    // default values
    int dataSizeMB = 10240;
    int chunkSizeKB = 1024;
    int numClients = 1;

    if (args.length > 1) {
      opType = args[0];
      if (!"encode".equals(opType) && !"decode".equals(opType)) {
        usage("Invalid type: should be either 'encode' or 'decode'");
      }

      try {
        coderIndex = Integer.parseInt(args[1]);
        if (coderIndex < 0 || coderIndex >= CODER_NAMES.size()) {
          usage("Invalid coder index, should be [0-" +
              (CODER_NAMES.size() - 1) + "]");
        }
      } catch (NumberFormatException e) {
        usage("Malformed coder index, " + e.getMessage());
      }
    } else {
      usage(null);
    }

    if (args.length > 2) {
      try {
        numClients = Integer.parseInt(args[2]);
        if (numClients <= 0) {
          usage("Invalid number of clients.");
        }
      } catch (NumberFormatException e) {
        usage("Malformed number of clients, " + e.getMessage());
      }
    }

    if (args.length > 3) {
      try {
        dataSizeMB = Integer.parseInt(args[3]);
        if (dataSizeMB <= 0) {
          usage("Invalid data size.");
        }
      } catch (NumberFormatException e) {
        usage("Malformed data size, " + e.getMessage());
      }
    }

    if (args.length > 4) {
      try {
        chunkSizeKB = Integer.parseInt(args[4]);
        if (chunkSizeKB <= 0) {
          usage("Chunk size should be positive.");
        }
        if (chunkSizeKB > MAX_CHUNK_SIZE) {
          usage("Chunk size should be no larger than " + MAX_CHUNK_SIZE);
        }
      } catch (NumberFormatException e) {
        usage("Malformed chunk size, " + e.getMessage());
      }
    }

    performBench(opType, coderIndex, numClients, dataSizeMB, chunkSizeKB);
  }

  /**
   * Performs benchmark.
   *
   * @param opType      The operation to perform. Can be encode or decode
   * @param coderIndex  An index into the coder array
   * @param numClients  Number of threads to launch concurrently
   * @param dataSizeMB  Total test data size in MB
   * @param chunkSizeKB Chunk size in KB
   */
  public static void performBench(String opType, int coderIndex, int numClients,
      int dataSizeMB, int chunkSizeKB) throws Exception {
    BenchData.configure(dataSizeMB, chunkSizeKB);

    List<RawErasureCoder> codersList = new ArrayList<RawErasureCoder>(numClients);

    for (int i = 0; i < numClients; i++) {
        codersList.add(getAndInitCoder(coderIndex, opType.equalsIgnoreCase("encode")));
    }

    ByteBuffer testData = genTestData((Boolean) codersList.get(0).getCoderOption(
        CoderOption.PREFER_DIRECT_BUFFER), BenchData.bufferSizeKB);

    ExecutorService executor = Executors.newFixedThreadPool(numClients);
    List<Future<Long>> futures = new ArrayList<>(numClients);
    StopWatch sw = new StopWatch().start();
    for (int i = 0; i < numClients; i++) {
      futures.add(executor.submit(new BenchmarkCallable(
          codersList.get(i), testData.duplicate())));
    }
    List<Long> durations = new ArrayList<>(numClients);
    try {
      for (Future<Long> future : futures) {
        durations.add(future.get());
      }
      long duration = sw.now(TimeUnit.MILLISECONDS);
      double totalDataSize = BenchData.totalDataSizeKB * numClients / 1024.0;
      DecimalFormat df = new DecimalFormat("#.##");
      System.out.println(CODER_NAMES.get(coderIndex) + " " + opType + " " +
          df.format(totalDataSize) + "MB data, with chunk size " +
          BenchData.chunkSize / 1024 + "KB");
      System.out.println("Total time: " + df.format(duration / 1000.0) + " s.");
      System.out.println("Total throughput: " + df.format(
          totalDataSize / duration * 1000.0) + " MB/s");
      printClientStatistics(durations, df);
      for (RawErasureCoder coder : codersList) {
          coder.release();
      }
    } catch (Exception e) {
      System.out.println("Error waiting for client to finish.");
      e.printStackTrace();
      throw e;
    } finally {
      executor.shutdown();
    }
  }

  private static RawErasureCoder getAndInitCoder(int index, boolean encode) {
    if (encode) {
      RawErasureEncoder encoder = CODER_MAKERS.get(index).createEncoder(
          BenchData.NUM_DATA_UNITS, BenchData.NUM_PARITY_UNITS);
      encoder.encode(new byte[BenchData.NUM_DATA_UNITS][64],
          new byte[BenchData.NUM_PARITY_UNITS][64]);
      return encoder;
    } else {
      RawErasureDecoder decoder = CODER_MAKERS.get(index).createDecoder(
          BenchData.NUM_DATA_UNITS, BenchData.NUM_PARITY_UNITS);
      byte[][] inputs = new byte[BenchData.NUM_ALL_UNITS][64];
      for (int erasedIndex : BenchData.ERASED_INDEXES) {
        inputs[erasedIndex] = null;
      }
      decoder.decode(inputs, BenchData.ERASED_INDEXES,
          new byte[BenchData.ERASED_INDEXES.length][64]);
      return decoder;
    }
  }

  private static void printClientStatistics(
      List<Long> durations, DecimalFormat df) {
    Collections.sort(durations);
    System.out.println("Clients statistics: ");
    Double min = durations.get(0) / 1000.0;
    Double max = durations.get(durations.size() - 1) / 1000.0;
    Long sum = 0L;
    for (Long duration : durations) {
      sum += duration;
    }
    Double avg = sum.doubleValue() / durations.size() / 1000.0;
    Double percentile = durations.get(
        (int) Math.ceil(durations.size() * 0.9) - 1) / 1000.0;
    System.out.println(durations.size() + " clients in total.");
    System.out.println("Min: " + df.format(min) + " s, Max: " +
        df.format(max) + " s, Avg: " + df.format(avg) +
        " s, 90th Percentile: " + df.format(percentile) + " s.");
  }

  private static ByteBuffer genTestData(boolean useDirectBuffer, int sizeKB) {
    Random random = new Random();
    int bufferSize = sizeKB * 1024;
    byte[] tmp = new byte[bufferSize];
    random.nextBytes(tmp);
    ByteBuffer data = useDirectBuffer ?
        ByteBuffer.allocateDirect(bufferSize) :
        ByteBuffer.allocate(bufferSize);
    data.put(tmp);
    data.flip();
    return data;
  }

  private static class BenchData {
    public static final int NUM_DATA_UNITS = 6;
    public static final int NUM_PARITY_UNITS = 3;
    public static final int NUM_ALL_UNITS = NUM_DATA_UNITS + NUM_PARITY_UNITS;
    private static int chunkSize;
    private static long totalDataSizeKB;
    private static int bufferSizeKB;

    private static final int[] ERASED_INDEXES = new int[]{6, 7, 8};
    private final ByteBuffer[] inputs = new ByteBuffer[NUM_DATA_UNITS];
    private ByteBuffer[] outputs = new ByteBuffer[NUM_PARITY_UNITS];
    private ByteBuffer[] decodeInputs = new ByteBuffer[NUM_ALL_UNITS];

    public static void configure(int dataSizeMB, int chunkSizeKB) {
      chunkSize = chunkSizeKB * 1024;
      // buffer size needs to be a multiple of (numDataUnits * chunkSize)
      int round = (int) Math.round(
          TARGET_BUFFER_SIZE_MB * 1024.0 / NUM_DATA_UNITS / chunkSizeKB);
      Preconditions.checkArgument(round > 0);
      bufferSizeKB = NUM_DATA_UNITS * chunkSizeKB * round;
      System.out.println("Using " + bufferSizeKB / 1024 + "MB buffer.");

      round = (int) Math.round(
          (dataSizeMB * 1024.0) / bufferSizeKB);
      if (round == 0) {
        round = 1;
      }
      totalDataSizeKB = round * bufferSizeKB;
    }

    public BenchData(boolean useDirectBuffer) {
      for (int i = 0; i < outputs.length; i++) {
        outputs[i] = useDirectBuffer ? ByteBuffer.allocateDirect(chunkSize) :
            ByteBuffer.allocate(chunkSize);
      }
    }

    public void prepareDecInput() {
      System.arraycopy(inputs, 0, decodeInputs, 0, NUM_DATA_UNITS);
    }

    public void encode(RawErasureEncoder encoder) {
      encoder.encode(inputs, outputs);
    }

    public void decode(RawErasureDecoder decoder) {
      decoder.decode(decodeInputs, ERASED_INDEXES, outputs);
    }
  }

  private static class BenchmarkCallable implements Callable<Long> {
    private final boolean isEncode;
    private RawErasureEncoder encoder;
    private RawErasureDecoder decoder;
    private final BenchData benchData;
    private final ByteBuffer testData;

    public BenchmarkCallable(RawErasureCoder coder, ByteBuffer testData) {
      this.isEncode = (coder instanceof RawErasureEncoder);
      if (isEncode) {
        encoder = (RawErasureEncoder) coder;
      } else {
        decoder = (RawErasureDecoder) coder;
      }
      benchData = new BenchData((Boolean) coder.getCoderOption(
          CoderOption.PREFER_DIRECT_BUFFER));
      this.testData = testData;
    }

    @Override
    public Long call() throws Exception {
      long rounds = BenchData.totalDataSizeKB / BenchData.bufferSizeKB;

      StopWatch sw = new StopWatch().start();
      for (long i = 0; i < rounds; i++) {
        while (testData.remaining() > 0) {
          for (ByteBuffer output : benchData.outputs) {
            output.clear();
          }

          for (int j = 0; j < benchData.inputs.length; j++) {
            benchData.inputs[j] = testData.duplicate();
            benchData.inputs[j].limit(
                testData.position() + BenchData.chunkSize);
            benchData.inputs[j] = benchData.inputs[j].slice();
            testData.position(testData.position() + BenchData.chunkSize);
          }

          if (!isEncode) {
            benchData.prepareDecInput();
          }

          if (isEncode) {
            benchData.encode(encoder);
          } else {
            benchData.decode(decoder);
          }
        }
        testData.clear();
      }
      return sw.now(TimeUnit.MILLISECONDS);
    }
  }
}
