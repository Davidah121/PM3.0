#pragma once
#include <SimpleJSON.h>
#include <File.h>
#include "EncryptWrapper.h"

struct PMInfo
{
	std::string Name;
	std::string Username;
	std::string Password;
	std::string DateCreated;
	std::string DateUpdated;
	std::string Description;

	void copy(const PMInfo& other)
	{
		this->Name = other.Name;
		this->Username = other.Username;
		this->Password = other.Password;
		this->DateCreated = other.DateCreated;
		this->DateUpdated = other.DateUpdated;
		this->Description = other.Description;
	}
};

class PMDatabase
{
public:
	PMDatabase();
	~PMDatabase();

	/**
	 * @brief Gets the current date in mm-dd-yyyy hh:mm:ss format
	 * 
	 * @return std::string 
	 */
	static std::string getCurrentDate();

	/**
	 * @brief Gets an entry from the list of entries.
	 * 		That entry may contain an encrypted password and username which must be decrypted on the user side.
	 * 
	 * @param s 
	 * @return PMInfo& 
	 */
	PMInfo* getEntry(std::string s);

	/**
	 * @brief Get the Entry As JSON.
	 * 		Useful when sending the data over a network
	 * 
	 * @param s 
	 * @return std::string 
	 */
	std::string convertEntryToJSON(PMInfo* entry);

	/**
	 * @brief Gets all Entries available.
	 * 
	 * @return std::vector<PMInfo> 
	 */
	std::vector<PMInfo*> getAllEntries();
	
	/**
	 * @brief Get the All Entries As a single JSON.
	 * 
	 * @return std::string 
	 */
	std::string getAllEntriesAsJSON();

	/**
	 * @brief Adds a new entry if the entry doesn't already exist.
	 * 		If the entry does exist, it will replace it instead acting as an edit function.
	 * 
	 * @param s
	 */
	bool addEntry(const PMInfo& info);

	/**
	 * @brief Determines if the entry does actually exist.
	 * 
	 * @param s 
	 * @return true 
	 * @return false 
	 */
	bool entryExist(std::string s);

	/**
	 * @brief Attempts to delete an entry if it does exist.
	 * 
	 * @param s 
	 */
	bool deleteEntry(std::string s);

	/**
	 * @brief Saves the database to a json file.
	 * 		For backup purposes and it is required if the server were to ever need to be restarted.
	 * 		The data will be encrypted using the server's own credentials which can be set later
	 * 
	 * @param outputFile 
	 */
	bool save(smpl::File outputFile, std::string password);

	/**
	 * @brief Loads the database from an existing json file.
	 * 		The data is encrypted so the server's credentials must be correct
	 * 
	 * @param inputFile 
	 */
	bool load(smpl::File inputFile, std::string password);
private:

	void loadAllEntries(smpl::SimpleJSON& jsonData);
	void loadIndividualEntry(smpl::JObject* entryObject);
	bool loadPairFromEntry(smpl::JObject* entryObject, std::string pairName, std::string& outputValue);

	/**
	 * @brief Deletes all entries. Internal use only
	 * 
	 */
	void clear();
	std::string accessHash; //base64
	std::unordered_map<std::string, PMInfo*> entries;
};