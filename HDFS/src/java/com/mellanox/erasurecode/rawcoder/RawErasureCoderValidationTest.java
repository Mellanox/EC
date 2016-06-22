/*
 ** Copyright (C) 2016 Mellanox Technologies
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at:
 **
 ** http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 ** either express or implied. See the License for the specific language
 ** governing permissions and  limitations under the License.
 **
 */

package com.mellanox.erasurecode.rawcoder;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

import org.apache.hadoop.io.erasurecode.rawcoder.CoderOption;
import org.apache.hadoop.io.erasurecode.rawcoder.DummyRawErasureCoderFactory;
import org.apache.hadoop.io.erasurecode.rawcoder.NativeRSRawErasureCoderFactory;
import org.apache.hadoop.io.erasurecode.rawcoder.RSRawErasureCoderFactory;
import org.apache.hadoop.io.erasurecode.rawcoder.RawErasureCoder;
import org.apache.hadoop.io.erasurecode.rawcoder.RawErasureCoderFactory;
import org.apache.hadoop.io.erasurecode.rawcoder.RawErasureDecoder;
import org.apache.hadoop.io.erasurecode.rawcoder.RawErasureEncoder;

import com.google.common.base.Preconditions;

public class RawErasureCoderValidationTest {

	private RawErasureCoderValidationTest(){
		// prevent instantiation
	}

	private static final byte zero = 0;
	private static int coderIndex = 0, chunkSizeB = 0, numData = 0, numCode = 0;

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

	private static void resetBuffers(ByteBuffer[] buffers) {
		resetBuffers(buffers, 0);
	}

	private static void resetBuffers(ByteBuffer[] buffers, int chunkSizeB) {
		for (ByteBuffer buffer : buffers) {
			if (buffer != null) {
				while (buffer.position() < chunkSizeB) {
					buffer.put(zero);
				}
				buffer.clear();
			}
		}
	}

	private static int getPlaceInArray(int[] erasures, int val) {
		for (int i = 0 ; i < erasures.length ; i++) {
			if (erasures[i] == val) {
				return i;
			}
		}
		return -1;
	}

	private static boolean containsDuplicates(int[] arr) {
		for (int i = 0 ; i < arr.length - 1; i++)
			for (int j = i + 1 ; j < arr.length ; j++)
				if (arr[i] == arr[j])
					return true;

		return false;
	}

	private static boolean containsDuplicates(int[] arr1, int[] arr2) {
		for (int i = 0 ; i < arr1.length; i++)
			for (int j = 0 ; j < arr2.length ; j++)
				if (arr1[i] == arr2[j])
					return true;

		return false;
	}

	private static int[] extractIntArray(String devices, int lastArraySize) {
		String[] splitDevices;
		int[] resultArray;
		boolean hasCodeIndex = false;
		int value;

		splitDevices = devices.split(",");
		if (splitDevices.length + lastArraySize >  numCode) {
			usage("Erasures + redundant size cannot exceeds the total number of code blocks");
		}

		resultArray = new int[splitDevices.length];

		for (int i = 0 ; i < splitDevices.length ; i++) {
			try {
				value = Integer.parseInt(splitDevices[i]);
				if (value < 0 || value > numData + numCode - 1) {
					usage("Erasures/redundant values must be in the range [0 , numData + numCode - 1 ] separated by comma");
				}
				if (value >= numData) {
					hasCodeIndex = true;
				} else {
					if (hasCodeIndex) {
						usage("Erasures/redundant data units should be first or before the parity units");
					}
				}
				resultArray[i] = value;
			} catch (NumberFormatException e) {
				usage("Erasures/redundant values must be in the range [0 , numData + numCode - 1] separated by comma, " + e.getMessage());
			}
		}

		if (containsDuplicates(resultArray))
			usage("Duplicate Erasures/redundant are not allowed");
		return resultArray;
	}

	private static RawErasureCoder getAndInitCoder(boolean encode) {
		if (encode) {
			return CODER_MAKERS.get(coderIndex).createEncoder(numData, numCode);
		} else {
			return CODER_MAKERS.get(coderIndex).createDecoder(numData, numCode);
		}
	}

	private static ByteBuffer[] allocateBuffers(boolean useDirectBuffer, int length) {
		ByteBuffer[] buffersArray = new ByteBuffer[length];
		for (int i = 0 ; i < length ; i ++) {
			buffersArray[i] = useDirectBuffer ? ByteBuffer.allocateDirect(chunkSizeB) : ByteBuffer.allocate(chunkSizeB);
		}
		return buffersArray;
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
				"\nRawErasureCoderValidationTest usage:\n" +
				"Encode : encode <coderIndex> <num data blocks> <num code blocks> <chunkSize-in-B> <input file>\n" +
				"Decode : decode <coderIndex> <num data blocks> <num code blocks> <chunkSize-in-B> <input file> <encoded file> <comma separated erasures> <comma separated redundant>\n");
		printAvailableCoders();
		System.exit(1);
	}

	public static void main(String[] args) {
		String opType = null, inputFile, encodedFile = null, parameters, erasuresStr = null, redundantStr = null;
		RandomAccessFile inputFileReader = null, encodedfileReader = null;
		int[] erasuresArray = null, redundantArray = new int[0];
		RawErasureCoder coder;

		if (args.length == 0 || args[0].equals("--help")) {
			usage(null);
		}

		// encode / decode
		opType = args[0];
		if (!"encode".equals(opType) && !"decode".equals(opType)) {
			usage("Invalid type: should be either 'encode' or 'decode'");
		}

		if ("encode".equals(opType) && args.length != 6) {
			usage("args size for encode operation must be equal to 6");
		} else if ("decode".equals(opType) && args.length != 8 && args.length != 9) {
			usage("args size for decode operation must be equal to 8/9");
		}

		// coder index
		try {
			coderIndex = Integer.parseInt(args[1]);
			if (coderIndex < 0 || coderIndex >= CODER_NAMES.size()) {
				usage("Invalid coder index, should be [0-" +
						(CODER_NAMES.size() - 1) + "]");
			}
		} catch (NumberFormatException e) {
			usage("Malformed coder index, " + e.getMessage());
		}

		// number of data blocks
		try {
			numData = Integer.parseInt(args[2]);
			if (numData <= 0) {
				usage("Invalid number of data blocks.");
			}
		} catch (NumberFormatException e) {
			usage("Malformed number of data blocks, " + e.getMessage());
		}

		// number of code blocks
		try {
			numCode = Integer.parseInt(args[3]);
			if (numCode <= 0) {
				usage("Invalid number of code blocks.");
			}
			if (numData + numCode > 16) {
				usage("Invalid inputs : #dataBlocks + #codeBlock cannot exceed 16");
			}
		} catch (NumberFormatException e) {
			usage("Malformed number of code blocks, " + e.getMessage());
		}

		// chunk size
		try {
			chunkSizeB = Integer.parseInt(args[4]);
			if (chunkSizeB <= 0) {
				usage("Invalid number of chunkSizeB.");
			}
			if (chunkSizeB % 64 != 0) {
				usage("Invalid inputs : chunksize must be aligned to 64b");
			}
		} catch (NumberFormatException e) {
			usage("Malformed size of chunk, " + e.getMessage());
		}

		// open file
		inputFile = args[5];
		try {
			inputFileReader = new RandomAccessFile(inputFile, "r");
		}
		catch (Exception e)
		{
			usage(String.format("Exception occurred trying to read the input file '%s'.", inputFile));
		}

		boolean isEncode = opType.equalsIgnoreCase("encode");
		if (!isEncode) {

			// open encoded file for decode
			encodedFile = args[6];
			try {
				encodedfileReader = new RandomAccessFile(encodedFile, "r");
			}
			catch (Exception e)
			{
				usage(String.format("Exception occurred trying to read the encoded file '%s'.", encodedFile));
			}

			// extract erasures
			erasuresStr = args[7];
			erasuresArray = extractIntArray(erasuresStr, 0 );

			if (args.length == 9) {
				redundantStr = args[8];
				redundantArray = extractIntArray(redundantStr, erasuresArray.length);

				if (containsDuplicates(erasuresArray, redundantArray)) {
					usage("Duplicate Erasures/redundant are not allowed");
				}
			}
		}
		coder = getAndInitCoder(isEncode);

		parameters = String.format("coderIndex = %d, numData = %d, numCode = %d, chunkSizeB = %d, inputFile = %s", coderIndex, numData, numCode, chunkSizeB, inputFile);

		try {
			if (isEncode) {
				System.out.println(String.format("Performing encode with the following parameters :\n%s", parameters));
				performEncode((RawErasureEncoder) coder, inputFileReader, inputFile);
			} else {
				System.out.println(String.format("Performing decode with the following parameters :\n%s , encodedFile = %s, erasures = %s, redundant = %s", parameters, encodedFile, erasuresStr, redundantStr));
				performDecode((RawErasureDecoder) coder, inputFileReader, inputFile, encodedfileReader, erasuresArray, redundantArray);
			}
			System.out.println("Test Complete");
			System.exit(0);
		} catch (IOException e) {
			System.out.println("Test Failed !");
			e.printStackTrace();
		}
	}

	/**
	 * Performs benchmark.
	 *
	 * @param opType      The operation to perform. Can be encode or decode
	 * @param coderIndex  An index into the coder array
	 * @param numData     Number of data blocks
	 * @param numCode     Number of code blocks
	 * @param chunkSizeB Chunk size in B
	 * @param fileReader input file for encode operation
	 * @throws IOException
	 */
	public static void performEncode(RawErasureEncoder encoder, RandomAccessFile fileReader, String inputFile) throws IOException {
		boolean isDirect = (Boolean) encoder.getCoderOption(CoderOption.PREFER_DIRECT_BUFFER);
		ByteBuffer[] inputs = allocateBuffers(isDirect, numData);
		ByteBuffer[] outputs = allocateBuffers(isDirect, numCode);
		FileChannel inChannel = fileReader.getChannel();
		FileOutputStream fileOutputStream = new FileOutputStream(new File(inputFile + "." + coderIndex + ".encode.code"));
		FileChannel outChannel = fileOutputStream.getChannel();

		// read input file
		while (inChannel.read(inputs) > 0) {

			// memset unfull buffers and reset all input buffers
			resetBuffers(inputs, chunkSizeB);

			// encode
			encoder.encode(inputs, outputs);

			// reset positions
			resetBuffers(inputs);

			resetBuffers(outputs);

			// write to file
			outChannel.write(outputs);

			// reset posion
			resetBuffers(outputs);
		}

		encoder.release();
		fileReader.close();
		fileOutputStream.close();
	}

	public static void performDecode(RawErasureDecoder decoder, RandomAccessFile inputFileReader, String inputFile, RandomAccessFile encodedFileReader,
			int[] erasuresArray, int[] redundantArray) throws IOException {

		boolean isDirect = (Boolean) decoder.getCoderOption(CoderOption.PREFER_DIRECT_BUFFER);

		ByteBuffer[] inputFileInputs = allocateBuffers(isDirect, numData);
		ByteBuffer[] encodedFileInputs = allocateBuffers(isDirect, numCode);
		ByteBuffer[] decodeInputs = allocateBuffers(isDirect, numData + numCode);
		ByteBuffer[] decodeOutputs = allocateBuffers(isDirect, erasuresArray.length);

		FileChannel inputFileChannel = inputFileReader.getChannel();
		FileChannel encodedFileChannel = encodedFileReader.getChannel();

		FileOutputStream inputfileOutputStream = new FileOutputStream(new File(inputFile + "." + coderIndex + ".decode.data"));
		FileChannel inputfileOutChannel = inputfileOutputStream.getChannel();

		FileOutputStream encodedFileOutputStream = new FileOutputStream(new File(inputFile + "." + coderIndex + ".decode.code"));
		FileChannel encodedFileOutChannel = encodedFileOutputStream.getChannel();

		long bytesLeft;

		ByteBuffer buffer;

		// // read input files
		while ((bytesLeft = inputFileChannel.read(inputFileInputs)) > 0 && encodedFileChannel.read(encodedFileInputs) > 0) {

			// memset unfull buffers and reset all input and encoded buffers
			resetBuffers(inputFileInputs, chunkSizeB);
			resetBuffers(encodedFileInputs, chunkSizeB);

			// prepare the decode inputs
			for (int i = 0 ; i < numData + numCode ; i ++) {
				decodeInputs[i] = i < numData ? inputFileInputs[i].duplicate() : encodedFileInputs[i - numData].duplicate() ;
			}

			// unset erased indexes
			for (int erasedIndex : erasuresArray) {
				decodeInputs[erasedIndex] = null;
			}

			// unset redundant indexes
			for (int redundant: redundantArray) {
				decodeInputs[redundant] = null;
			}

			// perform decode
			decoder.decode(decodeInputs, erasuresArray, decodeOutputs);

			// reset positions
			resetBuffers(decodeInputs);
			resetBuffers(decodeOutputs);

			// set redundant indexes
			for (int redundant: redundantArray) {
				decodeInputs[redundant] = redundant < numData ? inputFileInputs[redundant].duplicate() : encodedFileInputs[redundant - numData].duplicate() ;
			}

			// write to data file
			for (int i = 0 ; i < numData; i++) {
				buffer = decodeInputs[i] != null ? decodeInputs[i] : decodeOutputs[getPlaceInArray(erasuresArray, i)];
				buffer.limit(Math.min(buffer.limit(), (int)bytesLeft));
				bytesLeft -= bytesLeft > 0 ? inputfileOutChannel.write(buffer): 0;
			}

			// write to code file
			for (int i = numData ; i < numData + numCode ; i++) {
				buffer = decodeInputs[i] != null ? decodeInputs[i] : decodeOutputs[getPlaceInArray(erasuresArray, i)];
				encodedFileOutChannel.write(buffer);
			}

			// reset decodeOutputs positions
			resetBuffers(decodeOutputs);
		}

		decoder.release();
		inputFileChannel.close();
		encodedFileChannel.close();
		inputfileOutputStream.close();
		encodedFileOutputStream.close();
	}
}
