﻿/*
Copyright (c) 2013, Tolga HOŞGÖR
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies, 
either expressed or implied, of the FreeBSD Project.
*/

#include "../include/Worker.h"
#include "../include/Pool.h"

namespace threadpool11
{
	Worker::Worker(Pool* pool) :
		pool(pool),
		init(0),
		work(nullptr),
		terminate(false),
		thread(std::bind(&Worker::execute, this))
	{
		//std::cout << std::this_thread::get_id() << " Worker created" << std::endl;
	}

	Worker::Worker(Worker&& other) :
		pool(other.pool),
		init(other.init),
		work(std::move(other.work)),
		terminate(other.terminate),
		thread(std::move(other.thread))
	{}

	Worker::~Worker()
	{
		//std::cout << std::this_thread::get_id() << " Worker terminated" << std::endl;
	}
	
	bool Worker::operator==(Worker const& other)
	{
		return thread.get_id() == other.thread.get_id();
	}
	bool Worker::operator==(const Worker* other)
	{
		return operator==(*other);
	}

	void Worker::setWork(work_type const& work)
	{
		std::lock_guard<std::mutex> lock(workPostedMutex);
		this->work = std::move(work);
		workPosted.notify_one();
	}

	void Worker::execute()
	{
		//std::cout << std::this_thread::get_id() << " Execute called" << std::endl;
		{
			std::unique_lock<std::mutex> initLock(this->initMutex);
			std::unique_lock<std::mutex> lock(workPostedMutex);
			init = 1;
			initialized.notify_one();
			initLock.unlock();
			workPosted.wait(lock);
		}

		while(!terminate)
		{
			std::unique_lock<std::mutex> lock(workPostedMutex);
			//std::cout << "thread started" << std::endl;
			WORK:
			//std::cout << "work started 2" << std::endl;
			work();
			//std::cout << std::this_thread::get_id() <<  "-" << workPosted.native_handle()
			//	<< " Work called" << std::endl;
			pool->enqueuedWorkMutex.lock();
			if(pool->enqueuedWork.size() > 0)
			{
				work = std::move(pool->enqueuedWork.front());
				pool->enqueuedWork.pop_front();
				pool->enqueuedWorkMutex.unlock();
			//	std::cout << pool->enqueuedWork.size() << " got work from enqueue, going back to work" << std::endl;
				goto WORK;
			}
			pool->enqueuedWorkMutex.unlock();
			
			pool->notifyAllFinishedMutex.lock();
			pool->workerContainerMutex.lock();

			pool->activeWorkers.erase(poolIterator);
			pool->inactiveWorkers.emplace_front(this);

			if(pool->activeWorkers.size() == 0)
			{
			//	std::cout << "notify all finished" << std::endl;
				pool->notifyAllFinished.notify_one();
			}

			pool->workerContainerMutex.unlock();
			pool->notifyAllFinishedMutex.unlock();
			
			//std::cout << pool->activeWorkers.size() << " work finished" << std::endl;
			if(terminate)
				break;
			workPosted.wait(lock);
		}
		//std::cout << "terminating" << std::endl;
	}
}
