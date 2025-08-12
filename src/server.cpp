#include <HttpServer.h>
#include <StringTools.h>
#include <System.h>
#include <PCG.h>
#include <Concurrency.h>
#include <SimpleDir.h>
#include <IniFile.h>
#include <atomic>
#include "PMInfo.h"
using namespace smpl;

volatile std::atomic_bool running = {true};
HybridSpinLock mutex;
const int TOKEN_EXPIRE_TIME = 300000; //5 minutes of inactivity or 300 seconds
bool isSecure = false;
std::string secureOption = "";

/**
 *      APIs:
 *          GET:
 *              ListOfEntries
 *          POST:
 * 				ChangePassword
 * 				CreateAccount
 * 				Login
 *              AddEntry
 *              DeleteEntry
 * 				ImportEntries
 */

/*
	SeparateFileContaining access hash: "Base64Hash"
	Password provided + info from server gives final output (double encrypt. Server side decrypt -> user side decrypt)
    File format:
        {
            "Entries": [
                {
                    "Name": "INSERT NAME",
                    "Username": "Encrypted USERNAME",
                    "Password": "Encrypted PASSWORD",
                    "Date-Created": "TIME",
                    "Date-Updated": "UpdateTime",
                    "Description": "INSERT DESCRIPTION"
                },
                {
                    "Name": "INSERT NAME",
                    "Username": "Encrypted USERNAME",
                    "Password": "Encrypted PASSWORD",
                    "Date-Created": "TIME",
                    "Date-Updated": "UpdateTime",
                    "Description": "INSERT DESCRIPTION"
                }
            ]
        }

*/

struct UserInfo
{
	PMDatabase* userDatabase = nullptr;
	size_t expireTime;
	std::string username;
	std::string pass;
};

std::unordered_map<std::string, UserInfo> accessTokens;

PCG pcg = PCG(time(nullptr));

std::string vectorToString(const std::vector<unsigned char>& arr)
{
	std::string output;
	for(unsigned char c : arr)
		output += (char)c;
	return output;
}

std::string generateAccessToken()
{
	std::string token = "";
	for(int i=0; i<64; i++)
	{
		token += 'A' + (pcg.get() % (26));
	}
	return token;
}

bool extractJSONPairInfo(std::vector<JNode*> possibleNode, std::string& output)
{
	if(possibleNode.size() != 0)
	{
		if(possibleNode.front()->getType() == SimpleJSON::TYPE_PAIR)
		{
			output = ((JPair*)possibleNode.front())->getValue();
			return true;
		}
	}
	return false;
}

void dumpSaveInfo(UserInfo userToSave)
{
	try
	{
		File outputFile = "user_info/"+userToSave.username + "_PMDatabase.json";
		userToSave.userDatabase->save(outputFile, userToSave.pass);
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
}

int attemptToCreateAccount(std::string user, std::string pass)
{
	if(user.empty())
		return -1;
	if(pass.empty())
		return -2;
	
	File desiredOutputFile = "user_info/"+user+"_PMDatabase.json";
	if(desiredOutputFile.doesExist())
		return -3; //user already exist
	
	PMDatabase temp = PMDatabase();
	bool noErrorOnSave = temp.save(desiredOutputFile, EncryptWrapper::hash((unsigned char*)pass.data(), pass.size()));
	if(!noErrorOnSave)
		return -4; //something went wrong when saving a temporary database
	
	return 0;
}

void updateTokenExpiration(std::string s)
{
	mutex.lock();
	//update the token expiration date
	auto it = accessTokens.find(s);
	if(it != accessTokens.end())
	{
		if(System::getCurrentTimeMillis() < it->second.expireTime)
			it->second.expireTime = System::getCurrentTimeMillis() + TOKEN_EXPIRE_TIME;
	}
	mutex.unlock();
}

void quickLogout(std::string s)
{	
	mutex.lock();
	accessTokens[s].expireTime = 0; //Set it to the lowest value allowed. Will always be expired
	mutex.unlock();
}

void forceLogout(std::string username)
{
	std::unordered_map<std::string, UserInfo> newMap;
	for(auto& tokenPair : accessTokens)
	{
		if(tokenPair.second.username == username)
		{
			if(tokenPair.second.userDatabase != nullptr)
			{
				delete tokenPair.second.userDatabase;
				tokenPair.second.userDatabase = nullptr;
			}
		}
		else
		{
			newMap.insert(tokenPair);
		}

		if(System::getCurrentTimeMillis() < tokenPair.second.expireTime)
			newMap.insert(tokenPair);
	}
	accessTokens = newMap;
}

void changePasswordAndForceLogout(std::string token, std::string newHashedPass)
{
	mutex.lock();
	accessTokens[token].pass = newHashedPass; //need to modify the original and not a copy
	dumpSaveInfo(accessTokens[token]);
	forceLogout(accessTokens[token].username);
	mutex.unlock();
}

void forceDeleteAccount(UserInfo info)
{
	std::unordered_map<std::string, UserInfo> newMap;
	mutex.lock();
	SimpleDir::deleteResource("user_info/" + info.username + "_PMDatabase.json");
	for(auto& tokenPair : accessTokens)
	{
		if(tokenPair.second.username == info.username)
		{
			if(tokenPair.second.userDatabase != nullptr)
			{
				delete tokenPair.second.userDatabase;
				tokenPair.second.userDatabase = nullptr;
			}
		}
		else
		{
			newMap.insert(tokenPair);
		}

		if(System::getCurrentTimeMillis() < tokenPair.second.expireTime)
			newMap.insert(tokenPair);
	}
	accessTokens = newMap;
	mutex.unlock();
}

UserInfo checkIfAuthenticated(WebRequest& request)
{
	UserInfo temp;
	std::pair<std::string, std::string> token = request.getCookie("Access-Token");
	if(token.second.empty())
		return {};
	
	mutex.lock();
	auto it = accessTokens.find(token.second);
	if(it != accessTokens.end())
	{
		if(System::getCurrentTimeMillis() < it->second.expireTime)
		{
			temp = it->second;
		}
	}
	mutex.unlock();
	
	updateTokenExpiration(token.second);
	return temp;
}

void cleanup()
{
	//remove tokens that have been expired
	std::unordered_map<std::string, UserInfo> newMap;
	mutex.lock();
	for(auto& tokenPair : accessTokens)
	{
		if(System::getCurrentTimeMillis() < tokenPair.second.expireTime)
			newMap.insert(tokenPair);
		else
		{
			if(tokenPair.second.userDatabase != nullptr)
			{
				delete tokenPair.second.userDatabase;
				tokenPair.second.userDatabase = nullptr;
			}
		}
	}
	accessTokens = newMap;
	mutex.unlock();
}

bool send401Error(HttpServer* server, WebRequest& req, size_t id)
{
    WebRequest resp;
    resp.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 401 Unauthorized", false);
    server->fillGetResponseHeaders(resp);

    int err = server->getNetworkConnection()->sendMessage(resp, id);
    return (err == resp.getBytesInRequest());
}

void responseHandler(HttpServer* server, WebRequest& request, size_t requesterID, WebRequest& response)
{
    //send back the required Access cookie and update its expiration time on both the server and user
	std::pair<std::string, std::string> token = request.getCookie("Access-Token");
	UserInfo requesterInfo = checkIfAuthenticated(request);
	if(requesterInfo.userDatabase != nullptr)
	{
		//send cookie back to user
		response.addKeyValue("Set-Cookie", "Access-Token=" + token.second + "; HttpOnly; " + secureOption); //TODO: remember to add Secure; Must be HTTPS for it to work
	}
}

bool postRequestHandler(HttpServer* server, WebRequest& request, std::vector<unsigned char>& bodyData, size_t requesterID)
{
    std::string s = StringTools::toLowercase(request.getUrl());
	UserInfo requesterInfo = checkIfAuthenticated(request);
	std::string token = request.getCookie("Access-Token").second;

	if(s == "/api/login")
	{
		//get username and password in body data as json. Check with existing hash and if correct, allow the data to be sent over
		//assume the password received is actually just the hash of that password
		//assume that the only thing in the body data is the password
		std::string token;

		WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
		std::string body = "{\"status\": \"Failed\"}";

		std::string userStr, passStr;
		SimpleJSON json = SimpleJSON();
		bool loadWithoutError = json.load(bodyData.data(), bodyData.size());
		if(loadWithoutError)
		{
			auto userPair = json.getNodesPattern({"Username"});
			auto passPair = json.getNodesPattern({"Password"});
			
			//if an error occured, userStr or passStr are empty
			extractJSONPairInfo(userPair, userStr);
			extractJSONPairInfo(passPair, passStr);
			
			if(userStr.empty() || passStr.empty())
			{
				//send malformed request
				body = "{\"status\": \"Failed. No username or password provided.\"}";
			}
			else
			{
				File desiredOutputFile = "user_info/"+userStr+"_PMDatabase.json";
				if(!desiredOutputFile.doesExist())
				{
					//no account under that name. Send error back (DO NOT SPECIFY WHETHER ACCOUNT OR PASSWORD WAS CORRECT OR NOT)
					body = "{\"status\": \"Failed. Username or password is incorrect.\"}";
				}
				else
				{
					passStr = EncryptWrapper::hash((unsigned char*)passStr.data(), passStr.size());

					//found an existing account. Attempt to unencrypt the data
					PMDatabase* testDatabase = new PMDatabase();
					bool validPassword = testDatabase->load(desiredOutputFile, passStr);
					
					if(validPassword)
					{
						//generate access token and send that as a cookie
						token = generateAccessToken();
						response.addKeyValue("Set-Cookie", "Access-Token=" + token + "; HttpOnly; " + secureOption); //TODO: remember to add Secure; Must be HTTPS for it to work
						
						body = "{\"status\": \"Success\"}";

						UserInfo newInfo;
						newInfo.username = userStr;
						newInfo.pass = passStr;
						newInfo.userDatabase = testDatabase;
						newInfo.expireTime = System::getCurrentTimeMillis() + TOKEN_EXPIRE_TIME;

						mutex.lock();
						accessTokens.insert({token, newInfo});
						mutex.unlock();
					}
					else
					{
						delete testDatabase;

						//no account under that name. Send error back (DO NOT SPECIFY WHETHER ACCOUNT OR PASSWORD WAS CORRECT OR NOT)
						body = "{\"status\": \"Failed. Username or password is incorrect.\"}";
					}
				}
			}
		}

		
		//send cookie back to user
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
	}
	else if(s == "/api/create_account")
	{
		//body data is a json of username and password desired
		
		WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
		std::string body = "{\"status\": \"Failed\"}";

		SimpleJSON inputJSON = SimpleJSON();
		bool successfulLoad = inputJSON.load(bodyData.data(), bodyData.size());
		int createErr = -4;
		if(successfulLoad)
		{
			auto usernameData = inputJSON.getNodesPattern({"Username"});
			auto passwordData = inputJSON.getNodesPattern({"Password"});
			std::string userStr, passStr;
			extractJSONPairInfo(usernameData, userStr);
			extractJSONPairInfo(passwordData, passStr);
			createErr = attemptToCreateAccount(userStr, passStr);
			if(createErr == 0)
				body = "{\"status\": \"Success\"}";
			else if(createErr == -1)
				body = "{\"status\": \"Failed. Username is empty or invalid or the user already exist.\"}";
			else if(createErr == -2)
				body = "{\"status\": \"Failed. Password is empty or invalid.\"}";
			else if(createErr == -3)
				body = "{\"status\": \"Failed. Username is empty or invalid or the user already exist.\"}";
		}

		//send response
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
	}
	else if(s == "/api/logout")
	{
		if(requesterInfo.userDatabase == nullptr)
		{
        	return send401Error(server, request, requesterID);
		}
		quickLogout(token);

		WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
		std::string body = "{\"status\": \"Success\"}";
		//send response
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
	}
	else if(s == "/api/delete_account")
	{
		if(requesterInfo.userDatabase == nullptr)
		{
        	return send401Error(server, request, requesterID);
		}
		forceDeleteAccount(requesterInfo);

		WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
		std::string body = "{\"status\": \"Success\"}";
		//send response
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
	}
	else if(s == "/api/add_entry")
	{
		if(requesterInfo.userDatabase == nullptr)
		{
        	return send401Error(server, request, requesterID);
		}

		//body should be the json with all of the necessary stuff. No date created. No date updated.
		WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
		std::string body = "{\"status\": \"Failed\"}";

		SimpleJSON inputJSON = SimpleJSON();
		bool successfulLoad = inputJSON.load(bodyData.data(), bodyData.size());
		if(successfulLoad)
		{
			PMInfo newPMInfo;
			auto nameDataVector = inputJSON.getNodesPattern({"Name"});
			auto userDataVector = inputJSON.getNodesPattern({"Username"});
			auto passDataVector = inputJSON.getNodesPattern({"Password"});
			auto descDataVector = inputJSON.getNodesPattern({"Description"});

			std::string nameStr, userStr, passStr, descStr;
			extractJSONPairInfo(nameDataVector, nameStr);
			extractJSONPairInfo(userDataVector, userStr);
			extractJSONPairInfo(passDataVector, passStr);
			extractJSONPairInfo(descDataVector, descStr);

			mutex.lock();
			if(!nameStr.empty())
			{
				PMInfo* pmInfoPointer = requesterInfo.userDatabase->getEntry(nameStr);
				if(pmInfoPointer != nullptr)
					newPMInfo.copy(*pmInfoPointer);
			}

			newPMInfo.Name = nameStr;
			newPMInfo.Username = userStr;
			newPMInfo.Password = passStr;
			newPMInfo.Description = descStr;

			bool success = requesterInfo.userDatabase->addEntry(newPMInfo);
			dumpSaveInfo(requesterInfo);
			mutex.unlock();

			if(success)
				body = "{\"status\": \"Success\"}";
		}

		//send response
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
	}
	else if(s == "/api/delete_entry")
	{
		if(requesterInfo.userDatabase == nullptr)
		{
        	return send401Error(server, request, requesterID);
		}
		
		WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
		std::string body = "{\"status\": \"Failed\"}";

		SimpleJSON inputJSON = SimpleJSON();
		bool successfulLoad = inputJSON.load(bodyData.data(), bodyData.size());
		if(successfulLoad)
		{
			std::string nameStr;
			auto nameDataVector = inputJSON.getNodesPattern({"Name"});
			extractJSONPairInfo(nameDataVector, nameStr);
			
			mutex.lock();
			bool success = requesterInfo.userDatabase->deleteEntry(nameStr);
			dumpSaveInfo(requesterInfo);
			mutex.unlock();

			if(success)
				body = "{\"status\": \"Success\"}";
		}

		//send response
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
	}
	else if(s == "/api/import_entries")
	{
		if(requesterInfo.userDatabase == nullptr)
		{
        	return send401Error(server, request, requesterID);
		}

		//A json of entries that are being imported. Just a bunch of adds
		WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
		std::string body = "{\"status\": \"Failed\"}";

		SimpleJSON inputJSON = SimpleJSON();
		bool successfulLoad = inputJSON.load(bodyData.data(), bodyData.size());
		if(successfulLoad)
		{
			JArray* entryArr = nullptr;
			auto allEntries = inputJSON.getNodesPattern({"Entries"});
			if(allEntries.size() > 0)
			{
				if(allEntries.front()->getType() == SimpleJSON::TYPE_ARRAY)
				{
					entryArr = (JArray*)allEntries.front();
				}
			}

			if(entryArr != nullptr)
			{
				int count = 0;
				mutex.lock();
				for(JNode* jObj : entryArr->getNodes())
				{
					PMInfo newPMInfo;
					auto nameDataVector = jObj->getNodesPattern({"Name"});
					auto userDataVector = jObj->getNodesPattern({"Username"});
					auto passDataVector = jObj->getNodesPattern({"Password"});
					auto descDataVector = jObj->getNodesPattern({"Description"});

					std::string nameStr, userStr, passStr, descStr;
					extractJSONPairInfo(nameDataVector, nameStr);
					extractJSONPairInfo(userDataVector, userStr);
					extractJSONPairInfo(passDataVector, passStr);
					extractJSONPairInfo(descDataVector, descStr);

					if(!nameStr.empty())
					{
						PMInfo* pmInfoPointer = requesterInfo.userDatabase->getEntry(nameStr);
						if(pmInfoPointer != nullptr)
							newPMInfo.copy(*pmInfoPointer);
					}

					newPMInfo.Name = nameStr;
					newPMInfo.Username = userStr;
					newPMInfo.Password = passStr;
					newPMInfo.Description = descStr;

					if(requesterInfo.userDatabase->addEntry(newPMInfo))
						count++;
				}

				dumpSaveInfo(requesterInfo);
				mutex.unlock();
				body = "{\"status\": \"Success - " + StringTools::toString(count) + "\"}";
			}
		}

		//send response
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
	}
	else if(s == "/api/change_password")
	{
		if(requesterInfo.userDatabase == nullptr)
		{
        	return send401Error(server, request, requesterID);
		}
		
		WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
		std::string body = "{\"status\": \"Failed\"}";

		SimpleJSON inputJSON = SimpleJSON();
		bool successfulLoad = inputJSON.load(bodyData.data(), bodyData.size());
		if(successfulLoad)
		{
			std::string oldPassStr, newPassStr;
			auto oldPasswordData = inputJSON.getNodesPattern({"OldPassword"});
			auto newPasswordData = inputJSON.getNodesPattern({"NewPassword"});
			extractJSONPairInfo(oldPasswordData, oldPassStr);
			extractJSONPairInfo(newPasswordData, newPassStr);

			if(newPassStr.empty() || oldPassStr.empty())
			{
				body = "{\"status\": \"Failed. New or Old password cannot be empty.\"}";
			}
			else
			{
				bool success = false;
				std::string originalHashedPass = EncryptWrapper::hash((unsigned char*)oldPassStr.data(), oldPassStr.size());
				if(requesterInfo.pass == originalHashedPass)
				{
					std::string newHashedPass = EncryptWrapper::hash((unsigned char*)newPassStr.data(), newPassStr.size());
					changePasswordAndForceLogout(token, newHashedPass);
					success = true;
				}

				if(success)
					body = "{\"status\": \"Success\"}";
				else
					body = "{\"status\": \"Failed. Old Password is incorrect.\"}"; //this time its fine as you must be logged in to even access this api. Either session hijack or the user forgot.
			}
		}

		//send response
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
	}
    return false;
}

bool getRequestHandler(HttpServer* server, WebRequest& request, std::vector<unsigned char>& bodyData, size_t requesterID)
{
    std::string s = StringTools::toLowercase(request.getUrl());
	UserInfo requesterInfo = checkIfAuthenticated(request);

    if(s == "/api/list_all_entries")
    {
		if(requesterInfo.userDatabase == nullptr)
		{
        	return send401Error(server, request, requesterID);
		}

        //return as json - test data
		mutex.lock();
        std::string body = requesterInfo.userDatabase->getAllEntriesAsJSON();
		mutex.unlock();

        WebRequest response = WebRequest();
        response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
        server->fillGetResponseHeaders(response);
        response.addKeyValue("Content-Length", StringTools::toString(body.size()));
        response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
        bool err = server->sendResponse(response, requesterID, request) <= 0;
        if(!err)
            err = server->sendMessage(body, requesterID) <= 0;
        return err;
    }
	else if(s == "/api/get_user_info")
	{
        std::string body = "";
		
		if(requesterInfo.userDatabase == nullptr)
		{
        	return send401Error(server, request, requesterID);
		}
		body = "{\"Username\": \"" + requesterInfo.username + "\"}";
		
		WebRequest response = WebRequest();
		response.setHeader(WebRequest::TYPE_SERVER, "HTTP/1.1 200 OK", false);
		server->fillGetResponseHeaders(response);
		response.addKeyValue("Content-Length", StringTools::toString(body.size()));
		response.addKeyValue("Content-Type", WebRequest::getMimeTypeFromExt(".json"));
		bool err = server->sendResponse(response, requesterID, request) <= 0;
		if(!err)
			err = server->sendMessage(body, requesterID) <= 0;
		return err;
	}
    else
    {
		//prevent access to any non root folder
		File fullFilename = s;
		if(fullFilename.getPath().empty() || fullFilename.getPath() == "./" || fullFilename.getPath() == "/")
		{
        	return server->defaultGetFunction(request, requesterID);
		}
		return send401Error(server, request, requesterID);
    }
}

void closeInterrupt()
{
    running = false;
}

void readConfigFile(NetworkConfig& netConfigRef, int& threadsForServer, std::string& certFileRef, std::string& keyFileRef)
{
    netConfigRef.amountOfConnectionsAllowed = 64;
    netConfigRef.location = "0.0.0.0";
    netConfigRef.port = 44369;
    netConfigRef.TCP = true;
    netConfigRef.type = Network::TYPE_SERVER;
	netConfigRef.secure = false;

	IniFile initConfigFile = IniFile();
	if(initConfigFile.load("config.ini"))
	{
		std::string s = initConfigFile.readValue("SERVER", "Threads");
		threadsForServer = StringTools::toInt(initConfigFile.readValue("SERVER", "Threads"));
		netConfigRef.location = initConfigFile.readValue("SERVER", "Server_Location");
		netConfigRef.amountOfConnectionsAllowed = StringTools::toInt(initConfigFile.readValue("SERVER", "Connections_Allowed"));
		netConfigRef.port = StringTools::toInt(initConfigFile.readValue("SERVER", "Port"));

		certFileRef = initConfigFile.readValue("HTTPS", "Certificate");
		keyFileRef = initConfigFile.readValue("HTTPS", "Key");
		
		if(!certFileRef.empty() && !keyFileRef.empty())
			netConfigRef.secure = true;
	}
	else
	{
		SimpleFile f = SimpleFile("config.ini", SimpleFile::WRITE);
		if(f.isOpen())
		{
			f.writeString("[SERVER]\n");
			f.writeString("Server_Location = \"0.0.0.0\"\n");
			f.writeString("Connections_Allowed = 64\n");
			f.writeString("Port = 44369\n");
			f.writeString("Threads = 2\n");
			f.writeString("[HTTPS]\n");
			f.writeString("Certificate = \"\"\n");
			f.writeString("Key = \"\"\n");
			f.close();
		}
	}
}

void sanityCheck(NetworkConfig& netConfigRef, int& threadsForServer, std::string& certFileRef, std::string& keyFileRef)
{
	if(netConfigRef.amountOfConnectionsAllowed <= 0)
	{
		StringTools::println("ERROR: Network must allow at least 1 connection");
		exit(-1);
	}
	
	if(netConfigRef.port <= 0)
	{
		StringTools::println("ERROR: Port must be greater than 0");
		exit(-1);
	}
	
	if(netConfigRef.location.empty())
	{
		StringTools::println("ERROR: Server location can not be empty");
		exit(-1);
	}
	
	if((!certFileRef.empty() && keyFileRef.empty()) || (certFileRef.empty() && !keyFileRef.empty()))
	{
		StringTools::println("ERROR: Certificate or Key file is specified for HTTPS but both must be specified. Not just one.");
		exit(-1);
	}
	
	if(threadsForServer <= 0)
	{
		StringTools::println("ERROR: Worker threads for the server must be greater than 0");
		exit(-1);
	}
}

int main(int argc, char** argv)
{
	NetworkConfig config;
	int threadsForServer = 2;
	std::string certFile, keyFile;

	readConfigFile(config, threadsForServer, certFile, keyFile);
	sanityCheck(config, threadsForServer, certFile, keyFile);
	isSecure = config.secure;
	if(isSecure)
		secureOption = "Secure;";

    HttpServer server = HttpServer(config, threadsForServer, config.secure, certFile, keyFile);
    server.setAllowPersistentConnection(false);
    server.setPostFuncMapper([](HttpServer* server, WebRequest& request, std::vector<unsigned char>& bodyData, size_t requesterID) ->bool{
        return postRequestHandler(server, request, bodyData, requesterID);
    });
    server.setGetFuncMapper([](HttpServer* server, WebRequest& request, std::vector<unsigned char>& bodyData, size_t requesterID) ->bool{
        return getRequestHandler(server, request, bodyData, requesterID);
    });
	server.setResponseHandlerFuncMapper([](HttpServer* server, WebRequest& request, size_t requesterID, WebRequest& response) ->void{
		responseHandler(server, request, requesterID, response);
	});

    System::mapInteruptSignal(closeInterrupt);
    server.start();
	size_t nextCleanupTime = System::getCurrentTimeMillis() + TOKEN_EXPIRE_TIME;
	
    while(running)
    {
		System::sleep(10, 0, false);

		if(System::getCurrentTimeMillis() > nextCleanupTime)
		{
			cleanup();
			nextCleanupTime = System::getCurrentTimeMillis() + TOKEN_EXPIRE_TIME;
		}
	}
    StringTools::println("Shutting Down PM3.0 Server");
    return 0;
}