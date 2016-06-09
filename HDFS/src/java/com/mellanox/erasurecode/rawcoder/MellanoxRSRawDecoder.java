/*
 ** Copyright (C) 2014 Mellanox Technologies
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

import java.nio.ByteBuffer;

import org.apache.hadoop.io.erasurecode.rawcoder.AbstractNativeRawDecoder;

public class MellanoxRSRawDecoder extends AbstractNativeRawDecoder {

		  static {
			  MellanoxECLibraryLoader.checkNativeCodeLoaded();
		  }

		  public MellanoxRSRawDecoder(int numDataUnits, int numParityUnits) {
		    super(numDataUnits, numParityUnits);
		    initImpl(numDataUnits, numParityUnits);
		  }

		  @Override
		  protected void performDecodeImpl(
		          ByteBuffer[] inputs, int[] inputOffsets, int dataLen, int[] erased,
		          ByteBuffer[] outputs, int[] outputOffsets) {
		    decodeImpl(inputs, inputOffsets, dataLen, erased, outputs, outputOffsets);
		  }  

		  @Override
		  public void release() {
		    destroyImpl();
		  }

		  private native void initImpl(int numDataUnits, int numParityUnits);

		  private native void decodeImpl(
		          ByteBuffer[] inputs, int[] inputOffsets, int dataLen, int[] erased,
		          ByteBuffer[] outputs, int[] outputOffsets);

		  private native void destroyImpl();
}
