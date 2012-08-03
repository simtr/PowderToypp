#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <time.h>
#include <stdio.h>
#include <deque>

#ifdef WIN
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "Config.h"
#include "Client.h"
#include "MD5.h"
#include "graphics/Graphics.h"
#include "Misc.h"

#include "simulation/SaveRenderer.h"
#include "interface/Point.h"
#include "client/SaveInfo.h"
#include "ClientListener.h"
#include "Update.h"
#include "ThumbnailBroker.h"

extern "C"
{
#if defined(WIN) && !defined(__GNUC__)
#include <io.h>
#else
#include <dirent.h>
#endif
}

Client::Client():
	authUser(0, ""),
	updateAvailable(false)
{
	int i = 0;
	std::string proxyString("");
	for(i = 0; i < THUMB_CACHE_SIZE; i++)
	{
		thumbnailCache[i] = NULL;
	}
	for(i = 0; i < IMGCONNS; i++)
	{
		activeThumbRequests[i] = NULL;
		activeThumbRequestTimes[i] = 0;
		activeThumbRequestCompleteTimes[i] = 0;
	}

	//Read config
	std::ifstream configFile;
	configFile.open("powder.pref", ios::binary);
	if(configFile)
	{
		int fsize = configFile.tellg();
		configFile.seekg(0, std::ios::end);
		fsize = configFile.tellg() - (std::streampos)fsize;
		configFile.seekg(0, ios::beg);
		if(fsize)
		{
			json::Reader::Read(configDocument, configFile);
			try
			{
				authUser.ID = ((json::Number)(configDocument["User"]["ID"])).Value();
				authUser.SessionID = ((json::String)(configDocument["User"]["SessionID"])).Value();
				authUser.SessionKey = ((json::String)(configDocument["User"]["SessionKey"])).Value();
				authUser.Username = ((json::String)(configDocument["User"]["Username"])).Value();

				std::string userElevation = ((json::String)(configDocument["User"]["Elevation"])).Value();
				if(userElevation == "Admin")
					authUser.UserElevation = ElevationAdmin;
				else if(userElevation == "Mod")
					authUser.UserElevation = ElevationModerator;
				else
					authUser.UserElevation= ElevationNone;
			}
			catch (json::Exception &e)
			{
				authUser = User(0, "");
				std::cerr << "Error: Client [Read User data from pref] " << e.what() << std::endl;
			}
		}
		configFile.close();
	}

	if(GetPrefBool("version.update", false)==true)
	{
		SetPref("version.update", false);
		update_finish();
	}

	proxyString = GetPrefString("proxy", "");
	if(proxyString.length())
	{
		http_init((char *)proxyString.c_str());
	}
	else
	{
		http_init(NULL);
	}

	//Read stamps library
	std::ifstream stampsLib;
	stampsLib.open(STAMPS_DIR PATH_SEP "stamps.def", ios::binary);
	while(true)
	{
		char data[11];
		memset(data, 0, 11);
		if(stampsLib.readsome(data, 10)!=10)
			break;
		if(!data[0])
			break;
		stampIDs.push_back(data);
	}
	stampsLib.close();

	//Begin version check
	versionCheckRequest = http_async_req_start(NULL, SERVER "/Download/Version.json", NULL, 0, 1);
}

std::vector<std::string> Client::DirectorySearch(std::string directory, std::string search, std::string extension)
{
	std::vector<std::string> extensions;
	extensions.push_back(extension);
	return DirectorySearch(directory, search, extensions);
}

std::vector<std::string> Client::DirectorySearch(std::string directory, std::string search, std::vector<std::string> extensions)
{
	//Get full file listing
	std::vector<std::string> directoryList;
#if defined(WIN) && !defined(__GNUC__)
	//Windows
	struct _finddata_t currentFile;
	intptr_t findFileHandle;
	std::string fileMatch = directory + "*.*";
	findFileHandle = _findfirst(fileMatch.c_str(), &currentFile);
	if (findFileHandle == -1L)
	{
		printf("Unable to open directory\n");
		return std::vector<std::string>();
	}
	do
	{
		std::string currentFileName = std::string(currentFile.name);
		if(currentFileName.length()>4)
			directoryList.push_back(directory+currentFileName);
	}
	while (_findnext(findFileHandle, &currentFile) == 0);
	_findclose(findFileHandle);
#else
	//Linux or MinGW
	struct dirent * directoryEntry;
	DIR *directoryHandle = opendir(directory.c_str());
	if(!directoryHandle)
	{
		printf("Unable to open directory\n");
		return std::vector<std::string>();
	}
	while(directoryEntry = readdir(directoryHandle))
	{
		std::string currentFileName = std::string(directoryEntry->d_name);
		if(currentFileName.length()>4)
			directoryList.push_back(directory+currentFileName);
	}
	closedir(directoryHandle);
#endif

	std::vector<std::string> searchResults;
	for(std::vector<std::string>::iterator iter = directoryList.begin(), end = directoryList.end(); iter != end; ++iter)
	{
		std::string filename = *iter;
		bool extensionMatch = !extensions.size();
		for(std::vector<std::string>::iterator extIter = extensions.begin(), extEnd = extensions.end(); extIter != extEnd; ++extIter)
		{
			if(filename.find(*extIter)==filename.length()-(*extIter).length())
			{
				extensionMatch = true;
				break;
			}
		}
		bool searchMatch = !search.size();
		if(search.size() && filename.find(search)!=std::string::npos)
			searchMatch = true;
		
		if(searchMatch && extensionMatch)
			searchResults.push_back(filename);
	}

	//Filter results
	return searchResults;
}

void Client::WriteFile(std::vector<unsigned char> fileData, std::string filename)
{
	try
	{
		std::ofstream fileStream;
		fileStream.open(string(filename).c_str(), ios::binary);
		if(fileStream.is_open())
		{
			fileStream.write((char*)&fileData[0], fileData.size());
			fileStream.close();
		}
	}
	catch (std::exception & e)
	{
		std::cerr << "WriteFile:" << e.what() << std::endl;
		throw;
	}	
}

bool Client::FileExists(std::string filename)
{
	bool exists = false;
	try
	{
		std::ofstream fileStream;
		fileStream.open(string(filename).c_str(), ios::binary);
		if(fileStream.is_open())
		{
			exists = true;
			fileStream.close();
		}
	}
	catch (std::exception & e)
	{
		exists = false;
	}
	return exists;
}

void Client::WriteFile(std::vector<char> fileData, std::string filename)
{
	try
	{
		std::ofstream fileStream;
		fileStream.open(string(filename).c_str(), ios::binary);
		if(fileStream.is_open())
		{
			fileStream.write(&fileData[0], fileData.size());
			fileStream.close();
		}
	}
	catch (std::exception & e)
	{
		std::cerr << "WriteFile:" << e.what() << std::endl;
		throw;
	}	
}

std::vector<unsigned char> Client::ReadFile(std::string filename)
{
	try
	{
		std::ifstream fileStream;
		fileStream.open(string(filename).c_str(), ios::binary);
		if(fileStream.is_open())
		{
			fileStream.seekg(0, ios::end);
			size_t fileSize = fileStream.tellg();
			fileStream.seekg(0);

			unsigned char * tempData = new unsigned char[fileSize];
			fileStream.read((char *)tempData, fileSize);
			fileStream.close();

			std::vector<unsigned char> fileData;
			fileData.insert(fileData.end(), tempData, tempData+fileSize);
			delete[] tempData;

			return fileData;
		}
		else
		{
			return std::vector<unsigned char>();
		}
	}
	catch(std::exception & e)
	{
		std::cerr << "Readfile: " << e.what() << std::endl;
		throw;
	}
}

void Client::Tick()
{
	//Check thumbnail queue
	ThumbnailBroker::Ref().FlushThumbQueue();

	//Check status on version check request
	if(versionCheckRequest && http_async_req_status(versionCheckRequest))
	{
		int status;
		int dataLength;
		char * data = http_async_req_stop(versionCheckRequest, &status, &dataLength);
		versionCheckRequest = NULL;

		if(status != 200)
		{
			if(data)
				free(data);
		}
		else
		{
			std::istringstream dataStream(data);

			try
			{
				json::Object objDocument;
				json::Reader::Read(objDocument, dataStream);

				json::Object stableVersion = objDocument["Stable"];
				json::Object betaVersion = objDocument["Beta"];
				json::Object snapshotVersion = objDocument["Snapshot"];

				json::Number stableMajor = stableVersion["Major"];
				json::Number stableMinor = stableVersion["Minor"];
				json::Number stableBuild = stableVersion["Build"];
				json::String stableFile = stableVersion["File"];

				json::Number betaMajor = betaVersion["Major"];
				json::Number betaMinor = betaVersion["Minor"];
				json::Number betaBuild = betaVersion["Build"];
				json::String betaFile = betaVersion["File"];

				json::Number snapshotSnapshot = snapshotVersion["Snapshot"];
				json::String snapshotFile = snapshotVersion["File"];

				if(stableMajor.Value()>SAVE_VERSION || (stableMinor.Value()>MINOR_VERSION && stableMajor.Value()==SAVE_VERSION) || stableBuild.Value()>BUILD_NUM)
				{
					updateAvailable = true;
					updateInfo = UpdateInfo(stableMajor.Value(), stableMinor.Value(), stableBuild.Value(), stableFile.Value(), UpdateInfo::Stable);
				}

#ifdef BETA
				if(betaMajor.Value()>SAVE_VERSION || (betaMinor.Value()>MINOR_VERSION && betaMajor.Value()==SAVE_VERSION) || betaBuild.Value()>BUILD_NUM)
				{
					updateAvailable = true;
					updateInfo = UpdateInfo(betaMajor.Value(), betaMinor.Value(), betaBuild.Value(), betaFile.Value(), UpdateInfo::Beta);
				}
#endif

#ifdef SNAPSHOT
				if(snapshotSnapshot.Value() > SNAPSHOT_ID)
				{
					updateAvailable = true;
					updateInfo = UpdateInfo(snapshotSnapshot.Value(), snapshotFile.Value(), UpdateInfo::Snapshot);
				}
#endif

				if(updateAvailable)
				{
					notifyUpdateAvailable();
				}
			}
			catch (json::Exception &e)
			{
				//Do nothing
			}

			if(data)
				free(data);
		}
	}
}

UpdateInfo Client::GetUpdateInfo()
{
	return updateInfo;
}

void Client::notifyUpdateAvailable()
{
	for (std::vector<ClientListener*>::iterator iterator = listeners.begin(), end = listeners.end(); iterator != end; ++iterator)
	{
		(*iterator)->NotifyUpdateAvailable(this);
	}
}

void Client::notifyAuthUserChanged()
{
	for (std::vector<ClientListener*>::iterator iterator = listeners.begin(), end = listeners.end(); iterator != end; ++iterator)
	{
		(*iterator)->NotifyAuthUserChanged(this);
	}
}

void Client::AddListener(ClientListener * listener)
{
	listeners.push_back(listener);
}

void Client::RemoveListener(ClientListener * listener)
{
	for (std::vector<ClientListener*>::iterator iterator = listeners.begin(), end = listeners.end(); iterator != end; ++iterator)
	{
		if((*iterator) == listener)
		{
			listeners.erase(iterator);
			return;
		}
	}
}

void Client::Shutdown()
{
	ClearThumbnailRequests();
	http_done();

	//Save config
	std::ofstream configFile;
	configFile.open("powder.pref", ios::trunc);
	if(configFile)
	{
		if(authUser.ID)
		{
			configDocument["User"]["ID"] = json::Number(authUser.ID);
			configDocument["User"]["SessionID"] = json::String(authUser.SessionID);
			configDocument["User"]["SessionKey"] = json::String(authUser.SessionKey);
			configDocument["User"]["Username"] = json::String(authUser.Username);
			if(authUser.UserElevation == ElevationAdmin)
				configDocument["User"]["Elevation"] = json::String("Admin");
			else if(authUser.UserElevation == ElevationModerator)
				configDocument["User"]["Elevation"] = json::String("Mod");
			else
				configDocument["User"]["Elevation"] = json::String("None");
		}
		else
		{
			configDocument["User"] = json::Null();
		}
		json::Writer::Write(configDocument, configFile);
		configFile.close();
	}
}

Client::~Client()
{
}


void Client::SetAuthUser(User user)
{
	authUser = user;
	notifyAuthUserChanged();
}

User Client::GetAuthUser()
{
	return authUser;
}

RequestStatus Client::UploadSave(SaveInfo & save)
{
	lastError = "";
	int gameDataLength;
	char * gameData = NULL;
	int dataStatus;
	char * data;
	int dataLength = 0;
	std::stringstream userIDStream;
	userIDStream << authUser.ID;
	if(authUser.ID)
	{
		if(!save.GetGameSave())
		{
			lastError = "Empty game save";
			return RequestFailure;
		}
		save.SetID(0);

		gameData = save.GetGameSave()->Serialise(gameDataLength);

		if(!gameData)
		{
			lastError = "Cannot upload game save";
			return RequestFailure;
		}

		char * postNames[] = { "Name", "Description", "Data:save.bin", "Publish", NULL };
		char * postDatas[] = { (char *)(save.GetName().c_str()), (char *)(save.GetDescription().c_str()), gameData, (char *)(save.GetPublished()?"Public":"Private") };
		int postLengths[] = { save.GetName().length(), save.GetDescription().length(), gameDataLength, save.GetPublished()?6:7 };
		//std::cout << postNames[0] << " " << postDatas[0] << " " << postLengths[0] << std::endl;
		data = http_multipart_post("http://" SERVER "/Save.api", postNames, postDatas, postLengths, (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return RequestFailure;
	}
	if(data && dataStatus == 200)
	{
		if(strncmp((const char *)data, "OK", 2)!=0)
		{
			if(gameData) free(gameData);
			free(data);
			lastError = std::string((const char *)data);
			return RequestFailure;
		}
		else
		{
			int tempID;
			std::stringstream saveIDStream((char *)(data+3));
			saveIDStream >> tempID;
			if(!tempID)
			{
				lastError = "Server did not return Save ID";
				return RequestFailure;
			}
			else
			{
				save.SetID(tempID);
			}
		}
		free(data);
		if(gameData) free(gameData);
		return RequestOkay;
	}
	else if(data)
	{
		free(data);
	}
	if(gameData) free(gameData);
	return RequestFailure;
}

SaveFile * Client::GetStamp(string stampID)
{
	std::ifstream stampFile;
	stampFile.open(string(STAMPS_DIR PATH_SEP + stampID + ".stm").c_str(), ios::binary);
	if(stampFile.is_open())
	{
		stampFile.seekg(0, ios::end);
		size_t fileSize = stampFile.tellg();
		stampFile.seekg(0);

		unsigned char * tempData = (unsigned char *)malloc(fileSize);
		stampFile.read((char *)tempData, fileSize);
		stampFile.close();

		SaveFile * file = new SaveFile(string(stampID).c_str());
		GameSave * tempSave = new GameSave((char *)tempData, fileSize);
		file->SetGameSave(tempSave);
		return file;
	}
	else
	{
		return NULL;
	}
}

void Client::DeleteStamp(string stampID)
{
	for (std::list<string>::iterator iterator = stampIDs.begin(), end = stampIDs.end(); iterator != end; ++iterator)
	{
		if((*iterator) == stampID)
		{
			stringstream stampFilename;
			stampFilename << STAMPS_DIR;
			stampFilename << PATH_SEP;
			stampFilename << stampID;
			stampFilename << ".stm";
			remove(stampFilename.str().c_str());
			stampIDs.erase(iterator);
			return;
		}
	}
}

string Client::AddStamp(GameSave * saveData)
{
	unsigned t=(unsigned)time(NULL);
	if (lastStampTime!=t)
	{
		lastStampTime=t;
		lastStampName=0;
	}
	else
		lastStampName++;
	std::stringstream saveID;
	//sprintf(saveID, "%08x%02x", lastStampTime, lastStampName);
	saveID
	<< std::setw(8) << std::setfill('0') << std::hex << lastStampTime
	<< std::setw(2) << std::setfill('0') << std::hex << lastStampName;

#ifdef WIN
	_mkdir(STAMPS_DIR);
#else
	mkdir(STAMPS_DIR, 0755);
#endif

	int gameDataLength;
	char * gameData = saveData->Serialise(gameDataLength);

	std::ofstream stampStream;
	stampStream.open(string(STAMPS_DIR PATH_SEP + saveID.str()+".stm").c_str(), ios::binary);
	stampStream.write((const char *)gameData, gameDataLength);
	stampStream.close();

	stampIDs.push_front(saveID.str());

	updateStamps();

	return saveID.str();
}

void Client::updateStamps()
{

#ifdef WIN
	_mkdir(STAMPS_DIR);
#else
	mkdir(STAMPS_DIR, 0755);
#endif

	std::ofstream stampsStream;
	stampsStream.open(string(STAMPS_DIR PATH_SEP "stamps.def").c_str(), ios::binary);
	for (std::list<string>::const_iterator iterator = stampIDs.begin(), end = stampIDs.end(); iterator != end; ++iterator)
	{
		stampsStream.write((*iterator).c_str(), 10);
	}
	stampsStream.write("\0", 1);
	stampsStream.close();
	return;
}

int Client::GetStampsCount()
{
	return stampIDs.size();
}

vector<string> Client::GetStamps(int start, int count)
{
	if(start+count > stampIDs.size()) {
		if(start > stampIDs.size())
			return vector<string>();
		count = stampIDs.size()-start;
	}

	vector<string> stampRange;
	int index = 0;
	for (std::list<string>::const_iterator iterator = stampIDs.begin(), end = stampIDs.end(); iterator != end; ++iterator, ++index) {
		if(index>=start && index < start+count)
			stampRange.push_back(*iterator);
	}
	return stampRange;
}

RequestStatus Client::ExecVote(int saveID, int direction)
{
	lastError = "";
	int dataStatus;
	char * data;
	int dataLength = 0;
	std::stringstream idStream;
	idStream << saveID;
	std::string directionS;
	if(direction==1)
	{
		directionS = "Up";
	}
	else
	{
		directionS = "Down";
	}
	std::stringstream userIDStream;
	userIDStream << authUser.ID;
	if(authUser.ID)
	{
		char * postNames[] = { "ID", "Action", NULL };
		char * postDatas[] = { (char*)(idStream.str().c_str()), (char*)(directionS.c_str()) };
		int postLengths[] = { idStream.str().length(), directionS.length() };
		//std::cout << postNames[0] << " " << postDatas[0] << " " << postLengths[0] << std::endl;
		data = http_multipart_post("http://" SERVER "/Vote.api", postNames, postDatas, postLengths, (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return RequestFailure;
	}
	if(data && dataStatus == 200)
	{
		if(strncmp((const char *)data, "OK", 2)!=0)
		{
			free(data);
			lastError = std::string((const char *)data);
			return RequestFailure;
		}
		free(data);
		return RequestOkay;
	}
	else if(data)
	{
		free(data);
	}
	lastError = http_ret_text(dataStatus);
	return RequestFailure;
}

unsigned char * Client::GetSaveData(int saveID, int saveDate, int & dataLength)
{
	lastError = "";
	int dataStatus;
	unsigned char * data;
	dataLength = 0;
	std::stringstream urlStream;
	if(saveDate)
	{
		urlStream << "http://" << STATICSERVER << "/" << saveID << "_" << saveDate << ".cps";
	}
	else
	{
		urlStream << "http://" << STATICSERVER << "/" << saveID << ".cps";
	}

	data = (unsigned char *)http_simple_get((char *)urlStream.str().c_str(), &dataStatus, &dataLength);
	if(data && dataStatus == 200)
	{
		return data;
	}
	else if(data)
	{
		free(data);
	}
	return NULL;
}

LoginStatus Client::Login(string username, string password, User & user)
{
	lastError = "";
	std::stringstream urlStream;
	std::stringstream hashStream;
	char passwordHash[33];
	char totalHash[33];

	user.ID = 0;
	user.Username = "";
	user.SessionID = "";
	user.SessionKey = "";

	//Doop
	md5_ascii(passwordHash, (const unsigned char *)password.c_str(), password.length());
	passwordHash[32] = 0;
	hashStream << username << "-" << passwordHash;
	md5_ascii(totalHash, (const unsigned char *)(hashStream.str().c_str()), hashStream.str().length());
	totalHash[32] = 0;

	char * data;
	int dataStatus, dataLength;
	char * postNames[] = { "Username", "Hash", NULL };
	char * postDatas[] = { (char*)username.c_str(), totalHash };
	int postLengths[] = { username.length(), 32 };
	data = http_multipart_post("http://" SERVER "/Login.json", postNames, postDatas, postLengths, NULL, NULL, NULL, &dataStatus, &dataLength);
	//data = http_auth_get("http://" SERVER "/Login.json", (char*)username.c_str(), (char*)password.c_str(), NULL, &dataStatus, &dataLength);
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Object objDocument;
			json::Reader::Read(objDocument, dataStream);
			json::Number tempStatus = objDocument["Status"];

			free(data);
			if(tempStatus.Value() == 1)
			{
				json::Number userIDTemp = objDocument["UserID"];
				json::String sessionIDTemp = objDocument["SessionID"];
				json::String sessionKeyTemp = objDocument["SessionKey"];
				json::String userElevationTemp = objDocument["Elevation"];
				user.Username = username;
				user.ID = userIDTemp.Value();
				user.SessionID = sessionIDTemp.Value();
				user.SessionKey = sessionKeyTemp.Value();
				std::string userElevation = userElevationTemp.Value();
				if(userElevation == "Admin")
					user.UserElevation = ElevationAdmin;
				else if(userElevation == "Mod")
					user.UserElevation = ElevationModerator;
				else
					user.UserElevation= ElevationNone;
				return LoginOkay;
			}
			else
			{
				json::String tempError = objDocument["Error"];
				lastError = tempError.Value();
				return LoginError;
			}
		}
		catch (json::Exception &e)
		{
			lastError = "Server responded with crap";
			return LoginError;
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
	}
	if(data)
	{
		free(data);
	}
	return LoginError;
}

RequestStatus Client::DeleteSave(int saveID)
{
	lastError = "";
	std::vector<string> * tags = NULL;
	std::stringstream urlStream;
	char * data = NULL;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse/Delete.json?ID=" << saveID << "&Mode=Delete&Key=" << authUser.SessionKey;
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;
		data = http_auth_get((char *)urlStream.str().c_str(), (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return RequestFailure;
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Object objDocument;
			json::Reader::Read(objDocument, dataStream);

			int status = ((json::Number)objDocument["Status"]).Value();

			if(status!=1)
				goto failure;
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
			goto failure;
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
		goto failure;
	}
	if(data)
		free(data);
	return RequestOkay;
failure:
	if(data)
		free(data);
	return RequestFailure;
}

RequestStatus Client::AddComment(int saveID, std::string comment)
{
	lastError = "";
	std::vector<string> * tags = NULL;
	std::stringstream urlStream;
	char * data = NULL;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse/Comments.json?ID=" << saveID << "&Key=" << authUser.SessionKey;
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;
		
		char * postNames[] = { "Comment", NULL };
		char * postDatas[] = { (char*)(comment.c_str()) };
		int postLengths[] = { comment.length() };
		data = http_multipart_post((char *)urlStream.str().c_str(), postNames, postDatas, postLengths, (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return RequestFailure;
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Object objDocument;
			json::Reader::Read(objDocument, dataStream);

			int status = ((json::Number)objDocument["Status"]).Value();

			if(status!=1)
			{
				lastError = ((json::Number)objDocument["Error"]).Value();
			}

			if(status!=1)
				goto failure;
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
			goto failure;
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
		goto failure;
	}
	if(data)
		free(data);
	return RequestOkay;
failure:
	if(data)
		free(data);
	return RequestFailure;
}

RequestStatus Client::FavouriteSave(int saveID, bool favourite)
{
	lastError = "";
	std::vector<string> * tags = NULL;
	std::stringstream urlStream;
	char * data = NULL;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse/Favourite.json?ID=" << saveID << "&Key=" << authUser.SessionKey;
	if(!favourite)
		urlStream << "&Mode=Remove";
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;
		data = http_auth_get((char *)urlStream.str().c_str(), (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return RequestFailure;
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Object objDocument;
			json::Reader::Read(objDocument, dataStream);

			int status = ((json::Number)objDocument["Status"]).Value();

			if(status!=1)
			{
				lastError = ((json::String)objDocument["Error"]).Value();
				goto failure;
			}
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
			goto failure;
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
		goto failure;
	}
	if(data)
		free(data);
	return RequestOkay;
failure:
	if(data)
		free(data);
	return RequestFailure;
}

RequestStatus Client::ReportSave(int saveID, std::string message)
{
	lastError = "";
	std::vector<string> * tags = NULL;
	std::stringstream urlStream;
	char * data = NULL;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse/Report.json?ID=" << saveID << "&Key=" << authUser.SessionKey;
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;

		char * postNames[] = { "Reason", NULL };
		char * postDatas[] = { (char*)(message.c_str()) };
		int postLengths[] = { message.length() };
		data = http_multipart_post((char *)urlStream.str().c_str(), postNames, postDatas, postLengths, (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return RequestFailure;
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Object objDocument;
			json::Reader::Read(objDocument, dataStream);

			int status = ((json::Number)objDocument["Status"]).Value();

			if(status!=1)
			{
				lastError = ((json::String)objDocument["Error"]).Value();
				goto failure;
			}
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
			goto failure;
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
		goto failure;
	}
	if(data)
		free(data);
	return RequestOkay;
failure:
	if(data)
		free(data);
	return RequestFailure;
}

RequestStatus Client::UnpublishSave(int saveID)
{
	lastError = "";
	std::vector<string> * tags = NULL;
	std::stringstream urlStream;
	char * data = NULL;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse/Delete.json?ID=" << saveID << "&Mode=Unpublish&Key=" << authUser.SessionKey;
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;
		data = http_auth_get((char *)urlStream.str().c_str(), (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return RequestFailure;
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Object objDocument;
			json::Reader::Read(objDocument, dataStream);

			int status = ((json::Number)objDocument["Status"]).Value();

			if(status!=1)
				goto failure;
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
			goto failure;
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
		goto failure;
	}
	if(data)
		free(data);
	return RequestOkay;
failure:
	if(data)
		free(data);
	return RequestFailure;
}

SaveInfo * Client::GetSave(int saveID, int saveDate)
{
	lastError = "";
	std::stringstream urlStream;
	urlStream << "http://" << SERVER  << "/Browse/View.json?ID=" << saveID;
	if(saveDate)
	{
		urlStream << "&Date=" << saveDate;
	}
	char * data;
	int dataStatus, dataLength;
	//Save(int _id, int _votesUp, int _votesDown, string _userName, string _name, string description_, string date_, bool published_):
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;
		data = http_auth_get((char *)urlStream.str().c_str(), (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		data = http_simple_get((char *)urlStream.str().c_str(), &dataStatus, &dataLength);
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Object objDocument;
			json::Reader::Read(objDocument, dataStream);

			json::Number tempID = objDocument["ID"];
			json::Number tempScoreUp = objDocument["ScoreUp"];
			json::Number tempScoreDown = objDocument["ScoreDown"];
			json::Number tempMyScore = objDocument["ScoreMine"];
			json::String tempUsername = objDocument["Username"];
			json::String tempName = objDocument["Name"];
			json::String tempDescription = objDocument["Description"];
			json::Number tempDate = objDocument["Date"];
			json::Boolean tempPublished = objDocument["Published"];
			json::Boolean tempFavourite = objDocument["Favourite"];
			json::Number tempComments = objDocument["Comments"];

			json::Array tagsArray = objDocument["Tags"];
			vector<string> tempTags;

			for(int j = 0; j < tagsArray.Size(); j++)
			{
				json::String tempTag = tagsArray[j];
				tempTags.push_back(tempTag.Value());
			}

			SaveInfo * tempSave = new SaveInfo(
					tempID.Value(),
					tempDate.Value(),
					tempScoreUp.Value(),
					tempScoreDown.Value(),
					tempMyScore.Value(),
					tempUsername.Value(),
					tempName.Value(),
					tempDescription.Value(),
					tempPublished.Value(),
					tempTags
					);
			tempSave->Comments = tempComments.Value();
			tempSave->Favourite = tempFavourite.Value();
			return tempSave;
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
			return NULL;
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
	}
	return NULL;
}

Thumbnail * Client::GetPreview(int saveID, int saveDate)
{
	std::stringstream urlStream;
	urlStream << "http://" << STATICSERVER  << "/" << saveID;
	if(saveDate)
	{
		urlStream << "_" << saveDate;
	}
	urlStream << "_large.pti";
	pixel * thumbData;
	char * data;
	int status, data_size, imgw, imgh;
	data = http_simple_get((char *)urlStream.str().c_str(), &status, &data_size);
	if (status == 200 && data)
	{
		thumbData = Graphics::ptif_unpack(data, data_size, &imgw, &imgh);
		if(data)
		{
			free(data);
		}
		if(thumbData)
		{
			return new Thumbnail(saveID, saveDate, thumbData, ui::Point(imgw, imgh));
			free(thumbData);
		}
		else
		{
			thumbData = (pixel *)malloc((128*128) * PIXELSIZE);
			return new Thumbnail(saveID, saveDate, thumbData, ui::Point(128, 128));
			free(thumbData);
		}
	}
	else
	{
		if(data)
		{
			free(data);
		}
	}
	return new Thumbnail(saveID, saveDate, (pixel *)malloc((128*128) * PIXELSIZE), ui::Point(128, 128));
}

std::vector<SaveComment*> * Client::GetComments(int saveID, int start, int count)
{
	lastError = "";
	std::vector<SaveComment*> * commentArray = new std::vector<SaveComment*>();

	std::stringstream urlStream;
	char * data;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse/Comments.json?ID=" << saveID << "&Start=" << start << "&Count=" << count;
	data = http_simple_get((char *)urlStream.str().c_str(), &dataStatus, &dataLength);
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Array commentsArray;
			json::Reader::Read(commentsArray, dataStream);

			for(int j = 0; j < commentsArray.Size(); j++)
			{
				json::Number tempUserID = commentsArray[j]["UserID"];
				json::String tempUsername = commentsArray[j]["Username"];
				json::String tempComment = commentsArray[j]["Text"];
				commentArray->push_back(
							new SaveComment(
								tempUserID.Value(),
								tempUsername.Value(),
								tempComment.Value()
								)
							);
			}
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
	}
	if(data)
		free(data);
	return commentArray;
}

std::vector<SaveInfo*> * Client::SearchSaves(int start, int count, string query, string sort, std::string category, int & resultCount)
{
	lastError = "";
	resultCount = 0;
	std::vector<SaveInfo*> * saveArray = new std::vector<SaveInfo*>();
	std::stringstream urlStream;
	char * data;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse.json?Start=" << start << "&Count=" << count;
	if(query.length() || sort.length())
	{
		urlStream << "&Search_Query=";
		if(query.length())
			urlStream << URLEscape(query);
		if(sort == "date")
		{
			if(query.length())
				urlStream << URLEscape(" ");
			urlStream << URLEscape("sort:") << URLEscape(sort);
		}
	}
	if(category.length())
	{
		urlStream << "&Category=" << URLEscape(category);
	}
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;
		data = http_auth_get((char *)urlStream.str().c_str(), (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		data = http_simple_get((char *)urlStream.str().c_str(), &dataStatus, &dataLength);
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Object objDocument;
			json::Reader::Read(objDocument, dataStream);

			json::Number tempCount = objDocument["Count"];
			resultCount = tempCount.Value();
			json::Array savesArray = objDocument["Saves"];
			for(int j = 0; j < savesArray.Size(); j++)
			{
				json::Number tempID = savesArray[j]["ID"];
				json::Number tempDate = savesArray[j]["Date"];
				json::Number tempScoreUp = savesArray[j]["ScoreUp"];
				json::Number tempScoreDown = savesArray[j]["ScoreDown"];
				json::String tempUsername = savesArray[j]["Username"];
				json::String tempName = savesArray[j]["Name"];
				saveArray->push_back(
							new SaveInfo(
								tempID.Value(),
								tempDate.Value(),
								tempScoreUp.Value(),
								tempScoreDown.Value(),
								tempUsername.Value(),
								tempName.Value()
								)
							);
			}
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
	}
	if(data)
		free(data);
	return saveArray;
}

void Client::ClearThumbnailRequests()
{
	for(int i = 0; i < IMGCONNS; i++)
	{
		if(activeThumbRequests[i])
		{
			http_async_req_close(activeThumbRequests[i]);
			activeThumbRequests[i] = NULL;
			activeThumbRequestTimes[i] = 0;
			activeThumbRequestCompleteTimes[i] = 0;
		}
	}
}

Thumbnail * Client::GetThumbnail(int saveID, int saveDate)
{
	std::stringstream urlStream;
	std::stringstream idStream;
	int i = 0, currentTime = time(NULL);
	//Check active requests for any "forgotten" requests
	for(i = 0; i < IMGCONNS; i++)
	{
		//If the request is active, and we've recieved a response
		if(activeThumbRequests[i] && http_async_req_status(activeThumbRequests[i]))
		{
			//If we haven't already, mark the request as completed
			if(!activeThumbRequestCompleteTimes[i])
			{
				activeThumbRequestCompleteTimes[i] = time(NULL);
			}
			else if(activeThumbRequestCompleteTimes[i] < (currentTime-2)) //Otherwise, if it completed more than 2 seconds ago, destroy it.
			{
				http_async_req_close(activeThumbRequests[i]);
				activeThumbRequests[i] = NULL;
				activeThumbRequestTimes[i] = 0;
				activeThumbRequestCompleteTimes[i] = 0;
			}
		}
	}
	for(i = 0; i < THUMB_CACHE_SIZE; i++)
	{
		if(thumbnailCache[i] && thumbnailCache[i]->ID == saveID && thumbnailCache[i]->Datestamp == saveDate)
			return thumbnailCache[i];
	}
	urlStream << "http://" << STATICSERVER  << "/" << saveID;
	if(saveDate)
	{
		urlStream << "_" << saveDate;
	}
	urlStream << "_small.pti";
	idStream << saveID << ":" << saveDate;
	std::string idString = idStream.str();
	bool found = false;
	for(i = 0; i < IMGCONNS; i++)
	{
		if(activeThumbRequests[i] && activeThumbRequestIDs[i] == idString)
		{
			found = true;
			if(http_async_req_status(activeThumbRequests[i]))
			{
				pixel * thumbData;
				char * data;
				int status, data_size, imgw, imgh;
				data = http_async_req_stop(activeThumbRequests[i], &status, &data_size);
				free(activeThumbRequests[i]);
				activeThumbRequests[i] = NULL;
				if (status == 200 && data)
				{
					thumbData = Graphics::ptif_unpack(data, data_size, &imgw, &imgh);
					if(data)
					{
						free(data);
					}
					thumbnailCacheNextID %= THUMB_CACHE_SIZE;
					if(thumbnailCache[thumbnailCacheNextID])
					{
						delete thumbnailCache[thumbnailCacheNextID];
					}
					if(thumbData)
					{
						thumbnailCache[thumbnailCacheNextID] = new Thumbnail(saveID, saveDate, thumbData, ui::Point(imgw, imgh));
						free(thumbData);
					}
					else
					{
						thumbData = (pixel *)malloc((128*128) * PIXELSIZE);
						thumbnailCache[thumbnailCacheNextID] = new Thumbnail(saveID, saveDate, thumbData, ui::Point(128, 128));
						free(thumbData);
					}
					return thumbnailCache[thumbnailCacheNextID++];
				}
				else
				{
					if(data)
					{
						free(data);
					}
					thumbnailCacheNextID %= THUMB_CACHE_SIZE;
					if(thumbnailCache[thumbnailCacheNextID])
					{
						delete thumbnailCache[thumbnailCacheNextID];
					}
					thumbData = (pixel *)malloc((128*128) * PIXELSIZE);
					thumbnailCache[thumbnailCacheNextID] = new Thumbnail(saveID, saveDate, thumbData, ui::Point(128, 128));
					free(thumbData);
					return thumbnailCache[thumbnailCacheNextID++];
				}
			}
		}
	}
	if(!found)
	{
		for(i = 0; i < IMGCONNS; i++)
		{
			if(!activeThumbRequests[i])
			{
				activeThumbRequests[i] = http_async_req_start(NULL, (char *)urlStream.str().c_str(), NULL, 0, 1);
				activeThumbRequestTimes[i] = currentTime;
				activeThumbRequestCompleteTimes[i] = 0;
				activeThumbRequestIDs[i] = idString;
				return NULL;
			}
		}
	}
	//http_async_req_start(http, urlStream.str().c_str(), NULL, 0, 1);
	return NULL;
}

std::vector<string> * Client::RemoveTag(int saveID, string tag)
{
	lastError = "";
	std::vector<string> * tags = NULL;
	std::stringstream urlStream;
	char * data = NULL;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse/EditTag.json?Op=delete&ID=" << saveID << "&Tag=" << tag << "&Key=" << authUser.SessionKey;;
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;
		data = http_auth_get((char *)urlStream.str().c_str(), (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return NULL;
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Array tagsArray;
			json::Reader::Read(tagsArray, dataStream);

			tags = new std::vector<string>();

			for(int j = 0; j < tagsArray.Size(); j++)
			{
				json::String tempTag = tagsArray[j];
				tags->push_back(tempTag.Value());
			}
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
	}
	if(data)
		free(data);
	return tags;
}

std::vector<string> * Client::AddTag(int saveID, string tag)
{
	lastError = "";
	std::vector<string> * tags = NULL;
	std::stringstream urlStream;
	char * data = NULL;
	int dataStatus, dataLength;
	urlStream << "http://" << SERVER << "/Browse/EditTag.json?Op=add&ID=" << saveID << "&Tag=" << tag << "&Key=" << authUser.SessionKey;
	if(authUser.ID)
	{
		std::stringstream userIDStream;
		userIDStream << authUser.ID;
		data = http_auth_get((char *)urlStream.str().c_str(), (char *)(userIDStream.str().c_str()), NULL, (char *)(authUser.SessionID.c_str()), &dataStatus, &dataLength);
	}
	else
	{
		lastError = "Not authenticated";
		return NULL;
	}
	if(dataStatus == 200 && data)
	{
		try
		{
			std::istringstream dataStream(data);
			json::Array tagsArray;
			json::Reader::Read(tagsArray, dataStream);

			tags = new std::vector<string>();

			for(int j = 0; j < tagsArray.Size(); j++)
			{
				json::String tempTag = tagsArray[j];
				tags->push_back(tempTag.Value());
			}
		}
		catch (json::Exception &e)
		{
			lastError = "Could not read response";
		}
	}
	else
	{
		lastError = http_ret_text(dataStatus);
	}
	if(data)
		free(data);
	return tags;
}

vector<std::string> Client::explodePropertyString(std::string property)
{
	vector<string> stringArray;
	string current = "";
	for (string::iterator iter = property.begin(); iter != property.end(); ++iter) {
		if (*iter == '.') {
			if (current.length() > 0) {
				stringArray.push_back(current);
				current = "";
			}
		} else {
			current += *iter;
		}
	}
	if(current.length() > 0)
		stringArray.push_back(current);
	return stringArray;
}

std::string Client::GetPrefString(std::string property, std::string defaultValue)
{
	try
	{
		json::String value = GetPref(property);
		return value.Value();
	}
	catch (json::Exception & e)
	{

	}
	return defaultValue;
}

double Client::GetPrefNumber(std::string property, double defaultValue)
{
	try
	{
		json::Number value = GetPref(property);
		return value.Value();
	}
	catch (json::Exception & e)
	{

	}
	return defaultValue;
}

int Client::GetPrefInteger(std::string property, int defaultValue)
{
	try
	{
		std::stringstream defHexInt;
		defHexInt << std::hex << defaultValue;

		std::string hexString = GetPrefString(property, defHexInt.str());
		int finalValue = defaultValue;

		std::stringstream hexInt;
		hexInt << hexString;

		hexInt >> std::hex >> finalValue;

		return finalValue;
	}
	catch (json::Exception & e)
	{

	}
	catch(exception & e)
	{

	}
	return defaultValue;
}

vector<string> Client::GetPrefStringArray(std::string property)
{
	try
	{
		json::Array value = GetPref(property);
		vector<string> strArray;
		for(json::Array::iterator iter = value.Begin(); iter != value.End(); ++iter)
		{
			try
			{
				json::String cValue = *iter;
				strArray.push_back(cValue.Value());
			}
			catch (json::Exception & e)
			{
				
			}
		}
		return strArray;
	}
	catch (json::Exception & e)
	{

	}
	return vector<string>();
}

vector<double> Client::GetPrefNumberArray(std::string property)
{
	try
	{
		json::Array value = GetPref(property);
		vector<double> strArray;
		for(json::Array::iterator iter = value.Begin(); iter != value.End(); ++iter)
		{
			try
			{
				json::Number cValue = *iter;
				strArray.push_back(cValue.Value());
			}
			catch (json::Exception & e)
			{
				
			}
		}
		return strArray;
	}
	catch (json::Exception & e)
	{

	}
	return vector<double>();
}

vector<int> Client::GetPrefIntegerArray(std::string property)
{
	try
	{
		json::Array value = GetPref(property);
		vector<int> intArray;
		for(json::Array::iterator iter = value.Begin(); iter != value.End(); ++iter)
		{
			try
			{
				json::String cValue = *iter;
				int finalValue = 0;
		
				std::string hexString = cValue.Value();
				std::stringstream hexInt;
				hexInt << std::hex << hexString;
				hexInt >> finalValue;

				intArray.push_back(finalValue);
			}
			catch (json::Exception & e)
			{

			}
		}
		return intArray;
	}
	catch (json::Exception & e)
	{

	}
	return vector<int>();
}

vector<bool> Client::GetPrefBoolArray(std::string property)
{
	try
	{
		json::Array value = GetPref(property);
		vector<bool> strArray;
		for(json::Array::iterator iter = value.Begin(); iter != value.End(); ++iter)
		{
			try
			{
				json::Boolean cValue = *iter;
				strArray.push_back(cValue.Value());
			}
			catch (json::Exception & e)
			{
				
			}
		}
		return strArray;
	}
	catch (json::Exception & e)
	{

	}
	return vector<bool>();
}

bool Client::GetPrefBool(std::string property, bool defaultValue)
{
	try
	{
		json::Boolean value = GetPref(property);
		return value.Value();
	}
	catch (json::Exception & e)
	{

	}
	return defaultValue;
}

void Client::SetPref(std::string property, std::string value)
{
	json::UnknownElement stringValue = json::String(value);
	SetPref(property, stringValue);
}

void Client::SetPref(std::string property, double value)
{
	json::UnknownElement numberValue = json::Number(value);
	SetPref(property, numberValue);
}

void Client::SetPref(std::string property, int value)
{
	std::stringstream hexInt;
	hexInt << std::hex << value;
	json::UnknownElement intValue = json::String(hexInt.str());
	SetPref(property, intValue);
}

void Client::SetPref(std::string property, vector<string> value)
{
	json::Array newArray;
	for(vector<string>::iterator iter = value.begin(); iter != value.end(); ++iter)
	{
		newArray.Insert(json::String(*iter));
	}
	json::UnknownElement newArrayValue = newArray;
	SetPref(property, newArrayValue);
}

void Client::SetPref(std::string property, vector<double> value)
{
	json::Array newArray;
	for(vector<double>::iterator iter = value.begin(); iter != value.end(); ++iter)
	{
		newArray.Insert(json::Number(*iter));
	}
	json::UnknownElement newArrayValue = newArray;
	SetPref(property, newArrayValue);
}

void Client::SetPref(std::string property, vector<bool> value)
{
	json::Array newArray;
	for(vector<bool>::iterator iter = value.begin(); iter != value.end(); ++iter)
	{
		newArray.Insert(json::Boolean(*iter));
	}
	json::UnknownElement newArrayValue = newArray;
	SetPref(property, newArrayValue);
}

void Client::SetPref(std::string property, vector<int> value)
{
	json::Array newArray;
	for(vector<int>::iterator iter = value.begin(); iter != value.end(); ++iter)
	{
		std::stringstream hexInt;
		hexInt << std::hex << *iter;

		newArray.Insert(json::String(hexInt.str()));
	}
	json::UnknownElement newArrayValue = newArray;
	SetPref(property, newArrayValue);
}

void Client::SetPref(std::string property, bool value)
{
	json::UnknownElement boolValue = json::Boolean(value);
	SetPref(property, boolValue);
}

json::UnknownElement Client::GetPref(std::string property)
{
	vector<string> pTokens = Client::explodePropertyString(property);
	const json::UnknownElement & configDocumentCopy = configDocument;
	json::UnknownElement currentRef = configDocumentCopy;
	for(vector<string>::iterator iter = pTokens.begin(); iter != pTokens.end(); ++iter)
	{
		currentRef = ((const json::UnknownElement &)currentRef)[*iter];
	}
	return currentRef;
}

void Client::setPrefR(std::deque<string> tokens, json::UnknownElement & element, json::UnknownElement & value)
{
	if(tokens.size())
	{
		std::string token = tokens.front();
		tokens.pop_front();
		setPrefR(tokens, element[token], value);
	}
	else
		element = value;
}

void Client::SetPref(std::string property, json::UnknownElement & value)
{
	vector<string> pTokens = Client::explodePropertyString(property);
	deque<string> dTokens(pTokens.begin(), pTokens.end());
	string token = dTokens.front();
	dTokens.pop_front();
	setPrefR(dTokens, configDocument[token], value);
}
