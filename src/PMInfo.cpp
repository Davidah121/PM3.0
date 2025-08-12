#include "PMInfo.h"
#include <SimpleFile.h>
#include <ctime>
#include "EncryptWrapper.h"


std::string PMDatabase::getCurrentDate()
{
	time_t currentTime = time(nullptr);
	tm* tmStruct = std::localtime(&currentTime);
	char buffer[1024];
	std::strftime(buffer, 1024, "%D %TZ", tmStruct);

	return std::string(buffer);
}

PMDatabase::PMDatabase()
{
	
}
PMDatabase::~PMDatabase()
{
	clear();
}

PMInfo* PMDatabase::getEntry(std::string s)
{
	auto it = entries.find(s);
	if(it != entries.end())
		return it->second;
	return nullptr;
}


std::string PMDatabase::convertEntryToJSON(PMInfo* entry)
{
	if(entry)
	{
		std::string temp = "{\n";
		temp += "\t\"Name\": \"" + entry->Name + "\",\n";
		temp += "\t\"Username\": \"" + entry->Username + "\",\n";
		temp += "\t\"Password\": \"" + entry->Password + "\",\n";
		temp += "\t\"Date-Created\": \"" + entry->DateCreated + "\",\n";
		temp += "\t\"Date-Updated\": \"" + entry->DateUpdated + "\",\n";
		temp += "\t\"Description\": \"" + entry->Description + "\"\n";
		temp += "}";
		return temp;
	}
	return "";
}

std::vector<PMInfo*> PMDatabase::getAllEntries()
{
	std::vector<PMInfo*> list;
	for(std::pair<std::string, PMInfo*> c : entries)
	{
		if(c.second != nullptr)
			list.push_back(c.second);
	}
	return list;
}

std::string PMDatabase::getAllEntriesAsJSON()
{
	std::string output = "{\n\t\"Entries\": [\n";
	bool addedStuff = false;
	for(std::pair<std::string, PMInfo*> c : entries)
	{
		if(c.second != nullptr)
		{
			output += convertEntryToJSON(c.second) + ",\n";
			addedStuff = true;
		}
	}
	if(addedStuff)
	{
		//remove last 2 characters. Specifically the ",\n" part.
		output.pop_back();
		output.pop_back();
	}
	output += "\t]\n}";
	return output;
}

bool PMDatabase::addEntry(const PMInfo& info)
{
	if(info.Name.empty())
		return false;
	
	PMInfo* potentialExistingEntry = getEntry(info.Name);
	if(potentialExistingEntry != nullptr)
	{
		std::string oldDateCreated = potentialExistingEntry->DateCreated;

		potentialExistingEntry->copy(info);
		potentialExistingEntry->DateCreated = oldDateCreated;
		potentialExistingEntry->DateUpdated = getCurrentDate();
	}
	else
	{
		PMInfo* newInfo = new PMInfo();
		newInfo->copy(info);
		
		newInfo->DateCreated = getCurrentDate();
		newInfo->DateUpdated = newInfo->DateCreated;

		entries[info.Name] = newInfo;
	}
	return true;
}

bool PMDatabase::deleteEntry(std::string s)
{
	return entries.erase(s) == 1;
}

bool PMDatabase::entryExist(std::string s)
{
	auto it = entries.find(s);
	return it != entries.end();
}

bool PMDatabase::save(smpl::File outputFile, std::string password)
{
	std::string resultStr = "";
	resultStr += "{\n";
	resultStr += "\t\"Entries\": [\n";
	
	bool next = false;
	for(std::pair<std::string, PMInfo*> c : entries)
	{
		if(next)
			resultStr += ",\n";

		if(c.second != nullptr)
		{
			resultStr += "\t\t{\n";

			resultStr += "\t\t\t\"Name\": \"" + c.second->Name + "\",\n";
			resultStr += "\t\t\t\"Username\": \"" + c.second->Username + "\",\n";
			resultStr += "\t\t\t\"Password\": \"" + c.second->Password + "\",\n";
			resultStr += "\t\t\t\"Date-Created\": \"" + c.second->DateCreated + "\",\n";
			resultStr += "\t\t\t\"Date-Updated\": \"" + c.second->DateUpdated + "\",\n";
			resultStr += "\t\t\t\"Description\": \"" + c.second->Description + "\"\n";

			resultStr += "\t\t}";
		}
		next = true;
	}

	resultStr += "\n";
	resultStr += "\t]\n";
	resultStr += "}";
	
	
	std::vector<unsigned char> outputData = EncryptWrapper::encrypt((unsigned char*)resultStr.data(), resultStr.size(), (unsigned char*)password.data(), password.size());
	if(outputData.size() == 0)
		return false;
	
	smpl::SimpleFile f = smpl::SimpleFile(outputFile, smpl::SimpleFile::WRITE);
	if(f.isOpen())
	{
		f.writeBytes(outputData.data(), outputData.size());
		f.close();
	}
	else
		return false;

	return true;
}

bool PMDatabase::load(smpl::File inputFile, std::string password)
{
	clear();
	smpl::SimpleFile encryptedFile = smpl::SimpleFile(inputFile, smpl::SimpleFile::READ);
	if(encryptedFile.isOpen())
	{
		std::vector<unsigned char> allBytes = encryptedFile.readFullFileAsBytes();
		encryptedFile.close();
		std::vector<unsigned char> originalData = EncryptWrapper::decrypt(allBytes.data(), allBytes.size(), (unsigned char*)password.data(), password.size());

		if(originalData.empty())
			return false;
		
		smpl::SimpleJSON json = smpl::SimpleJSON();
		json.load(originalData.data(), originalData.size());
		loadAllEntries(json);

		return true;
	}

	return false;
}

void PMDatabase::loadAllEntries(smpl::SimpleJSON& jsonData)
{
	std::vector<smpl::JNode*> nodesFound = jsonData.getNodesPattern({"Entries"}, 0);
	if(nodesFound.size() == 1)
	{
		if(nodesFound.front()->getType() == smpl::SimpleJSON::TYPE_ARRAY)
		{
			std::vector<smpl::JNode*> allChildNodes = ((smpl::JArray*)nodesFound.front())->getNodes();
			for(smpl::JNode* childNode : allChildNodes)
			{
				if(childNode->getType() == smpl::SimpleJSON::TYPE_OBJECT)
				{
					loadIndividualEntry((smpl::JObject*)childNode);
				}
			}
		}
	}
}

void PMDatabase::loadIndividualEntry(smpl::JObject* entryObject)
{
	PMInfo tempInfo;
	if(!loadPairFromEntry(entryObject, "Name", tempInfo.Name))
		return;
	if(!loadPairFromEntry(entryObject, "Username", tempInfo.Username))
		return;
	if(!loadPairFromEntry(entryObject, "Password", tempInfo.Password))
		return;
	if(!loadPairFromEntry(entryObject, "Date-Created", tempInfo.DateCreated))
		return;
	if(!loadPairFromEntry(entryObject, "Date-Updated", tempInfo.DateUpdated))
		return;
	if(!loadPairFromEntry(entryObject, "Description", tempInfo.Description))
		return;
	if(!loadPairFromEntry(entryObject, "Username", tempInfo.Username))
		return;
	
	addEntry(tempInfo);
}

bool PMDatabase::loadPairFromEntry(smpl::JObject* entryObject, std::string pairName, std::string& outputValue)
{
	auto pair = entryObject->getNodesPattern({pairName});
	if(!pair.empty())
	{
		if(pair.front()->getType() == smpl::SimpleJSON::TYPE_PAIR)
			outputValue = ((smpl::JPair*)pair.front())->getValue();
		else
			return false;
	}
	else
		return false;
	return true;
}

void PMDatabase::clear()
{
	for(std::pair<std::string, PMInfo*> c : entries)
	{
		if(c.second != nullptr)
			delete c.second;
	}
	entries.clear();
}