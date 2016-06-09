/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.mellanox.erasurecode.rawcoder;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

/**
 * Erasure code native libraries related utilities.
 */
public final class MellanoxECLibraryLoader {

  private static final Log LOG =
      LogFactory.getLog(MellanoxECLibraryLoader.class.getName());

  /**
   * The reason why the library is not available, or null if it is available.
   */
  private static final String LOADING_FAILURE_REASON;

  static {
	  LOG.info("Using Mellanox Erasure Coding Offload plugin");
	  String problem = null;
	  try {
	    System.loadLibrary("HdfsEcOffload");
	  } catch (Throwable t) {
		problem = "Loading HdfsEcOffload failed: " + t.getMessage();
	    LOG.error("Loading HdfsEcOffload failed", t);
	  }
		
      LOADING_FAILURE_REASON = problem;
  }

  private MellanoxECLibraryLoader() {}

  /**
   * Are native libraries loaded?
   */
  public static boolean isNativeCodeLoaded() {
    return LOADING_FAILURE_REASON == null;
  }

  /**
   * Is the native library loaded and initialized? Throw exception if not.
   */
  public static void checkNativeCodeLoaded() {
    if (LOADING_FAILURE_REASON != null) {
      throw new RuntimeException(LOADING_FAILURE_REASON);
    }
  }

  /**
   * Load native library available or supported.
   */

  public static String getLoadingFailureReason() {
    return LOADING_FAILURE_REASON;
  }
}
