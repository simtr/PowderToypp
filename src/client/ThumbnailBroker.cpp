#include <algorithm>
#include <iostream>
#include <typeinfo>
#include "ThumbnailBroker.h"
#include "ThumbnailListener.h"
#include "Client.h"
#include "GameSave.h"
#include "search/Thumbnail.h"
#include "simulation/SaveRenderer.h"

//Asynchronous Thumbnail render & request processing

class ThumbnailBroker::ThumbnailSpec
{
public:
	int Width, Height;
	ThumbnailListener * CompletedListener;
	ThumbnailSpec(int width, int height, ThumbnailListener * completedListener) :
		Width(width), Height(height), CompletedListener(completedListener) {}
};

class ThumbnailBroker::ThumbnailID
{
public:
	int SaveID, SaveDate;
	bool operator ==(const ThumbnailID & second)
	{
		return SaveID == second.SaveID && SaveDate == second.SaveDate;
	}
	ThumbnailID(int saveID, int saveDate) : SaveID(saveID), SaveDate(saveDate) {}
	ThumbnailID() : SaveID(0), SaveDate(0) {}
};

class ThumbnailBroker::ThumbnailRequest
{
public:
	bool Complete;
	void * HTTPContext;
	int RequestTime;

	ThumbnailID ID;
	std::vector<ThumbnailSpec> SubRequests;
	
	ThumbnailRequest(int saveID, int saveDate, int width, int height, ThumbnailListener * completedListener) :
		ID(saveID, saveDate), Complete(false), HTTPContext(NULL), RequestTime(0)
		{
			SubRequests.push_back(ThumbnailSpec(width, height, completedListener));
		}
	ThumbnailRequest() : Complete(false), HTTPContext(NULL), RequestTime(0) {}
};

class ThumbnailBroker::ThumbRenderRequest
{
public:
	int Width, Height;
	GameSave * Save;
	ThumbnailListener * CompletedListener;
	ThumbRenderRequest(GameSave * save, int width, int height, ThumbnailListener * completedListener) :
		Save(save), Width(width), Height(height), CompletedListener(completedListener) {}
	ThumbRenderRequest() :	Save(0), Width(0), Height(0), CompletedListener(NULL) {}
};

ThumbnailBroker::ThumbnailBroker()
{
	thumbnailQueueRunning = false;
	thumbnailQueueMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_init (&thumbnailQueueMutex, NULL);

	listenersMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_init (&listenersMutex, NULL);
}

ThumbnailBroker::~ThumbnailBroker()
{

}

void ThumbnailBroker::RenderThumbnail(GameSave * gameSave, int width, int height, ThumbnailListener * tListener)
{
	AttachThumbnailListener(tListener);
	pthread_mutex_lock(&thumbnailQueueMutex);
	bool running = thumbnailQueueRunning;
	thumbnailQueueRunning = true;
	renderRequests.push_back(ThumbRenderRequest(new GameSave(*gameSave), width, height, tListener));
	pthread_mutex_unlock(&thumbnailQueueMutex);
	
	if(!running)
	{
#ifdef DEBUG
		std::cout << typeid(*this).name() << " Starting background thread for new " << __FUNCTION__ << " request" << std::endl;
#endif
		pthread_create(&thumbnailQueueThread, 0, &ThumbnailBroker::thumbnailQueueProcessHelper, this);
	}
}

void ThumbnailBroker::RetrieveThumbnail(int saveID, int saveDate, int width, int height, ThumbnailListener * tListener)
{
	AttachThumbnailListener(tListener);
	pthread_mutex_lock(&thumbnailQueueMutex);
	bool running = thumbnailQueueRunning;
	thumbnailQueueRunning = true;
	thumbnailRequests.push_back(ThumbnailRequest(saveID, saveDate, width, height, tListener));
	pthread_mutex_unlock(&thumbnailQueueMutex);

	if(!running)
	{
#ifdef DEBUG
		std::cout << typeid(*this).name() << " Starting background thread for new " << __FUNCTION__ << " request" << std::endl;
#endif
		pthread_create(&thumbnailQueueThread, 0, &ThumbnailBroker::thumbnailQueueProcessHelper, this);
	}
}

void * ThumbnailBroker::thumbnailQueueProcessHelper(void * ref)
{
	((ThumbnailBroker*)ref)->thumbnailQueueProcessTH();
	return NULL;
}

void ThumbnailBroker::FlushThumbQueue()
{
	pthread_mutex_lock(&thumbnailQueueMutex);
	while(thumbnailComplete.size())
	{
		if(CheckThumbnailListener(thumbnailComplete.front().first))
			thumbnailComplete.front().first->OnThumbnailReady(thumbnailComplete.front().second);
		else
			delete thumbnailComplete.front().second;
		thumbnailComplete.pop_front();
	}	
	pthread_mutex_unlock(&thumbnailQueueMutex);
}

void ThumbnailBroker::thumbnailQueueProcessTH()
{
	time_t lastAction = time(NULL);
	while(true)
	{
		//Shutdown after 2 seconds of idle
		if(time(NULL) - lastAction > 2)
		{
			pthread_mutex_lock(&thumbnailQueueMutex);
			thumbnailQueueRunning = false;
			pthread_mutex_unlock(&thumbnailQueueMutex);
			break;
		}

		//Renderer
		pthread_mutex_lock(&thumbnailQueueMutex);
		if(renderRequests.size())
		{
			lastAction = time(NULL);
			ThumbRenderRequest req;
			req = renderRequests.front();
			renderRequests.pop_front();
			pthread_mutex_unlock(&thumbnailQueueMutex);

#ifdef DEBUG
			std::cout << typeid(*this).name() << " Processing render request" << std::endl;
#endif

			Thumbnail * thumbnail = SaveRenderer::Ref().Render(req.Save);
			delete req.Save;

			if(thumbnail)
			{
				thumbnail->Resize(req.Width, req.Height);

				pthread_mutex_lock(&thumbnailQueueMutex);
				thumbnailComplete.push_back(std::pair<ThumbnailListener*, Thumbnail*>(req.CompletedListener, thumbnail));
				pthread_mutex_unlock(&thumbnailQueueMutex);	
			}
		}
		else
		{
			pthread_mutex_unlock(&thumbnailQueueMutex);
		}

		//Renderer
		pthread_mutex_lock(&thumbnailQueueMutex);
		if(thumbnailRequests.size())
		{
			lastAction = time(NULL);
			Thumbnail * thumbnail = NULL;

			ThumbnailRequest req;
			req = thumbnailRequests.front();

			//Check the cache
			for(std::deque<std::pair<ThumbnailID, Thumbnail*> >::iterator iter = thumbnailCache.begin(), end = thumbnailCache.end(); iter != end; ++iter)
			{
				if((*iter).first == req.ID)
				{
					thumbnail = (*iter).second;
#ifdef DEBUG
					std::cout << typeid(*this).name() << " " << req.ID.SaveID << ":" << req.ID.SaveDate << " found in cache" << std::endl;
#endif
				}
			}

			if(thumbnail)
			{
				//Got thumbnail from cache
				thumbnailRequests.pop_front();
				pthread_mutex_unlock(&thumbnailQueueMutex);

				for(std::vector<ThumbnailSpec>::iterator specIter = req.SubRequests.begin(), specEnd = req.SubRequests.end(); specIter != specEnd; ++specIter)
				{
					Thumbnail * tempThumbnail = new Thumbnail(*thumbnail);
					tempThumbnail->Resize((*specIter).Width, (*specIter).Height);

					pthread_mutex_lock(&thumbnailQueueMutex);
					thumbnailComplete.push_back(std::pair<ThumbnailListener*, Thumbnail*>((*specIter).CompletedListener, tempThumbnail));
					pthread_mutex_unlock(&thumbnailQueueMutex);	
				}
			}
			else if(!thumbnail)
			{
				//Check for ongoing requests
				bool requested = false;
				for(std::list<ThumbnailRequest>::iterator iter = currentRequests.begin(), end = currentRequests.end(); iter != end; ++iter)
				{
					if((*iter).ID == req.ID)
					{
						requested = true;
						
						//Add the current listener to the item already being requested
						(*iter).SubRequests.push_back(req.SubRequests.front());
					}
				}

				if(requested)
				{
					//Already requested
					thumbnailRequests.pop_front();
					pthread_mutex_unlock(&thumbnailQueueMutex);
				}
				else if(currentRequests.size() < IMGCONNS) //If it's not already being requested and we still have more space for a new connection, request it
				{
					thumbnailRequests.pop_front();
					pthread_mutex_unlock(&thumbnailQueueMutex);

					//If it's not already being requested, request it
					if(!requested && CheckThumbnailListener(req.SubRequests.front().CompletedListener))
					{
						std::stringstream urlStream;
						urlStream << "http://" << STATICSERVER << "/" << req.ID.SaveID;
						if(req.ID.SaveDate)
						{
							urlStream << "_" << req.ID.SaveDate;
						}
						urlStream << "_small.pti";

#ifdef DEBUG
						std::cout << typeid(*this).name() << " Creating new request for " << req.ID.SaveID << ":" << req.ID.SaveDate << std::endl;
#endif

						req.HTTPContext = http_async_req_start(NULL, (char *)urlStream.str().c_str(), NULL, 0, 1);
						req.RequestTime = time(NULL);
						currentRequests.push_back(req);
					}
				}
				else
				{
					//Already full of requests
					pthread_mutex_unlock(&thumbnailQueueMutex);

				}
			}
		}
		else
		{
			pthread_mutex_unlock(&thumbnailQueueMutex);
		}

		std::list<ThumbnailRequest>::iterator iter = currentRequests.begin();
		std::list<ThumbnailRequest>::iterator end = currentRequests.end();
		while (iter != currentRequests.end())
		{
			lastAction = time(NULL);

			ThumbnailRequest req = *iter;
			Thumbnail * thumbnail = NULL;

			if(http_async_req_status(req.HTTPContext))
			{

				pixel * thumbData;
				char * data;
				int status, data_size, imgw, imgh;
				data = http_async_req_stop(req.HTTPContext, &status, &data_size);
				free(req.HTTPContext);

				if (status == 200 && data)
				{
					thumbData = Graphics::ptif_unpack(data, data_size, &imgw, &imgh);
					free(data);

					if(thumbData)
					{
						thumbnail = new Thumbnail(req.ID.SaveID, req.ID.SaveID, thumbData, ui::Point(imgw, imgh));
						free(thumbData);
					}
					else
					{
						thumbnail = new Thumbnail(req.ID.SaveID, req.ID.SaveID, thumbData, ui::Point(128, 128));
						free(thumbData);
					}

					if(thumbnailCache.size() >= THUMB_CACHE_SIZE)
					{
						delete thumbnailCache.front().second;
						thumbnailCache.pop_front();
					}
					thumbnailCache.push_back(std::pair<ThumbnailID, Thumbnail*>(req.ID, thumbnail));

					for(std::vector<ThumbnailSpec>::iterator specIter = req.SubRequests.begin(), specEnd = req.SubRequests.end(); specIter != specEnd; ++specIter)
					{
						Thumbnail * tempThumbnail = new Thumbnail(*thumbnail);
						tempThumbnail->Resize((*specIter).Width, (*specIter).Height);

						pthread_mutex_lock(&thumbnailQueueMutex);
						thumbnailComplete.push_back(std::pair<ThumbnailListener*, Thumbnail*>((*specIter).CompletedListener, tempThumbnail));
						pthread_mutex_unlock(&thumbnailQueueMutex);	
					}
				}
				else
				{
#ifdef DEBUG
					std::cout << typeid(*this).name() << " Request for " << req.ID.SaveID << ":" << req.ID.SaveDate << " failed with status " << status << std::endl;
#endif	
					if(data)
						free(data);
				}
				iter = currentRequests.erase(iter);
			}
			else
			{
				++iter;
			}
		}

	}
}

void ThumbnailBroker::RetrieveThumbnail(int saveID, int width, int height, ThumbnailListener * tListener)
{
	RetrieveThumbnail(saveID, 0, width, height, tListener);
}

bool ThumbnailBroker::CheckThumbnailListener(ThumbnailListener * tListener)
{
	pthread_mutex_lock(&listenersMutex);
	int count = std::count(validListeners.begin(), validListeners.end(), tListener);
	pthread_mutex_unlock(&listenersMutex);

	return count;
}

void ThumbnailBroker::AttachThumbnailListener(ThumbnailListener * tListener)
{
	pthread_mutex_lock(&listenersMutex);
	validListeners.push_back(tListener);
	pthread_mutex_unlock(&listenersMutex);
}

void ThumbnailBroker::DetachThumbnailListener(ThumbnailListener * tListener)
{
	pthread_mutex_lock(&listenersMutex);
	std::remove(validListeners.begin(), validListeners.end(), tListener);
	pthread_mutex_unlock(&listenersMutex);
}