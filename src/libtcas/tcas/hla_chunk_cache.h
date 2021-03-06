﻿/*
 * hla_chunk_cache.h -- High-level chunk cache API of the 'libtcas' library
 * Copyright (C) 2011 milkyjing <milkyjing@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * milkyjing
 *
 */

#ifndef LIBTCAS_HLA_CHUNK_CACHE_H
#define LIBTCAS_HLA_CHUNK_CACHE_H
#pragma once

#include "tcas.h"
#include "hla_z_comp.h"
#include "queue.h"
#include "rb.h"
#include "avl.h"
#include "hla_file_cache.h"

#include <Windows.h>

#define TCAS_MAX_FRAME_CHUNKS_CACHED 2

/**
 * TCAS_FrameChunk structure, element of the chunks cache tree 1, and the vector which holds the reference to the elements in the tree
 */
typedef struct _tcas_normal_frame_chunk {
    TCAS_Chunk chunk;   /**< content of the chunk */
    tcas_u32 offset;    /**< used as key to uniquely identifies the chunk */
    int ref_count;      /**< reference count of the chunk by the frames */
} TCAS_NormalFrameChunk, *TCAS_pNormalFrameChunk;

/**
 * TCAS_KeyFrameChunk structure, element of the chunks cache tree 2, and the vector which holds the real intermediate chunk object generated by the elements in the tree
 */
typedef struct _tcas_key_frame_chunk {
    TCAS_KeyFrameChunkPair chunkPair;   /**< content of the key chunk */
    tcas_u32 offset;    /**< used as key to uniquely identifies the chunk */
    int ref_count;      /**< reference count of the chunk by the frames */
} TCAS_KeyFrameChunk, *TCAS_pKeyFrameChunk;

/**
 * TCAS_FrameChunk structure, contained in a vector which holds references to TCAS_NormalFrameChunk.chunk or the real intermediate chunk
 */
typedef struct _tcas_frame_chunk {
    TCAS_pChunk pChunk; /**< can be either a reference to the normal frame chunk, nor real content of the generated intermediate frame chunk */
    tcas_u32 offset;    /**< used as key to uniquely identifies the chunk */
    int *pRef;          /**< pointer to ref_count */
    signed char is_intermediate;    /**< indicates the pChunk type */
} TCAS_FrameChunk, *TCAS_pFrameChunk;

/**
 * TCAS_QueuedFrameChunks structure, to hold the frame id and a frame of chunks (contained in a vector), it is the element of the queue qFrameChunks
 */
typedef struct _tcas_queued_frame_chunks {
    int id;              /**< frame number */
    VectorPtr chunks;    /**< holds a frame of TCAS_FrameChunk values */
} TCAS_QueuedFrameChunks, *TCAS_pQueuedFrameChunks;

/**
 * TCAS_FrameChunksCacheProcArgs structure, to hold the parameters which are going to be passed to the worker thread
 */
typedef struct _tcas_frame_chunks_cache_proc_args {
    TCAS_FileCache fileCache;
    TCAS_File file;
    TCAS_Header header;
    TCAS_IndexStreamsPtr indexStreams;
    tcas_u32 n;             /* should be specified during the rendering time, not in the initialization */
    RbTreePtr cacheNormal;  /* a reference to cacheNormal of TCAS_FrameChunksCache */
    AvlTreePtr cacheKey;    /* a reference to cacheKey of TCAS_FrameChunksCache */
    QueuePtr qFrameChunks;  /* holds TCAS_QueuedFrameChunks, a reference to the qFrameChunks of TCAS_FrameChunksCache */
    tcas_u16 width;
    tcas_u16 height;
} TCAS_FrameChunksCacheProcArgs, *TCAS_pFrameChunksCacheProcArgs;

/**
 * a structure to hold the basic information of the frame cache facility
 */
typedef struct _tcas_frame_chunks_cache {
    RbTreePtr cacheNormal;  /**< a red-black tree which actually contains the content of chunks */
    AvlTreePtr cacheKey;    /**< an AVL tree which actually contains the key chunk pair */
    QueuePtr qFrameChunks;  /**< a queue of frames being cached, the capacity of the queue is limitted to maxFrameCount */
    int maxFrameCount;      /**< maximum chunks of frames that the cache can hold */
    HANDLE semFrames;       /**< a semaphore indicates the available frames of chunks in the queue */
    HANDLE semQueue;        /**< a semaphore indicates the available room of the frames queue, the maximum count is limitted to maxFrameCount */
    CRITICAL_SECTION lock;  /**< lock for synchronization */
    HANDLE tdWorker;        /**< the handler to the worker thread */
    DWORD threadID;
    int active;
    TCAS_FrameChunksCacheProcArgs fccpArgs;
    tcas_u32 minFrame;
    tcas_u32 maxFrame;
} TCAS_FrameChunksCache, *TCAS_pFrameChunksCache;


/* Inhibit C++ name-mangling for libtcas functions but not for system calls. */
#ifdef __cplusplus
extern "C" {
#endif    /* __cplusplus */
    
/**
 * Initialize the frame chunks cache struct
 * @param filename specify the file name which is to be opened
 * @param fpsNumerator FramePerSecond = fpsNumerator / (double)fpsDenominator, can be 0, in this case, fps will not be used
 * @param fpsDenominator FramePerSecond = fpsNumerator / (double)fpsDenominator, can be 0, in this case, fps will not be used
 * @param width width of the target video
 * @param height height of the target video
 * @param maxFrameCount maximum frames (of chunks) that the cache queue can hold
 * @param pFcc a pointer to TCAS_FrameChunksCache structure, which is going to be initialized
 * @return TCAS_Error_Code
 */
extern TCAS_Error_Code libtcas_frame_chunks_cache_init(const char *filename, tcas_u32 fpsNumerator, tcas_u32 fpsDenominator, tcas_u16 width, tcas_u16 height, int maxFrameCount, int fileCacheSize, TCAS_pFrameChunksCache pFcc);

/**
 * Actually run the worker thread to generate frames (of chunks) into the queue
 * @param pFcc a pointer to TCAS_FrameChunksCache structure, which should have been initialized
 */
extern void libtcas_frame_chunks_cache_run(TCAS_pFrameChunksCache pFcc);

/**
 * Get the frame of chunks being cached in the queue.
 * Remark: in the main thread, we use this function to ask for the specific frame of chunks in the frame chunks cache queue, 
 * during which the first frame (of chunks) will be returned, if the frame returned is not the frame specified, 
 * all the frames in the frame cache queue will be dropped, if they meet, only the first frame will be dropped. 
 * If there is no frames cached, the main thread has to wait for the coming of the very first frame. 
 * After one frame being retrieved, semQueue count will be increase by one.
 *
 * @param pFcc a pointer to TCAS_FrameChunksCache structure, which should have been initialized
 * @param n frame index which is wanted
 * @param pv a pointer to the VectorPtr which is going to hold the frame chunks vector TCAS_FrameChunk
 * @return 1 - specified frame returned, 0 - frames cached is not the one we wanted, hence should call this function again to get the specified frame
 */
extern int libtcas_frame_chunks_cache_get(TCAS_pFrameChunksCache pFcc, tcas_u32 n, VectorPtr *pv);

/**
 * Generate frame from the cached frame chunks
 * @param pFcc a pointer to TCAS_FrameChunksCache structure, which should have been initialized
 * @param vFrameChunks a vector which holds several TCAS_FrameChunk
 * @param targetWidth width of the target video
 * @param targetHeight height of the target video
 * @param pBuf a pointer to a block of memory which is going to hold the TCAS frame
 * @return TCAS_Error_Code
 */
extern TCAS_Error_Code libtcas_create_frame_with_chunks_cached(TCAS_pFrameChunksCache pFcc, VectorPtr vFrameChunks, tcas_u16 targetWidth, tcas_u16 targetHeight, tcas_byte **pBuf);

/**
 * Free a frame of chunks cached
 * @param pFcc a pointer to TCAS_FrameChunksCache structure, which should have been initialized
 * @param vFrameChunks a vector which holds several TCAS_FrameChunk
 */
extern void libtcas_frame_chunks_cache_free_frame(TCAS_pFrameChunksCache pFcc, VectorPtr vFrameChunks);

/**
 * Finalize the frame chunks cache facility.
 * @param pFcc a pointer to TCAS_FrameChunksCache structure, which should have been initialized
 */
extern void libtcas_frame_chunks_cache_fin(TCAS_pFrameChunksCache pFcc);

#ifdef __cplusplus
}
#endif    /* __cplusplus */

#endif    /* LIBTCAS_HLA_CHUNK_CACHE_H */

