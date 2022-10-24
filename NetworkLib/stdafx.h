#pragma once

#pragma comment(lib, "ws2_32")

#include <iostream>
#include <WinSock2.h>
#include <DbgHelp.h>
#include <crtdbg.h>
#include <Psapi.h>
#include <assert.h>
#include <tchar.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <unordered_set>
#include <unordered_map>
#include <memory>

typedef uint64_t sessionID_t;

#include "EIOType.h"

#include "CCrashDump.h"
#include "Profiler.h"
#include "ProfilerManager.h"

#include "Chunk.h"
#include "ObjectPool.h"
#include "TLSObjectPoolMiddleware.h"
#include "TLSObjectPool.h"

#include "LockFreeStack.h"
#include "LockFreeQueue.h"

#include "Message.h"
#include "IntrusivePointer.h"
#include "RingBuffer.h"
#include "INetworkEventListener.h"
#include "LanServer.h"
#include "WanServer.h"