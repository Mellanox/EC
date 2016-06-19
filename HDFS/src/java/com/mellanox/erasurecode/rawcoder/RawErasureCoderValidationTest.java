package com.mellanox.erasurecode.rawcoder;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
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

	private static RawErasureCoder getAndInitCoder(int index, boolean encode, int numData, int numCode) {
		if (encode) {
			return CODER_MAKERS.get(index).createEncoder(numData, numCode);
		} else {
			return CODER_MAKERS.get(index).createDecoder(numData, numCode);
		}
	}

	private static ByteBuffer[] allocateBuffers(boolean useDirectBuffer, int length, int sizeB) {
		ByteBuffer[] buffersArray = new ByteBuffer[length];
		for (int i = 0 ; i < length ; i ++) {
			buffersArray[i] = useDirectBuffer ? ByteBuffer.allocateDirect(sizeB) : ByteBuffer.allocate(sizeB);
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
				"Decode : decode <coderIndex> <num data blocks> <num code blocks> <chunkSize-in-B> <input file> <encoded file> <comma sepereated erasures>\n");
		printAvailableCoders();
		System.exit(1);
	}

	public static void main(String[] args) {
		String opType = null, inputFile, encodedFile = null, parameters;
		int coderIndex = 0, chunkSizeB = 0, numData = 0, numCode = 0, erasureValue;;
		RandomAccessFile inputFileReader = null, encodedfileReader = null;
		ArrayList<Integer> erasuresList = new ArrayList<Integer>();
		int[] erasuresArray = null;
		String[] splitDevices;
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
		} else if ("decode".equals(opType) && args.length != 8) {
			usage("args size for decode operation must be equal to 8");
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
			splitDevices = args[7].split(",");
			if (splitDevices.length > numData + numCode) {
				usage("Erasures size cannot exceeds the total number of blocks");
			}

			for (int i = 0 ; i < splitDevices.length ; i++) {
				try {
					erasureValue = Integer.parseInt(splitDevices[i]);
					if (erasureValue != 1 && erasureValue != 0) {
						usage("Erasures must be 0/1 saparated by comma");
					}
					if (erasureValue == 1) {
						erasuresList.add(i);
					}
				} catch (NumberFormatException e) {
					usage("Erasures must be 0/1 saparated by comma, " + e.getMessage());
				}
			}

			if (erasuresList.size() > numCode) {
				usage("Erasures devices cannot exceeds the number of code blocks");
			}

			erasuresArray = new int[erasuresList.size()];
			for (int i = 0 ; i < erasuresList.size(); i ++) {
				erasuresArray[i] = erasuresList.get(i);
			}

		}

		coder = getAndInitCoder(coderIndex, isEncode, numData, numCode);

		parameters = String.format("coderIndex = %d, numData = %d, numCode = %d, chunkSizeB = %d, inputFile = %s", coderIndex, numData, numCode, chunkSizeB, inputFile);

		try {
			if (isEncode) {
				System.out.println(String.format("Performing encode with the following parameters :\n%s", parameters));
				performEncode((RawErasureEncoder) coder, numData, numCode, chunkSizeB, inputFileReader, inputFile, coderIndex);
			} else {
				System.out.println(String.format("Performing decode with the following parameters :\n%s , encodedFile = %s, erasures = %s", parameters, encodedFile, args[7]));
				performDecode((RawErasureDecoder) coder,  numData, numCode, chunkSizeB, inputFileReader, inputFile, encodedfileReader, erasuresArray, coderIndex);
			}
			System.out.println("Test Complete");
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
	public static void performEncode(RawErasureEncoder encoder, int numData, int numCode, int chunkSizeB,
			RandomAccessFile fileReader, String inputFile, int coderIndex) throws IOException {
		boolean isDirect = (Boolean) encoder.getCoderOption(CoderOption.PREFER_DIRECT_BUFFER);
		ByteBuffer[] inputs = allocateBuffers(isDirect, numData, chunkSizeB);
		ByteBuffer[] outputs = allocateBuffers(isDirect, numCode, chunkSizeB);
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

	public static void performDecode(RawErasureDecoder decoder, int numData, int numCode, int chunkSizeB,
			RandomAccessFile inputFileReader, String inputFile, RandomAccessFile encodedFileReader,
			int[] erasuresArray, int coderIndex) throws IOException {

		boolean isDirect = (Boolean) decoder.getCoderOption(CoderOption.PREFER_DIRECT_BUFFER);

		ByteBuffer[] inputFileInputs = allocateBuffers(isDirect, numData, chunkSizeB);
		ByteBuffer[] encodedFileInputs = allocateBuffers(isDirect, numCode, chunkSizeB);
		ByteBuffer[] decodeInputs = allocateBuffers(isDirect, numData + numCode, chunkSizeB);
		ByteBuffer[] decodeOutputs = allocateBuffers(isDirect, erasuresArray.length, chunkSizeB);

		FileChannel inputFileChannel = inputFileReader.getChannel();
		FileChannel encodedFileChannel = encodedFileReader.getChannel();

		FileOutputStream inputfileOutputStream = new FileOutputStream(new File(inputFile + "." + coderIndex + ".decode.data"));
		FileChannel inputfileOutChannel = inputfileOutputStream.getChannel();

		FileOutputStream encodedFileOutputStream = new FileOutputStream(new File(inputFile + "." + coderIndex + ".decode.code"));
		FileChannel encodedFileOutChannel = encodedFileOutputStream.getChannel();

		int erasuresItr;
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

			// perform decode
			decoder.decode(decodeInputs, erasuresArray, decodeOutputs);

			// reset positions
			resetBuffers(decodeInputs);
			resetBuffers(decodeOutputs);

			erasuresItr = 0;
			// write to data file
			for (int i = 0 ; i < numData; i++) {
				buffer = decodeInputs[i] != null ? decodeInputs[i] : decodeOutputs[erasuresItr++];
				buffer.limit(Math.min(buffer.limit(), (int)bytesLeft));
				bytesLeft -= bytesLeft > 0 ? inputfileOutChannel.write(buffer): 0;
			}

			// write to code file
			for (int i = numData ; i < numData + numCode ; i++) {
				buffer = decodeInputs[i] != null ? decodeInputs[i] : decodeOutputs[erasuresItr++];
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
